# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Skipping shell invocations is good, when possible. This wrapper around subprocess does dirty work of
parsing command lines into argv and making sure that no shell magic is being used.
"""

#TODO: ship pyprocessing?
import multiprocessing, multiprocessing.dummy
import subprocess, shlex, re, logging, sys, traceback, os, imp, glob
# XXXkhuey Work around http://bugs.python.org/issue1731717
subprocess._cleanup = lambda: None
import command, util
if sys.platform=='win32':
    import win32process

_log = logging.getLogger('pymake.process')

_escapednewlines = re.compile(r'\\\n')
_blacklist = re.compile(r'[$><;[{~`|&()]')
_needsglob = re.compile(r'[\*\?]')
def clinetoargv(cline):
    """
    If this command line can safely skip the shell, return an argv array.
    @returns argv, badchar
    """

    str = _escapednewlines.sub('', cline)
    m = _blacklist.search(str)
    if m is not None:
        return None, m.group(0)

    args = shlex.split(str, comments=True)

    if len(args) and args[0].find('=') != -1:
        return None, '='

    return args, None

def doglobbing(args, cwd):
    """
    Perform any needed globbing on the argument list passed in
    """
    globbedargs = []
    for arg in args:
        if _needsglob.search(arg):
            globbedargs.extend(glob.glob(os.path.join(cwd, arg)))
        else:
            globbedargs.append(arg)

    return globbedargs

shellwords = (':', '.', 'break', 'cd', 'continue', 'exec', 'exit', 'export',
              'getopts', 'hash', 'pwd', 'readonly', 'return', 'shift', 
              'test', 'times', 'trap', 'umask', 'unset', 'alias',
              'set', 'bind', 'builtin', 'caller', 'command', 'declare',
              'echo', 'enable', 'help', 'let', 'local', 'logout', 
              'printf', 'read', 'shopt', 'source', 'type', 'typeset',
              'ulimit', 'unalias', 'set')

def call(cline, env, cwd, loc, cb, context, echo, justprint=False):
    #TODO: call this once up-front somewhere and save the result?
    shell, msys = util.checkmsyscompat()

    shellreason = None
    if msys and cline.startswith('/'):
        shellreason = "command starts with /"
    else:
        argv, badchar = clinetoargv(cline)
        if argv is None:
            shellreason = "command contains shell-special character '%s'" % (badchar,)
        elif len(argv) and argv[0] in shellwords:
            shellreason = "command starts with shell primitive '%s'" % (argv[0],)
        else:
            argv = doglobbing(argv, cwd)

    if shellreason is not None:
        _log.debug("%s: using shell: %s: '%s'", loc, shellreason, cline)
        if msys:
            if len(cline) > 3 and cline[1] == ':' and cline[2] == '/':
                cline = '/' + cline[0] + cline[2:]
            cline = [shell, "-c", cline]
        context.call(cline, shell=not msys, env=env, cwd=cwd, cb=cb, echo=echo,
                     justprint=justprint)
        return

    if not len(argv):
        cb(res=0)
        return

    if argv[0] == command.makepypath:
        command.main(argv[1:], env, cwd, cb)
        return

    if argv[0:2] == [sys.executable.replace('\\', '/'),
                     command.makepypath.replace('\\', '/')]:
        command.main(argv[2:], env, cwd, cb)
        return

    if argv[0].find('/') != -1:
        executable = util.normaljoin(cwd, argv[0])
    else:
        executable = None

    context.call(argv, executable=executable, shell=False, env=env, cwd=cwd, cb=cb,
                 echo=echo, justprint=justprint)

def call_native(module, method, argv, env, cwd, loc, cb, context, echo, justprint=False,
                pycommandpath=None):
    argv = doglobbing(argv, cwd)
    context.call_native(module, method, argv, env=env, cwd=cwd, cb=cb,
                        echo=echo, justprint=justprint, pycommandpath=pycommandpath)

def statustoresult(status):
    """
    Convert the status returned from waitpid into a prettier numeric result.
    """
    sig = status & 0xFF
    if sig:
        return -sig

    return status >>8

class Job(object):
    """
    A single job to be executed on the process pool.
    """
    done = False # set to true when the job completes

    def __init__(self):
        self.exitcode = -127

    def notify(self, condition, result):
        condition.acquire()
        self.done = True
        self.exitcode = result
        condition.notify()
        condition.release()

    def get_callback(self, condition):
        return lambda result: self.notify(condition, result)

class PopenJob(Job):
    """
    A job that executes a command using subprocess.Popen.
    """
    def __init__(self, argv, executable, shell, env, cwd):
        Job.__init__(self)
        self.argv = argv
        self.executable = executable
        self.shell = shell
        self.env = env
        self.cwd = cwd

    def run(self):
        try:
            p = subprocess.Popen(self.argv, executable=self.executable, shell=self.shell, env=self.env, cwd=self.cwd)
            return p.wait()
        except OSError, e:
            print >>sys.stderr, e
            return -127

class PythonException(Exception):
    def __init__(self, message, exitcode):
        Exception.__init__(self)
        self.message = message
        self.exitcode = exitcode

    def __str__(self):
        return self.message

def load_module_recursive(module, path):
    """
    Emulate the behavior of __import__, but allow
    passing a custom path to search for modules.
    """
    bits = module.split('.')
    for i, bit in enumerate(bits):
        dotname = '.'.join(bits[:i+1])
        try:
          f, path, desc = imp.find_module(bit, path)
          m = imp.load_module(dotname, f, path, desc)
          if f is None:
              path = m.__path__
        except ImportError:
            return

class PythonJob(Job):
    """
    A job that calls a Python method.
    """
    def __init__(self, module, method, argv, env, cwd, pycommandpath=None):
        self.module = module
        self.method = method
        self.argv = argv
        self.env = env
        self.cwd = cwd
        self.pycommandpath = pycommandpath or []

    def run(self):
        oldenv = os.environ
        try:
            os.chdir(self.cwd)
            os.environ = self.env
            if self.module not in sys.modules:
                load_module_recursive(self.module,
                                      sys.path + self.pycommandpath)
            if self.module not in sys.modules:
                print >>sys.stderr, "No module named '%s'" % self.module
                return -127                
            m = sys.modules[self.module]
            if self.method not in m.__dict__:
                print >>sys.stderr, "No method named '%s' in module %s" % (method, module)
                return -127
            m.__dict__[self.method](self.argv)
        except PythonException, e:
            print >>sys.stderr, e
            return e.exitcode
        except:
            e = sys.exc_info()[1]
            if isinstance(e, SystemExit) and (e.code == 0 or e.code == '0'):
                pass # sys.exit(0) is not a failure
            else:
                print >>sys.stderr, e
                print >>sys.stderr, traceback.print_exc()
                return -127
        finally:
            os.environ = oldenv
        return 0

def job_runner(job):
    """
    Run a job. Called in a Process pool.
    """
    return job.run()

class ParallelContext(object):
    """
    Manages the parallel execution of processes.
    """

    _allcontexts = set()
    _condition = multiprocessing.Condition()

    def __init__(self, jcount):
        self.jcount = jcount
        self.exit = False

        self.processpool = multiprocessing.Pool(processes=jcount)
        self.threadpool = multiprocessing.dummy.Pool(processes=jcount)
        self.pending = [] # list of (cb, args, kwargs)
        self.running = [] # list of (subprocess, cb)

        self._allcontexts.add(self)

    def finish(self):
        assert len(self.pending) == 0 and len(self.running) == 0, "pending: %i running: %i" % (len(self.pending), len(self.running))
        self.processpool.close()
        self.threadpool.close()
        self.processpool.join()
        self.threadpool.join()
        self._allcontexts.remove(self)

    def run(self):
        while len(self.pending) and len(self.running) < self.jcount:
            cb, args, kwargs = self.pending.pop(0)
            cb(*args, **kwargs)

    def defer(self, cb, *args, **kwargs):
        assert self.jcount > 1 or not len(self.pending), "Serial execution error defering %r %r %r: currently pending %r" % (cb, args, kwargs, self.pending)
        self.pending.append((cb, args, kwargs))

    def _docall_generic(self, pool, job, cb, echo, justprint):
        if echo is not None:
            print echo
        processcb = job.get_callback(ParallelContext._condition)
        if justprint:
            processcb(0)
        else:
            pool.apply_async(job_runner, args=(job,), callback=processcb)
        self.running.append((job, cb))

    def call(self, argv, shell, env, cwd, cb, echo, justprint=False, executable=None):
        """
        Asynchronously call the process
        """

        job = PopenJob(argv, executable=executable, shell=shell, env=env, cwd=cwd)
        self.defer(self._docall_generic, self.threadpool, job, cb, echo, justprint)

    def call_native(self, module, method, argv, env, cwd, cb,
                    echo, justprint=False, pycommandpath=None):
        """
        Asynchronously call the native function
        """

        job = PythonJob(module, method, argv, env, cwd, pycommandpath)
        self.defer(self._docall_generic, self.processpool, job, cb, echo, justprint)

    @staticmethod
    def _waitany(condition):
        def _checkdone():
            jobs = []
            for c in ParallelContext._allcontexts:
                for i in xrange(0, len(c.running)):
                    if c.running[i][0].done:
                        jobs.append(c.running[i])
                for j in jobs:
                    if j in c.running:
                        c.running.remove(j)
            return jobs

        # We must acquire the lock, and then check to see if any jobs have
        # finished.  If we don't check after acquiring the lock it's possible
        # that all outstanding jobs will have completed before we wait and we'll
        # wait for notifications that have already occurred.
        condition.acquire()
        jobs = _checkdone()

        if jobs == []:
            condition.wait()
            jobs = _checkdone()

        condition.release()

        return jobs
        
    @staticmethod
    def spin():
        """
        Spin the 'event loop', and never return.
        """

        while True:
            clist = list(ParallelContext._allcontexts)
            for c in clist:
                c.run()

            dowait = util.any((len(c.running) for c in ParallelContext._allcontexts))
            if dowait:
                # Wait on local jobs first for perf
                for job, cb in ParallelContext._waitany(ParallelContext._condition):
                    cb(job.exitcode)
            else:
                assert any(len(c.pending) for c in ParallelContext._allcontexts)

def makedeferrable(usercb, **userkwargs):
    def cb(*args, **kwargs):
        kwargs.update(userkwargs)
        return usercb(*args, **kwargs)

    return cb

_serialContext = None
_parallelContext = None

def getcontext(jcount):
    global _serialContext, _parallelContext
    if jcount == 1:
        if _serialContext is None:
            _serialContext = ParallelContext(1)
        return _serialContext
    else:
        if _parallelContext is None:
            _parallelContext = ParallelContext(jcount)
        return _parallelContext

