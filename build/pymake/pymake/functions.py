"""
Makefile functions.
"""

import parser, util
import subprocess, os, logging
from globrelative import glob
from cStringIO import StringIO

log = logging.getLogger('pymake.data')

class Function(object):
    """
    An object that represents a function call. This class is always subclassed
    with the following methods and attributes:

    minargs = minimum # of arguments
    maxargs = maximum # of arguments (0 means unlimited)

    def resolve(self, makefile, variables, fd, setting)
        Calls the function
        calls fd.write() with strings
    """

    __slots__ = ('_arguments', 'loc')

    def __init__(self, loc):
        self._arguments = []
        self.loc = loc
        assert self.minargs > 0

    def __getitem__(self, key):
        return self._arguments[key]

    def setup(self):
        argc = len(self._arguments)

        if argc < self.minargs:
            raise data.DataError("Not enough arguments to function %s, requires %s" % (self.name, self.minargs), self.loc)

        assert self.maxargs == 0 or argc <= self.maxargs, "Parser screwed up, gave us too many args"

    def append(self, arg):
        assert isinstance(arg, (data.Expansion, data.StringExpansion))
        self._arguments.append(arg)

    def __len__(self):
        return len(self._arguments)

    def __repr__(self):
        return "%s<%s>(%r)" % (
            self.__class__.__name__, self.loc,
            ','.join([repr(a) for a in self._arguments]),
            )

class VariableRef(Function):
    __slots__ = ('vname', 'loc')

    def __init__(self, loc, vname):
        self.loc = loc
        assert isinstance(vname, (data.Expansion, data.StringExpansion))
        self.vname = vname
        
    def setup(self):
        assert False, "Shouldn't get here"

    def resolve(self, makefile, variables, fd, setting):
        vname = self.vname.resolvestr(makefile, variables, setting)
        if vname in setting:
            raise data.DataError("Setting variable '%s' recursively references itself." % (vname,), self.loc)

        flavor, source, value = variables.get(vname)
        if value is None:
            log.debug("%s: variable '%s' was not set" % (self.loc, vname))
            return

        value.resolve(makefile, variables, fd, setting + [vname])

    def __repr__(self):
        return "VariableRef<%s>(%r)" % (self.loc, self.vname)

class SubstitutionRef(Function):
    """$(VARNAME:.c=.o) and $(VARNAME:%.c=%.o)"""

    __slots__ = ('loc', 'vname', 'substfrom', 'substto')

    def __init__(self, loc, varname, substfrom, substto):
        self.loc = loc
        self.vname = varname
        self.substfrom = substfrom
        self.substto = substto

    def setup(self):
        assert False, "Shouldn't get here"

    def resolve(self, makefile, variables, fd, setting):
        vname = self.vname.resolvestr(makefile, variables, setting)
        if vname in setting:
            raise data.DataError("Setting variable '%s' recursively references itself." % (vname,), self.loc)

        substfrom = self.substfrom.resolvestr(makefile, variables, setting)
        substto = self.substto.resolvestr(makefile, variables, setting)

        flavor, source, value = variables.get(vname)
        if value is None:
            log.debug("%s: variable '%s' was not set" % (self.loc, vname))
            return

        f = data.Pattern(substfrom)
        if not f.ispattern():
            f = data.Pattern('%' + substfrom)
            substto = '%' + substto

        fd.write(' '.join([f.subst(substto, word, False)
                           for word in value.resolvesplit(makefile, variables, setting + [vname])]))

    def __repr__(self):
        return "SubstitutionRef<%s>(%r:%r=%r)" % (
            self.loc, self.vname, self.substfrom, selfsubstto,)

class SubstFunction(Function):
    name = 'subst'
    minargs = 3
    maxargs = 3

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        s = self._arguments[0].resolvestr(makefile, variables, setting)
        r = self._arguments[1].resolvestr(makefile, variables, setting)
        d = self._arguments[2].resolvestr(makefile, variables, setting)
        fd.write(d.replace(s, r))

class PatSubstFunction(Function):
    name = 'patsubst'
    minargs = 3
    maxargs = 3

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        s = self._arguments[0].resolvestr(makefile, variables, setting)
        r = self._arguments[1].resolvestr(makefile, variables, setting)

        p = data.Pattern(s)
        fd.write(' '.join([p.subst(r, word, False)
                           for word in self._arguments[2].resolvesplit(makefile, variables, setting)]))

class StripFunction(Function):
    name = 'strip'
    minargs = 1
    maxargs = 1

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        util.joiniter(fd, self._arguments[0].resolvesplit(makefile, variables, setting))

class FindstringFunction(Function):
    name = 'findstring'
    minargs = 2
    maxargs = 2

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        s = self._arguments[0].resolvestr(makefile, variables, setting)
        r = self._arguments[1].resolvestr(makefile, variables, setting)
        if r.find(s) == -1:
            return
        fd.write(s)

