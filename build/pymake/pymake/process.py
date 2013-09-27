"""
Skipping shell invocations is good, when possible. This wrapper around subprocess does dirty work of
parsing command lines into argv and making sure that no shell magic is being used.
"""

#TODO: ship pyprocessing?
import multiprocessing
import subprocess, shlex, re, logging, sys, traceback, os, imp, glob
import site
from collections import deque
# XXXkhuey Work around http://bugs.python.org/issue1731717
subprocess._cleanup = lambda: None
import command, util
if sys.platform=='win32':
    import win32process

_log = logging.getLogger('pymake.process')

_escapednewlines = re.compile(r'\\\n')

def tokens2re(tokens):
    # Create a pattern for non-escaped tokens, in the form:
    #   (?<!\\)(?:a|b|c...)
    # This is meant to match patterns a, b, or c, or ... if they are not
    # preceded by a backslash.
    # where a, b, c... are in the form
    #   (?P<name>pattern)
    # which matches the pattern and captures it in a named match group.
    # The group names and patterns come are given as a dict in the function
    # argument.
    nonescaped = r'(?<!\\)(?:%s)' % '|'.join('(?P<%s>%s)' % (name, value) for name, value in tokens.iteritems())
    # The final pattern matches either the above pattern, or an escaped
    # backslash, captured in the "escape" match group.
    return re.compile('(?:%s|%s)' % (nonescaped, r'(?P<escape>\\\\)'))

_unquoted_tokens = tokens2re({
  'whitespace': r'[\t\r\n ]+',
  'quote': r'[\'"]',
  'comment': '#',
  'special': r'[<>&|`~(){}$;]',
  'backslashed': r'\\[^\\]',
  'glob': r'[\*\?]',
})

_doubly_quoted_tokens = tokens2re({
  'quote': '"',
  'backslashedquote': r'\\"',
  'special': '\$',
  'backslashed': r'\\[^\\"]',
})

class MetaCharacterException(Exception):
    def __init__(self, char):
        self.char = char

class ClineSplitter(list):
    """
    Parses a given command line string and creates a list of command
    and arguments, with wildcard expansion.
    """
    def __init__(self, cline, cwd):
        self.cwd = cwd
        self.arg = None
        self.cline = cline
        self.glob = False
        self._parse_unquoted()

    def _push(self, str):
        """
        Push the given string as part of the current argument
        """
        if self.arg is None:
            self.arg = ''
        self.arg += str

    def _next(self):
        """
        Finalize current argument, effectively adding it to the list.
        Perform globbing if needed.
        """
        if self.arg is None:
            return
        if self.glob:
            if os.path.isabs(self.arg):
                path = self.arg
            else:
                path = os.path.join(self.cwd, self.arg)
            globbed = glob.glob(path)
            if not globbed:
                # If globbing doesn't find anything, the literal string is
                # used.
                self.append(self.arg)
            else:
                self.extend(f[len(path)-len(self.arg):] for f in globbed)
            self.glob = False
        else:
            self.append(self.arg)
        self.arg = None

    def _parse_unquoted(self):
        """
        Parse command line remainder in the context of an unquoted string.
        """
        while self.cline:
            # Find the next token
            m = _unquoted_tokens.search(self.cline)
            # If we find none, the remainder of the string can be pushed to
            # the current argument and the argument finalized
            if not m:
                self._push(self.cline)
                break
            # The beginning of the string, up to the found token, is part of
            # the current argument
            if m.start():
                self._push(self.cline[:m.start()])
            self.cline = self.cline[m.end():]

            match = dict([(name, value) for name, value in m.groupdict().items() if value])
            if 'quote' in match:
                # " or ' start a quoted string
                if match['quote'] == '"':
                    self._parse_doubly_quoted()
                else:
                    self._parse_quoted()
            elif 'comment' in match:
                # Comments are ignored. The current argument can be finalized,
                # and parsing stopped.
                break
            elif 'special' in match:
                # Unquoted, non-escaped special characters need to be sent to a
                # shell.
                raise MetaCharacterException, match['special']
            elif 'whitespace' in match:
                # Whitespaces terminate current argument.
                self._next()
            elif 'escape' in match:
                # Escaped backslashes turn into a single backslash
                self._push('\\')
            elif 'backslashed' in match:
                # Backslashed characters are unbackslashed
                # e.g. echo \a -> a
                self._push(match['backslashed'][1])
            elif 'glob' in match:
                # ? or * will need globbing
                self.glob = True
                self._push(m.group(0))
            else:
                raise Exception, "Shouldn't reach here"
        if self.arg:
            self._next()

    def _parse_quoted(self):
        # Single quoted strings are preserved, except for the final quote
        index = self.cline.find("'")
        if index == -1:
            raise Exception, 'Unterminated quoted string in command'
        self._push(self.cline[:index])
        self.cline = self.cline[index+1:]

    def _parse_doubly_quoted(self):
        if not self.cline:
            raise Exception, 'Unterminated quoted string in command'
        while self.cline:
            m = _doubly_quoted_tokens.search(self.cline)
            if not m:
                raise Exception, 'Unterminated quoted string in command'
            self._push(self.cline[:m.start()])
            self.cline = self.cline[m.end():]
            match = dict([(name, value) for name, value in m.groupdict().items() if value])
            if 'quote' in match:
                # a double quote ends the quoted string, so go back to
                # unquoted parsing
                return
            elif 'special' in match:
                # Unquoted, non-escaped special characters in a doubly quoted
                # string still have a special meaning and need to be sent to a
                # shell.
                raise MetaCharacterException, match['special']
            elif 'escape' in match:
                # Escaped backslashes turn into a single backslash
                self._push('\\')
            elif 'backslashedquote' in match:
                # Backslashed double quotes are un-backslashed
                self._push('"')
            elif 'backslashed' in match:
                # Backslashed characters are kept backslashed
                self._push(match['backslashed'])

