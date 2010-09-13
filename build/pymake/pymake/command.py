"""
Makefile execution.

Multiple `makes` can be run within the same process. Each one has an entirely data.Makefile and .Target
structure, environment, and working directory. Typically they will all share a parallel execution context,
except when a submake specifies -j1 when the parent make is building in parallel.
"""

import os, subprocess, sys, logging, time, traceback, re
from optparse import OptionParser
import data, parserdata, process, util

# TODO: If this ever goes from relocatable package to system-installed, this may need to be
# a configured-in path.

makepypath = util.normaljoin(os.path.dirname(__file__), '../make.py')

_simpleopts = re.compile(r'^[a-zA-Z]+(\s|$)')
def parsemakeflags(env):
    """
    Parse MAKEFLAGS from the environment into a sequence of command-line arguments.
    """

    makeflags = env.get('MAKEFLAGS', '')
    makeflags = makeflags.strip()

    if makeflags == '':
        return []

    if _simpleopts.match(makeflags):
        makeflags = '-' + makeflags

    opts = []
    curopt = ''

    i = 0
    while i < len(makeflags):
        c = makeflags[i]
        if c.isspace():
            opts.append(curopt)
            curopt = ''
            i += 1
            while i < len(makeflags) and makeflags[i].isspace():
                i += 1
            continue

        if c == '\\':
            i += 1
            if i == len(makeflags):
                raise data.DataError("MAKEFLAGS has trailing backslash")
            c = makeflags[i]
            
        curopt += c
        i += 1

    if curopt != '':
        opts.append(curopt)

    return opts

def _version(*args):
    print """pymake: GNU-compatible make program
Copyright (C) 2009 The Mozilla Foundation <http://www.mozilla.org/>
This is free software; see the source for copying conditions.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE."""

_log = logging.getLogger('pymake.execution')

class _MakeContext(object):
    def __init__(self, makeflags, makelevel, workdir, context, env, targets, options, ostmts, overrides, cb):
        self.makeflags = makeflags
        self.makelevel = makelevel

        self.workdir = workdir
        self.context = context
        self.env = env
        self.targets = targets
        self.options = options
        self.ostmts = ostmts
        self.overrides = overrides
        self.cb = cb

        self.restarts = 0

        self.remakecb(True)

    def remakecb(self, remade, error=None):
        if error is not None:
            print error
            self.context.defer(self.cb, 2)
            return

        if remade:
            if self.restarts > 0:
                _log.info("make.py[%i]: Restarting makefile parsing", self.makelevel)

            self.makefile = data.Makefile(restarts=self.restarts,
                                          make='%s %s' % (sys.executable.replace('\\', '/'), makepypath.replace('\\', '/')),
                                          makeflags=self.makeflags,
                                          makeoverrides=self.overrides,
                                          workdir=self.workdir,
                                          context=self.context, env=self.env, makelevel=self.makelevel,
                                          targets=self.targets, keepgoing=self.options.keepgoing,
                                          silent=self.options.silent)

            self.restarts += 1

            try:
                self.ostmts.execute(self.makefile)
                for f in self.options.makefiles:
                    self.makefile.include(f)
                self.makefile.finishparsing()
                self.makefile.remakemakefiles(self.remakecb)
            except util.MakeError, e:
                print e
                self.context.defer(self.cb, 2)

            return

        if len(self.targets) == 0:
            if self.makefile.defaulttarget is None:
                print "No target specified and no default target found."
                self.context.defer(self.cb, 2)
                return

            _log.info("Making default target %s", self.makefile.defaulttarget)
            self.realtargets = [self.makefile.defaulttarget]
            self.tstack = ['<default-target>']
        else:
            self.realtargets = self.targets
            self.tstack = ['<command-line>']

        self.makefile.gettarget(self.realtargets.pop(0)).make(self.makefile, self.tstack, cb=self.makecb)

    def makecb(self, error, didanything):
        assert error in (True, False)

        if error:
            self.context.defer(self.cb, 2)
            return

        if not len(self.realtargets):
            if self.options.printdir:
                print "make.py[%i]: Leaving directory '%s'" % (self.makelevel, self.workdir)
            sys.stdout.flush()

            self.context.defer(self.cb, 0)
        else:
            self.makefile.gettarget(self.realtargets.pop(0)).make(self.makefile, self.tstack, self.makecb)

def main(args, env, cwd, cb):
    """
    Start a single makefile execution, given a command line, working directory, and environment.

    @param cb a callback to notify with an exit code when make execution is finished.
    """

    try:
        makelevel = int(env.get('MAKELEVEL', '0'))

        op = OptionParser()
        op.add_option('-f', '--file', '--makefile',
                      action='append',
                      dest='makefiles',
                      default=[])
        op.add_option('-d',
                      action="store_true",
                      dest="verbose", default=False)
        op.add_option('-k', '--keep-going',
                      action="store_true",
                      dest="keepgoing", default=False)
        op.add_option('--debug-log',
                      dest="debuglog", default=None)
        op.add_option('-C', '--directory',
                      dest="directory", default=None)
        op.add_option('-v', '--version', action="store_true",
                      dest="printversion", default=False)
        op.add_option('-j', '--jobs', type="int",
                      dest="jobcount", default=1)
        op.add_option('-w', '--print-directory', action="store_true",
                      dest="printdir")
        op.add_option('--no-print-directory', action="store_false",
                      dest="printdir", default=True)
        op.add_option('-s', '--silent', action="store_true",
                      dest="silent", default=False)

        options, arguments1 = op.parse_args(parsemakeflags(env))
        options, arguments2 = op.parse_args(args, values=options)

        op.destroy()

        arguments = arguments1 + arguments2

        if options.printversion:
            _version()
            cb(0)
            return

        shortflags = []
        longflags = []

        if options.keepgoing:
            shortflags.append('k')

        if options.printdir:
            shortflags.append('w')

        if options.silent:
            shortflags.append('s')
            options.printdir = False

        loglevel = logging.WARNING
        if options.verbose:
            loglevel = logging.DEBUG
            shortflags.append('d')

        logkwargs = {}
        if options.debuglog:
            logkwargs['filename'] = options.debuglog
            longflags.append('--debug-log=%s' % options.debuglog)

        if options.directory is None:
            workdir = cwd
        else:
            workdir = util.normaljoin(cwd, options.directory)

        if options.jobcount != 1:
            longflags.append('-j%i' % (options.jobcount,))

        makeflags = ''.join(shortflags)
        if len(longflags):
            makeflags += ' ' + ' '.join(longflags)

        logging.basicConfig(level=loglevel, **logkwargs)

        context = process.getcontext(options.jobcount)

        if options.printdir:
            print "make.py[%i]: Entering directory '%s'" % (makelevel, workdir)
            sys.stdout.flush()

        if len(options.makefiles) == 0:
            if os.path.exists(util.normaljoin(workdir, 'Makefile')):
                options.makefiles.append('Makefile')
            else:
                print "No makefile found"
                cb(2)
                return

        ostmts, targets, overrides = parserdata.parsecommandlineargs(arguments)

        _MakeContext(makeflags, makelevel, workdir, context, env, targets, options, ostmts, overrides, cb)
    except (util.MakeError), e:
        print e
        if options.printdir:
            print "make.py[%i]: Leaving directory '%s'" % (makelevel, workdir)
        sys.stdout.flush()
        cb(2)
        return