class FilterFunction(Function):
    name = 'filter'
    minargs = 2
    maxargs = 2

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        plist = [data.Pattern(p)
                 for p in self._arguments[0].resolvesplit(makefile, variables, setting)]

        fd.write(' '.join([w for w in self._arguments[1].resolvesplit(makefile, variables, setting)
                           if util.any((p.match(w) for p in plist))]))

class FilteroutFunction(Function):
    name = 'filter-out'
    minargs = 2
    maxargs = 2

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        plist = [data.Pattern(p)
                 for p in self._arguments[0].resolvesplit(makefile, variables, setting)]

        fd.write(' '.join([w for w in self._arguments[1].resolvesplit(makefile, variables, setting)
                           if not util.any((p.match(w) for p in plist))]))

class SortFunction(Function):
    name = 'sort'
    minargs = 1
    maxargs = 1

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        d = set(self._arguments[0].resolvesplit(makefile, variables, setting))
        util.joiniter(fd, sorted(d))

class WordFunction(Function):
    name = 'word'
    minargs = 2
    maxargs = 2

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        n = self._arguments[0].resolvestr(makefile, variables, setting)
        # TODO: provide better error if this doesn't convert
        n = int(n)
        words = list(self._arguments[1].resolvesplit(makefile, variables, setting))
        if n < 1 or n > len(words):
            return
        fd.write(words[n - 1])

class WordlistFunction(Function):
    name = 'wordlist'
    minargs = 3
    maxargs = 3

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        nfrom = self._arguments[0].resolvestr(makefile, variables, setting)
        nto = self._arguments[1].resolvestr(makefile, variables, setting)
        # TODO: provide better errors if this doesn't convert
        nfrom = int(nfrom)
        nto = int(nto)

        words = list(self._arguments[2].resolvesplit(makefile, variables, setting))

        if nfrom < 1:
            nfrom = 1
        if nto < 1:
            nto = 1

        util.joiniter(fd, words[nfrom - 1:nto])

class WordsFunction(Function):
    name = 'words'
    minargs = 1
    maxargs = 1

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        fd.write(str(len(self._arguments[0].resolvesplit(makefile, variables, setting))))

class FirstWordFunction(Function):
    name = 'firstword'
    minargs = 1
    maxargs = 1

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        l = self._arguments[0].resolvesplit(makefile, variables, setting)
        if len(l):
            fd.write(l[0])

class LastWordFunction(Function):
    name = 'lastword'
    minargs = 1
    maxargs = 1

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        l = self._arguments[0].resolvesplit(makefile, variables, setting)
        if len(l):
            fd.write(l[-1])

def pathsplit(path, default='./'):
    """
    Splits a path into dirpart, filepart on the last slash. If there is no slash, dirpart
    is ./
    """
    dir, slash, file = util.strrpartition(path, '/')
    if dir == '':
        return default, file

    return dir + slash, file

class DirFunction(Function):
    name = 'dir'
    minargs = 1
    maxargs = 1

    def resolve(self, makefile, variables, fd, setting):
        fd.write(' '.join([pathsplit(path)[0]
                           for path in self._arguments[0].resolvesplit(makefile, variables, setting)]))

class NotDirFunction(Function):
    name = 'notdir'
    minargs = 1
    maxargs = 1

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        fd.write(' '.join([pathsplit(path)[1]
                           for path in self._arguments[0].resolvesplit(makefile, variables, setting)]))

class SuffixFunction(Function):
    name = 'suffix'
    minargs = 1
    maxargs = 1

    __slots__ = Function.__slots__

    @staticmethod
    def suffixes(words):
        for w in words:
            dir, file = pathsplit(w)
            base, dot, suffix = util.strrpartition(file, '.')
            if base != '':
                yield dot + suffix

    def resolve(self, makefile, variables, fd, setting):
        util.joiniter(fd, self.suffixes(self._arguments[0].resolvesplit(makefile, variables, setting)))

class BasenameFunction(Function):
    name = 'basename'
    minargs = 1
    maxargs = 1

    __slots__ = Function.__slots__

    @staticmethod
    def basenames(words):
        for w in words:
            dir, file = pathsplit(w, '')
            base, dot, suffix = util.strrpartition(file, '.')
            if dot == '':
                base = suffix

            yield dir + base

    def resolve(self, makefile, variables, fd, setting):
        util.joiniter(fd, self.basenames(self._arguments[0].resolvesplit(makefile, variables, setting)))

class AddSuffixFunction(Function):
    name = 'addprefix'
    minargs = 2
    maxargs = 2

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        suffix = self._arguments[0].resolvestr(makefile, variables, setting)

        fd.write(' '.join([w + suffix for w in self._arguments[1].resolvesplit(makefile, variables, setting)]))