def clinetoargv(cline, cwd):
    """
    If this command line can safely skip the shell, return an argv array.
    @returns argv, badchar
    """
    str = _escapednewlines.sub('', cline)
    try:
        args = ClineSplitter(str, cwd)
    except MetaCharacterException, e:
        return None, e.char

    if len(args) and args[0].find('=') != -1:
        return None, '='

    return args, None

# shellwords contains a set of shell builtin commands that need to be
# executed within a shell. It also contains a set of commands that are known
# to be giving problems when run directly instead of through the msys shell.
shellwords = (':', '.', 'break', 'cd', 'continue', 'exec', 'exit', 'export',
              'getopts', 'hash', 'pwd', 'readonly', 'return', 'shift', 
              'test', 'times', 'trap', 'umask', 'unset', 'alias',
              'set', 'bind', 'builtin', 'caller', 'command', 'declare',
              'echo', 'enable', 'help', 'let', 'local', 'logout', 
              'printf', 'read', 'shopt', 'source', 'type', 'typeset',
              'ulimit', 'unalias', 'set', 'find')

def prepare_command(cline, cwd, loc):
    """
    Returns a list of command and arguments for the given command line string.
    If the command needs to be run through a shell for some reason, the
    returned list contains the shell invocation.
    """

    #TODO: call this once up-front somewhere and save the result?
    shell, msys = util.checkmsyscompat()

    shellreason = None
    executable = None
    if msys and cline.startswith('/'):
        shellreason = "command starts with /"
    else:
        argv, badchar = clinetoargv(cline, cwd)
        if argv is None:
            shellreason = "command contains shell-special character '%s'" % (badchar,)
        elif len(argv) and argv[0] in shellwords:
            shellreason = "command starts with shell primitive '%s'" % (argv[0],)
        elif argv and (os.sep in argv[0] or os.altsep and os.altsep in argv[0]):
            executable = util.normaljoin(cwd, argv[0])
            # Avoid "%1 is not a valid Win32 application" errors, assuming
            # that if the executable path is to be resolved with PATH, it will
            # be a Win32 executable.
            if sys.platform == 'win32' and os.path.isfile(executable) and open(executable, 'rb').read(2) == "#!":
                shellreason = "command executable starts with a hashbang"

    if shellreason is not None:
        _log.debug("%s: using shell: %s: '%s'", loc, shellreason, cline)
        if msys:
            if len(cline) > 3 and cline[1] == ':' and cline[2] == '/':
                cline = '/' + cline[0] + cline[2:]
        argv = [shell, "-c", cline]
        executable = None

    return executable, argv