class AddPrefixFunction(Function):
    name = 'addsuffix'
    minargs = 2
    maxargs = 2

    def resolve(self, makefile, variables, fd, setting):
        prefix = self._arguments[0].resolvestr(makefile, variables, setting)

        fd.write(' '.join([prefix + w for w in self._arguments[1].resolvesplit(makefile, variables, setting)]))

class JoinFunction(Function):
    name = 'join'
    minargs = 2
    maxargs = 2

    __slots__ = Function.__slots__

    @staticmethod
    def iterjoin(l1, l2):
        for i in xrange(0, max(len(l1), len(l2))):
            i1 = i < len(l1) and l1[i] or ''
            i2 = i < len(l2) and l2[i] or ''
            yield i1 + i2

    def resolve(self, makefile, variables, fd, setting):
        list1 = list(self._arguments[0].resolvesplit(makefile, variables, setting))
        list2 = list(self._arguments[1].resolvesplit(makefile, variables, setting))

        util.joiniter(fd, self.iterjoin(list1, list2))

class WildcardFunction(Function):
    name = 'wildcard'
    minargs = 1
    maxargs = 1

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        patterns = self._arguments[0].resolvesplit(makefile, variables, setting)

        fd.write(' '.join([x.replace('\\','/')
                           for p in patterns
                           for x in glob(makefile.workdir, p)]))

    __slots__ = Function.__slots__

class RealpathFunction(Function):
    name = 'realpath'
    minargs = 1
    maxargs = 1

    def resolve(self, makefile, variables, fd, setting):
        fd.write(' '.join([os.path.realpath(os.path.join(makefile.workdir, path)).replace('\\', '/')
                           for path in self._arguments[0].resolvesplit(makefile, variables, setting)]))

class AbspathFunction(Function):
    name = 'abspath'
    minargs = 1
    maxargs = 1

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        assert os.path.isabs(makefile.workdir)
        fd.write(' '.join([util.normaljoin(makefile.workdir, path).replace('\\', '/')
                           for path in self._arguments[0].resolvesplit(makefile, variables, setting)]))

class IfFunction(Function):
    name = 'if'
    minargs = 1
    maxargs = 3

    __slots__ = Function.__slots__

    def setup(self):
        Function.setup(self)
        self._arguments[0].lstrip()
        self._arguments[0].rstrip()

    def resolve(self, makefile, variables, fd, setting):
        condition = self._arguments[0].resolvestr(makefile, variables, setting)

        if len(condition):
            self._arguments[1].resolve(makefile, variables, fd, setting)
        elif len(self._arguments) > 2:
            return self._arguments[2].resolve(makefile, variables, fd, setting)

class OrFunction(Function):
    name = 'or'
    minargs = 1
    maxargs = 0

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        for arg in self._arguments:
            r = arg.resolvestr(makefile, variables, setting)
            if r != '':
                fd.write(r)
                return

class AndFunction(Function):
    name = 'and'
    minargs = 1
    maxargs = 0

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        r = ''

        for arg in self._arguments:
            r = arg.resolvestr(makefile, variables, setting)
            if r == '':
                return

        fd.write(r)

class ForEachFunction(Function):
    name = 'foreach'
    minargs = 3
    maxargs = 3

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        vname = self._arguments[0].resolvestr(makefile, variables, setting)
        e = self._arguments[2]

        v = data.Variables(parent=variables)
        firstword = True

        for w in self._arguments[1].resolvesplit(makefile, variables, setting):
            if firstword:
                firstword = False
            else:
                fd.write(' ')

            v.set(vname, data.Variables.FLAVOR_SIMPLE, data.Variables.SOURCE_AUTOMATIC, w)
            e.resolve(makefile, v, fd, setting)

class CallFunction(Function):
    name = 'call'
    minargs = 1
    maxargs = 0

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        vname = self._arguments[0].resolvestr(makefile, variables, setting)
        if vname in setting:
            raise data.DataError("Recursively setting variable '%s'" % (vname,))

        v = data.Variables(parent=variables)
        v.set('0', data.Variables.FLAVOR_SIMPLE, data.Variables.SOURCE_AUTOMATIC, vname)
        for i in xrange(1, len(self._arguments)):
            param = self._arguments[i].resolvestr(makefile, variables, setting)
            v.set(str(i), data.Variables.FLAVOR_SIMPLE, data.Variables.SOURCE_AUTOMATIC, param)

        flavor, source, e = variables.get(vname)

        if e is None:
            return

        if flavor == data.Variables.FLAVOR_SIMPLE:
            log.warning("%s: calling variable '%s' which is simply-expanded" % (self.loc, vname))

        # but we'll do it anyway
        e.resolve(makefile, v, fd, setting + [vname])

class ValueFunction(Function):
    name = 'value'
    minargs = 1
    maxargs = 1

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        varname = self._arguments[0].resolvestr(makefile, variables, setting)

        flavor, source, value = variables.get(varname, expand=False)
        if value is not None:
            fd.write(value)

class EvalFunction(Function):
    name = 'eval'
    minargs = 1
    maxargs = 1

    def resolve(self, makefile, variables, fd, setting):
        if makefile.parsingfinished:
            # GNU make allows variables to be set by recursive expansion during
            # command execution. This seems really dumb to me, so I don't!
            raise data.DataError("$(eval) not allowed via recursive expansion after parsing is finished", self.loc)

        stmts = parser.parsestring(self._arguments[0].resolvestr(makefile, variables, setting),
                                   'evaluation from %s' % self.loc)
        stmts.execute(makefile)

class OriginFunction(Function):
    name = 'origin'
    minargs = 1
    maxargs = 1

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        vname = self._arguments[0].resolvestr(makefile, variables, setting)

        flavor, source, value = variables.get(vname)
        if source is None:
            r = 'undefined'
        elif source == data.Variables.SOURCE_OVERRIDE:
            r = 'override'

        elif source == data.Variables.SOURCE_MAKEFILE:
            r = 'file'
        elif source == data.Variables.SOURCE_ENVIRONMENT:
            r = 'environment'
        elif source == data.Variables.SOURCE_COMMANDLINE:
            r = 'command line'
        elif source == data.Variables.SOURCE_AUTOMATIC:
            r = 'automatic'
        elif source == data.Variables.SOURCE_IMPLICIT:
            r = 'default'

        fd.write(r)

class FlavorFunction(Function):
    name = 'flavor'
    minargs = 1
    maxargs = 1

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        varname = self._arguments[0].resolvestr(makefile, variables, setting)
        
        flavor, source, value = variables.get(varname)
        if flavor is None:
            r = 'undefined'
        elif flavor == data.Variables.FLAVOR_RECURSIVE:
            r = 'recursive'
        elif flavor == data.Variables.FLAVOR_SIMPLE:
            r = 'simple'
        fd.write(r)

class ShellFunction(Function):
    name = 'shell'
    minargs = 1
    maxargs = 1

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        #TODO: call this once up-front somewhere and save the result?
        shell, msys = util.checkmsyscompat()
        cline = self._arguments[0].resolvestr(makefile, variables, setting)

        log.debug("%s: running shell command '%s'" % (self.loc, cline))
        if msys:
            cline = [shell, "-c", cline]
        p = subprocess.Popen(cline, shell=not msys, stdout=subprocess.PIPE, cwd=makefile.workdir)
        stdout, stderr = p.communicate()

        stdout = stdout.replace('\r\n', '\n')
        if stdout.endswith('\n'):
            stdout = stdout[:-1]
        stdout = stdout.replace('\n', ' ')

        fd.write(stdout)

class ErrorFunction(Function):
    name = 'error'
    minargs = 1
    maxargs = 1

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        v = self._arguments[0].resolvestr(makefile, variables, setting)
        raise data.DataError(v, self.loc)

class WarningFunction(Function):
    name = 'warning'
    minargs = 1
    maxargs = 1

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        v = self._arguments[0].resolvestr(makefile, variables, setting)
        log.warning(v)

class InfoFunction(Function):
    name = 'info'
    minargs = 1
    maxargs = 1

    __slots__ = Function.__slots__

    def resolve(self, makefile, variables, fd, setting):
        v = self._arguments[0].resolvestr(makefile, variables, setting)
        print v

functionmap = {
    'subst': SubstFunction,
    'patsubst': PatSubstFunction,
    'strip': StripFunction,
    'findstring': FindstringFunction,
    'filter': FilterFunction,
    'filter-out': FilteroutFunction,
    'sort': SortFunction,
    'word': WordFunction,
    'wordlist': WordlistFunction,
    'words': WordsFunction,
    'firstword': FirstWordFunction,
    'lastword': LastWordFunction,
    'dir': DirFunction,
    'notdir': NotDirFunction,
    'suffix': SuffixFunction,
    'basename': BasenameFunction,
    'addsuffix': AddSuffixFunction,
    'addprefix': AddPrefixFunction,
    'join': JoinFunction,
    'wildcard': WildcardFunction,
    'realpath': RealpathFunction,
    'abspath': AbspathFunction,
    'if': IfFunction,
    'or': OrFunction,
    'and': AndFunction,
    'foreach': ForEachFunction,
    'call': CallFunction,
    'value': ValueFunction,
    'eval': EvalFunction,
    'origin': OriginFunction,
    'flavor': FlavorFunction,
    'shell': ShellFunction,
    'error': ErrorFunction,
    'warning': WarningFunction,
    'info': InfoFunction,
}

import data