def call(cline, env, cwd, loc, cb, context, echo, justprint=False):
    executable, argv = prepare_command(cline, cwd, loc)

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

    context.call(argv, executable=executable, shell=False, env=env, cwd=cwd, cb=cb,
                 echo=echo, justprint=justprint)

def call_native(module, method, argv, env, cwd, loc, cb, context, echo, justprint=False,
                pycommandpath=None):
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
        self.parentpid = os.getpid()

    def run(self):
        assert os.getpid() != self.parentpid
        # subprocess.Popen doesn't use the PATH set in the env argument for
        # finding the executable on some platforms (but strangely it does on
        # others!), so set os.environ['PATH'] explicitly. This is parallel-
        # safe because pymake uses separate processes for parallelism, and
        # each process is serial. See http://bugs.python.org/issue8557 for a
        # general overview of "subprocess PATH semantics and portability".
        oldpath = os.environ['PATH']
        try:
            if self.env is not None and self.env.has_key('PATH'):
                os.environ['PATH'] = self.env['PATH']
            p = subprocess.Popen(self.argv, executable=self.executable, shell=self.shell, env=self.env, cwd=self.cwd)
            return p.wait()
        except OSError, e:
            print >>sys.stderr, e
            return -127
        finally:
            os.environ['PATH'] = oldpath

class PythonException(Exception):
    def __init__(self, message, exitcode):
        Exception.__init__(self)
        self.message = message
        self.exitcode = exitcode

    def __str__(self):
        return self.message

def load_module_recursive(module, path):
    """
    Like __import__, but allow passing a custom path to search for modules.
    """
    oldsyspath = sys.path
    sys.path = []
    for p in path:
        site.addsitedir(p)
    sys.path.extend(oldsyspath)
    try:
        __import__(module)
    except ImportError:
        return
    finally:
        sys.path = oldsyspath

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
        self.parentpid = os.getpid()

    def run(self):
        assert os.getpid() != self.parentpid
        # os.environ is a magic dictionary. Setting it to something else
        # doesn't affect the environment of subprocesses, so use clear/update
        oldenv = dict(os.environ)
        try:
            os.chdir(self.cwd)
            os.environ.clear()
            os.environ.update(self.env)
            if self.module not in sys.modules:
                load_module_recursive(self.module,
                                      sys.path + self.pycommandpath)
            if self.module not in sys.modules:
                print >>sys.stderr, "No module named '%s'" % self.module
                return -127                
            m = sys.modules[self.module]
            if self.method not in m.__dict__:
                print >>sys.stderr, "No method named '%s' in module %s" % (self.method, self.module)
                return -127
            rv = m.__dict__[self.method](self.argv)
            if rv != 0 and rv is not None:
                print >>sys.stderr, (
                    "Native command '%s %s' returned value '%s'" %
                    (self.module, self.method, rv))
                return (rv if isinstance(rv, int) else 1)

        except PythonException, e:
            print >>sys.stderr, e
            return e.exitcode
        except:
            e = sys.exc_info()[1]
            if isinstance(e, SystemExit) and (e.code == 0 or e.code is None):
                pass # sys.exit(0) is not a failure
            else:
                print >>sys.stderr, e
                traceback.print_exc()
                return -127
        finally:
            os.environ.clear()
            os.environ.update(oldenv)
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
        self.pending = deque() # deque of (cb, args, kwargs)
        self.running = [] # list of (subprocess, cb)

        self._allcontexts.add(self)

    def finish(self):
        assert len(self.pending) == 0 and len(self.running) == 0, "pending: %i running: %i" % (len(self.pending), len(self.running))
        self.processpool.close()
        self.processpool.join()
        self._allcontexts.remove(self)

    def run(self):
        while len(self.pending) and len(self.running) < self.jcount:
            cb, args, kwargs = self.pending.popleft()
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
        self.defer(self._docall_generic, self.processpool, job, cb, echo, justprint)

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

