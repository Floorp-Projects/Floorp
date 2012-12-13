"""
A representation of makefile data structures.
"""

import logging, re, os, sys
import parserdata, parser, functions, process, util, implicit
from cStringIO import StringIO

if sys.version_info[0] < 3:
    str_type = basestring
else:
    str_type = str

_log = logging.getLogger('pymake.data')

class DataError(util.MakeError):
    pass

class ResolutionError(DataError):
    """
    Raised when dependency resolution fails, either due to recursion or to missing
    prerequisites.This is separately catchable so that implicit rule search can try things
    without having to commit.
    """
    pass

def withoutdups(it):
    r = set()
    for i in it:
        if not i in r:
            r.add(i)
            yield i

def mtimeislater(deptime, targettime):
    """
    Is the mtime of the dependency later than the target?
    """

    if deptime is None:
        return True
    if targettime is None:
        return False
    return deptime > targettime

def getmtime(path):
    try:
        s = os.stat(path)
        return s.st_mtime
    except OSError:
        return None

def stripdotslash(s):
    if s.startswith('./'):
        st = s[2:]
        return st if st != '' else '.'
    return s

def stripdotslashes(sl):
    for s in sl:
        yield stripdotslash(s)

def getindent(stack):
    return ''.ljust(len(stack) - 1)

def _if_else(c, t, f):
    if c:
        return t()
    return f()


class BaseExpansion(object):
    """Base class for expansions.

    A make expansion is the parsed representation of a string, which may
    contain references to other elements.
    """

    @property
    def is_static_string(self):
        """Returns whether the expansion is composed of static string content.

        This is always True for StringExpansion. It will be True for Expansion
        only if all elements of that Expansion are static strings.
        """
        raise Exception('Must be implemented in child class.')

    def functions(self, descend=False):
        """Obtain all functions inside this expansion.

        This is a generator for pymake.functions.Function instances.

        By default, this only returns functions existing as the primary
        elements of this expansion. If `descend` is True, it will descend into
        child expansions and extract all functions in the tree.
        """
        # An empty generator. Yeah, it's weird.
        for x in []:
            yield x

    def variable_references(self, descend=False):
        """Obtain all variable references in this expansion.

        This is a generator for pymake.functionsVariableRef instances.

        To retrieve the names of variables, simply query the `vname` field on
        the returned instances. Most of the time these will be StringExpansion
        instances.
        """
        for f in self.functions(descend=descend):
            if not isinstance(f, functions.VariableRef):
                continue

            yield f

    @property
    def is_filesystem_dependent(self):
        """Whether this expansion may query the filesystem for evaluation.

        This effectively asks "is any function in this expansion dependent on
        the filesystem.
        """
        for f in self.functions(descend=True):
            if f.is_filesystem_dependent:
                return True

        return False

    @property
    def is_shell_dependent(self):
        """Whether this expansion may invoke a shell for evaluation."""

        for f in self.functions(descend=True):
            if isinstance(f, functions.ShellFunction):
                return True

        return False


class StringExpansion(BaseExpansion):
    """An Expansion representing a static string.

    This essentially wraps a single str instance.
    """

    __slots__ = ('loc', 's',)
    simple = True

    def __init__(self, s, loc):
        assert isinstance(s, str_type)
        self.s = s
        self.loc = loc

    def lstrip(self):
        self.s = self.s.lstrip()

    def rstrip(self):
        self.s = self.s.rstrip()

    def isempty(self):
        return self.s == ''

    def resolve(self, i, j, fd, k=None):
        fd.write(self.s)

    def resolvestr(self, i, j, k=None):
        return self.s

    def resolvesplit(self, i, j, k=None):
        return self.s.split()

    def clone(self):
        e = Expansion(self.loc)
        e.appendstr(self.s)
        return e

    @property
    def is_static_string(self):
        return True

    def __len__(self):
        return 1

    def __getitem__(self, i):
        assert i == 0
        return self.s, False

    def __repr__(self):
        return "Exp<%s>(%r)" % (self.loc, self.s)

    def __eq__(self, other):
        """We only compare the string contents."""
        return self.s == other

    def __ne__(self, other):
        return not self.__eq__(other)

    def to_source(self, escape_variables=False, escape_comments=False):
        s = self.s

        if escape_comments:
            s = s.replace('#', '\\#')

        if escape_variables:
            return s.replace('$', '$$')

        return s


class Expansion(BaseExpansion, list):
    """A representation of expanded data.

    This is effectively an ordered list of StringExpansion and
    pymake.function.Function instances. Every item in the collection appears in
    the same context in a make file.
    """

    __slots__ = ('loc',)
    simple = False

    def __init__(self, loc=None):
        # A list of (element, isfunc) tuples
        # element is either a string or a function
        self.loc = loc

    @staticmethod
    def fromstring(s, path):
        return StringExpansion(s, parserdata.Location(path, 1, 0))

    def clone(self):
        e = Expansion()
        e.extend(self)
        return e

    def appendstr(self, s):
        assert isinstance(s, str_type)
        if s == '':
            return

        self.append((s, False))

    def appendfunc(self, func):
        assert isinstance(func, functions.Function)
        self.append((func, True))

    def concat(self, o):
        """Concatenate the other expansion on to this one."""
        if o.simple:
            self.appendstr(o.s)
        else:
            self.extend(o)

    def isempty(self):
        return (not len(self)) or self[0] == ('', False)

    def lstrip(self):
        """Strip leading literal whitespace from this expansion."""
        while True:
            i, isfunc = self[0]
            if isfunc:
                return

            i = i.lstrip()
            if i != '':
                self[0] = i, False
                return

            del self[0]

    def rstrip(self):
        """Strip trailing literal whitespace from this expansion."""
        while True:
            i, isfunc = self[-1]
            if isfunc:
                return

            i = i.rstrip()
            if i != '':
                self[-1] = i, False
                return

            del self[-1]

    def finish(self):
        # Merge any adjacent literal strings:
        strings = []
        elements = []
        for (e, isfunc) in self:
            if isfunc:
                if strings:
                    s = ''.join(strings)
                    if s:
                        elements.append((s, False))
                    strings = []
                elements.append((e, True))
            else:
                strings.append(e)

        if not elements:
            # This can only happen if there were no function elements.
            return StringExpansion(''.join(strings), self.loc)

        if strings:
            s = ''.join(strings)
            if s:
                elements.append((s, False))

        if len(elements) < len(self):
            self[:] = elements

        return self

    def resolve(self, makefile, variables, fd, setting=[]):
        """
        Resolve this variable into a value, by interpolating the value
        of other variables.

        @param setting (Variable instance) the variable currently
               being set, if any. Setting variables must avoid self-referential
               loops.
        """
        assert isinstance(makefile, Makefile)
        assert isinstance(variables, Variables)
        assert isinstance(setting, list)

        for e, isfunc in self:
            if isfunc:
                e.resolve(makefile, variables, fd, setting)
            else:
                assert isinstance(e, str_type)
                fd.write(e)

    def resolvestr(self, makefile, variables, setting=[]):
        fd = StringIO()
        self.resolve(makefile, variables, fd, setting)
        return fd.getvalue()

    def resolvesplit(self, makefile, variables, setting=[]):
        return self.resolvestr(makefile, variables, setting).split()

    @property
    def is_static_string(self):
        """An Expansion is static if all its components are strings, not
        functions."""
        for e, is_func in self:
            if is_func:
                return False

        return True

    def functions(self, descend=False):
        for e, is_func in self:
            if is_func:
                yield e

            if descend:
                for exp in e.expansions(descend=True):
                    for f in exp.functions(descend=True):
                        yield f

    def __repr__(self):
        return "<Expansion with elements: %r>" % ([e for e, isfunc in self],)

    def to_source(self, escape_variables=False, escape_comments=False):
        parts = []
        for e, is_func in self:
            if is_func:
                parts.append(e.to_source())
                continue

            if escape_variables:
                parts.append(e.replace('$', '$$'))
                continue

            parts.append(e)

        return ''.join(parts)

    def __eq__(self, other):
        if not isinstance(other, (Expansion, StringExpansion)):
            return False

        # Expansions are equivalent if adjacent string literals normalize to
        # the same value. So, we must normalize before any comparisons are
        # made.
        a = self.clone().finish()

        if isinstance(other, StringExpansion):
            if isinstance(a, StringExpansion):
                return a == other

            # A normalized Expansion != StringExpansion.
            return False

        b = other.clone().finish()

        # b could be a StringExpansion now.
        if isinstance(b, StringExpansion):
            if isinstance(a, StringExpansion):
                return a == b

            # Our normalized Expansion != normalized StringExpansion.
            return False

        if len(a) != len(b):
            return False

        for i in xrange(len(self)):
            e1, is_func1 = a[i]
            e2, is_func2 = b[i]

            if is_func1 != is_func2:
                return False

            if type(e1) != type(e2):
                return False

            if e1 != e2:
                return False

        return True

    def __ne__(self, other):
        return not self.__eq__(other)

class Variables(object):
    """
    A mapping from variable names to variables. Variables have flavor, source, and value. The value is an 
    expansion object.
    """

    __slots__ = ('parent', '_map')

    FLAVOR_RECURSIVE = 0
    FLAVOR_SIMPLE = 1
    FLAVOR_APPEND = 2

    SOURCE_OVERRIDE = 0
    SOURCE_COMMANDLINE = 1
    SOURCE_MAKEFILE = 2
    SOURCE_ENVIRONMENT = 3
    SOURCE_AUTOMATIC = 4
    SOURCE_IMPLICIT = 5

    def __init__(self, parent=None):
        self._map = {} # vname -> flavor, source, valuestr, valueexp
        self.parent = parent

    def readfromenvironment(self, env):
        for k, v in env.iteritems():
            self.set(k, self.FLAVOR_RECURSIVE, self.SOURCE_ENVIRONMENT, v)

    def get(self, name, expand=True):
        """
        Get the value of a named variable. Returns a tuple (flavor, source, value)

        If the variable is not present, returns (None, None, None)

        @param expand If true, the value will be returned as an expansion. If false,
        it will be returned as an unexpanded string.
        """
        flavor, source, valuestr, valueexp = self._map.get(name, (None, None, None, None))
        if flavor is not None:
            if expand and flavor != self.FLAVOR_SIMPLE and valueexp is None:
                d = parser.Data.fromstring(valuestr, parserdata.Location("Expansion of variables '%s'" % (name,), 1, 0))
                valueexp, t, o = parser.parsemakesyntax(d, 0, (), parser.iterdata)
                self._map[name] = flavor, source, valuestr, valueexp

            if flavor == self.FLAVOR_APPEND:
                if self.parent:
                    pflavor, psource, pvalue = self.parent.get(name, expand)
                else:
                    pflavor, psource, pvalue = None, None, None

                if pvalue is None:
                    flavor = self.FLAVOR_RECURSIVE
                    # fall through
                else:
                    if source > psource:
                        # TODO: log a warning?
                        return pflavor, psource, pvalue

                    if not expand:
                        return pflavor, psource, pvalue + ' ' + valuestr

                    pvalue = pvalue.clone()
                    pvalue.appendstr(' ')
                    pvalue.concat(valueexp)

                    return pflavor, psource, pvalue
                    
            if not expand:
                return flavor, source, valuestr

            if flavor == self.FLAVOR_RECURSIVE:
                val = valueexp
            else:
                val = Expansion.fromstring(valuestr, "Expansion of variable '%s'" % (name,))

            return flavor, source, val

        if self.parent is not None:
            return self.parent.get(name, expand)

        return (None, None, None)

    def set(self, name, flavor, source, value):
        assert flavor in (self.FLAVOR_RECURSIVE, self.FLAVOR_SIMPLE)
        assert source in (self.SOURCE_OVERRIDE, self.SOURCE_COMMANDLINE, self.SOURCE_MAKEFILE, self.SOURCE_ENVIRONMENT, self.SOURCE_AUTOMATIC, self.SOURCE_IMPLICIT)
        assert isinstance(value, str_type), "expected str, got %s" % type(value)

        prevflavor, prevsource, prevvalue = self.get(name)
        if prevsource is not None and source > prevsource:
            # TODO: give a location for this warning
            _log.info("not setting variable '%s', set by higher-priority source to value '%s'" % (name, prevvalue))
            return

        self._map[name] = flavor, source, value, None

    def append(self, name, source, value, variables, makefile):
        assert source in (self.SOURCE_OVERRIDE, self.SOURCE_MAKEFILE, self.SOURCE_AUTOMATIC)
        assert isinstance(value, str_type)

        if name not in self._map:
            self._map[name] = self.FLAVOR_APPEND, source, value, None
            return

        prevflavor, prevsource, prevvalue, valueexp = self._map[name]
        if source > prevsource:
            # TODO: log a warning?
            return

        if prevflavor == self.FLAVOR_SIMPLE:
            d = parser.Data.fromstring(value, parserdata.Location("Expansion of variables '%s'" % (name,), 1, 0))
            valueexp, t, o = parser.parsemakesyntax(d, 0, (), parser.iterdata)

            val = valueexp.resolvestr(makefile, variables, [name])
            self._map[name] = prevflavor, prevsource, prevvalue + ' ' + val, None
            return

        newvalue = prevvalue + ' ' + value
        self._map[name] = prevflavor, prevsource, newvalue, None

    def merge(self, other):
        assert isinstance(other, Variables)
        for k, flavor, source, value in other:
            self.set(k, flavor, source, value)

    def __iter__(self):
        for k, (flavor, source, value, valueexp) in self._map.iteritems():
            yield k, flavor, source, value

    def __contains__(self, item):
        return item in self._map

class Pattern(object):
    """
    A pattern is a string, possibly with a % substitution character. From the GNU make manual:

    '%' characters in pattern rules can be quoted with precending backslashes ('\'). Backslashes that
    would otherwise quote '%' charcters can be quoted with more backslashes. Backslashes that
    quote '%' characters or other backslashes are removed from the pattern before it is compared t
    file names or has a stem substituted into it. Backslashes that are not in danger of quoting '%'
    characters go unmolested. For example, the pattern the\%weird\\%pattern\\ has `the%weird\' preceding
    the operative '%' character, and 'pattern\\' following it. The final two backslashes are left alone
    because they cannot affect any '%' character.

    This insane behavior probably doesn't matter, but we're compatible just for shits and giggles.
    """

    __slots__ = ('data')

    def __init__(self, s):
        r = []
        i = 0
        while i < len(s):
            c = s[i]
            if c == '\\':
                nc = s[i + 1]
                if nc == '%':
                    r.append('%')
                    i += 1
                elif nc == '\\':
                    r.append('\\')
                    i += 1
                else:
                    r.append(c)
            elif c == '%':
                self.data = (''.join(r), s[i+1:])
                return
            else:
                r.append(c)
            i += 1

        # This is different than (s,) because \% and \\ have been unescaped. Parsing patterns is
        # context-sensitive!
        self.data = (''.join(r),)

    def ismatchany(self):
        return self.data == ('','')

    def ispattern(self):
        return len(self.data) == 2

    def __hash__(self):
        return self.data.__hash__()

    def __eq__(self, o):
        assert isinstance(o, Pattern)
        return self.data == o.data

    def gettarget(self):
        assert not self.ispattern()
        return self.data[0]

    def hasslash(self):
        return self.data[0].find('/') != -1 or self.data[1].find('/') != -1

    def match(self, word):
        """
        Match this search pattern against a word (string).

        @returns None if the word doesn't match, or the matching stem.
                      If this is a %-less pattern, the stem will always be ''
        """
        d = self.data
        if len(d) == 1:
            if word == d[0]:
                return word
            return None

        d0, d1 = d
        l1 = len(d0)
        l2 = len(d1)
        if len(word) >= l1 + l2 and word.startswith(d0) and word.endswith(d1):
            if l2 == 0:
                return word[l1:]
            return word[l1:-l2]

        return None

    def resolve(self, dir, stem):
        if self.ispattern():
            return dir + self.data[0] + stem + self.data[1]

        return self.data[0]

    def subst(self, replacement, word, mustmatch):
        """
        Given a word, replace the current pattern with the replacement pattern, a la 'patsubst'

        @param mustmatch If true and this pattern doesn't match the word, throw a DataError. Otherwise
                         return word unchanged.
        """
        assert isinstance(replacement, str_type)

        stem = self.match(word)
        if stem is None:
            if mustmatch:
                raise DataError("target '%s' doesn't match pattern" % (word,))
            return word

        if not self.ispattern():
            # if we're not a pattern, the replacement is not parsed as a pattern either
            return replacement

        return Pattern(replacement).resolve('', stem)

    def __repr__(self):
        return "<Pattern with data %r>" % (self.data,)

    _backre = re.compile(r'[%\\]')
    def __str__(self):
        if not self.ispattern():
            return self._backre.sub(r'\\\1', self.data[0])

        return self._backre.sub(r'\\\1', self.data[0]) + '%' + self.data[1]

class RemakeTargetSerially(object):
    __slots__ = ('target', 'makefile', 'indent', 'rlist')

    def __init__(self, target, makefile, indent, rlist):
        self.target = target
        self.makefile = makefile
        self.indent = indent
        self.rlist = rlist
        self.commandscb(False)

    def resolvecb(self, error, didanything):
        assert error in (True, False)

        if didanything:
            self.target.didanything = True

        if error:
            self.target.error = True
            self.makefile.error = True
            if not self.makefile.keepgoing:
                self.target.notifydone(self.makefile)
                return
            else:
                # don't run the commands!
                del self.rlist[0]
                self.commandscb(error=False)
        else:
            self.rlist.pop(0).runcommands(self.indent, self.commandscb)

    def commandscb(self, error):
        assert error in (True, False)

        if error:
            self.target.error = True
            self.makefile.error = True

        if self.target.error and not self.makefile.keepgoing:
            self.target.notifydone(self.makefile)
            return

        if not len(self.rlist):
            self.target.notifydone(self.makefile)
        else:
            self.rlist[0].resolvedeps(True, self.resolvecb)

class RemakeTargetParallel(object):
    __slots__ = ('target', 'makefile', 'indent', 'rlist', 'rulesremaining', 'currunning')

    def __init__(self, target, makefile, indent, rlist):
        self.target = target
        self.makefile = makefile
        self.indent = indent
        self.rlist = rlist

        self.rulesremaining = len(rlist)
        self.currunning = False

        for r in rlist:
            makefile.context.defer(self.doresolve, r)

    def doresolve(self, r):
        if self.makefile.error and not self.makefile.keepgoing:
            r.error = True
            self.resolvecb(True, False)
        else:
            r.resolvedeps(False, self.resolvecb)

    def resolvecb(self, error, didanything):
        assert error in (True, False)

        if error:
            self.target.error = True

        if didanything:
            self.target.didanything = True

        self.rulesremaining -= 1

        # commandscb takes care of the details if we're currently building
        # something
        if self.currunning:
            return

        self.runnext()

    def runnext(self):
        assert not self.currunning

        if self.makefile.error and not self.makefile.keepgoing:
            self.rlist = []
        else:
            while len(self.rlist) and self.rlist[0].error:
                del self.rlist[0]

        if not len(self.rlist):
            if not self.rulesremaining:
                self.target.notifydone(self.makefile)
            return

        if self.rlist[0].depsremaining != 0:
            return

        self.currunning = True
        self.rlist.pop(0).runcommands(self.indent, self.commandscb)

    def commandscb(self, error):
        assert error in (True, False)
        if error:
            self.target.error = True
            self.makefile.error = True

        assert self.currunning
        self.currunning = False
        self.runnext()

class RemakeRuleContext(object):
    def __init__(self, target, makefile, rule, deps,
                 targetstack, avoidremakeloop):
        self.target = target
        self.makefile = makefile
        self.rule = rule
        self.deps = deps
        self.targetstack = targetstack
        self.avoidremakeloop = avoidremakeloop

        self.running = False
        self.error = False
        self.depsremaining = len(deps) + 1
        self.remake = False

    def resolvedeps(self, serial, cb):
        self.resolvecb = cb
        self.didanything = False
        if serial:
            self._resolvedepsserial()
        else:
            self._resolvedepsparallel()

    def _weakdepfinishedserial(self, error, didanything):
        if error:
            self.remake = True
        self._depfinishedserial(False, didanything)

    def _depfinishedserial(self, error, didanything):
        assert error in (True, False)

        if didanything:
            self.didanything = True

        if error:
            self.error = True
            if not self.makefile.keepgoing:
                self.resolvecb(error=True, didanything=self.didanything)
                return
        
        if len(self.resolvelist):
            dep, weak = self.resolvelist.pop(0)
            self.makefile.context.defer(dep.make,
                                        self.makefile, self.targetstack, weak and self._weakdepfinishedserial or self._depfinishedserial)
        else:
            self.resolvecb(error=self.error, didanything=self.didanything)

    def _resolvedepsserial(self):
        self.resolvelist = list(self.deps)
        self._depfinishedserial(False, False)

    def _startdepparallel(self, d):
        if self.makefile.error:
            depfinished(True, False)
        else:
            dep, weak = d
            dep.make(self.makefile, self.targetstack, weak and self._weakdepfinishedparallel or self._depfinishedparallel)

    def _weakdepfinishedparallel(self, error, didanything):
        if error:
            self.remake = True
        self._depfinishedparallel(False, didanything)

    def _depfinishedparallel(self, error, didanything):
        assert error in (True, False)

        if error:
            print "<%s>: Found error" % self.target.target
            self.error = True
        if didanything:
            self.didanything = True

        self.depsremaining -= 1
        if self.depsremaining == 0:
            self.resolvecb(error=self.error, didanything=self.didanything)

    def _resolvedepsparallel(self):
        self.depsremaining -= 1
        if self.depsremaining == 0:
            self.resolvecb(error=self.error, didanything=self.didanything)
            return

        self.didanything = False

        for d in self.deps:
            self.makefile.context.defer(self._startdepparallel, d)

    def _commandcb(self, error):
        assert error in (True, False)

        if error:
            self.runcb(error=True)
            return

        if len(self.commands):
            self.commands.pop(0)(self._commandcb)
        else:
            self.runcb(error=False)

    def runcommands(self, indent, cb):
        assert not self.running
        self.running = True

        self.runcb = cb

        if self.rule is None or not len(self.rule.commands):
            if self.target.mtime is None:
                self.target.beingremade()
            else:
                for d, weak in self.deps:
                    if mtimeislater(d.mtime, self.target.mtime):
                        if d.mtime is None:
                            self.target.beingremade()
                        else:
                            _log.info("%sNot remaking %s ubecause it would have no effect, even though %s is newer.", indent, self.target.target, d.target)
                        break
            cb(error=False)
            return

        if self.rule.doublecolon:
            if len(self.deps) == 0:
                if self.avoidremakeloop:
                    _log.info("%sNot remaking %s using rule at %s because it would introduce an infinite loop.", indent, self.target.target, self.rule.loc)
                    cb(error=False)
                    return

        remake = self.remake
        if remake:
            _log.info("%sRemaking %s using rule at %s: weak dependency was not found.", indent, self.target.target, self.rule.loc)
        else:
            if self.target.mtime is None:
                remake = True
                _log.info("%sRemaking %s using rule at %s: target doesn't exist or is a forced target", indent, self.target.target, self.rule.loc)

        if not remake:
            if self.rule.doublecolon:
                if len(self.deps) == 0:
                    _log.info("%sRemaking %s using rule at %s because there are no prerequisites listed for a double-colon rule.", indent, self.target.target, self.rule.loc)
                    remake = True

        if not remake:
            for d, weak in self.deps:
                if mtimeislater(d.mtime, self.target.mtime):
                    _log.info("%sRemaking %s using rule at %s because %s is newer.", indent, self.target.target, self.rule.loc, d.target)
                    remake = True
                    break

        if remake:
            self.target.beingremade()
            self.target.didanything = True
            try:
                self.commands = [c for c in self.rule.getcommands(self.target, self.makefile)]
            except util.MakeError, e:
                print e
                sys.stdout.flush()
                cb(error=True)
                return

            self._commandcb(False)
        else:
            cb(error=False)

MAKESTATE_NONE = 0
MAKESTATE_FINISHED = 1
MAKESTATE_WORKING = 2

class Target(object):
    """
    An actual (non-pattern) target.

    It holds target-specific variables and a list of rules. It may also point to a parent
    PatternTarget, if this target is being created by an implicit rule.

    The rules associated with this target may be Rule instances or, in the case of static pattern
    rules, PatternRule instances.
    """

    wasremade = False

    def __init__(self, target, makefile):
        assert isinstance(target, str_type)
        self.target = target
        self.vpathtarget = None
        self.rules = []
        self.variables = Variables(makefile.variables)
        self.explicit = False
        self._state = MAKESTATE_NONE

    def addrule(self, rule):
        assert isinstance(rule, (Rule, PatternRuleInstance))
        if len(self.rules) and rule.doublecolon != self.rules[0].doublecolon:
            raise DataError("Cannot have single- and double-colon rules for the same target. Prior rule location: %s" % self.rules[0].loc, rule.loc)

        if isinstance(rule, PatternRuleInstance):
            if len(rule.prule.targetpatterns) != 1:
                raise DataError("Static pattern rules must only have one target pattern", rule.prule.loc)
            if rule.prule.targetpatterns[0].match(self.target) is None:
                raise DataError("Static pattern rule doesn't match target '%s'" % self.target, rule.loc)

        self.rules.append(rule)

    def isdoublecolon(self):
        return self.rules[0].doublecolon

    def isphony(self, makefile):
        """Is this a phony target? We don't check for existence of phony targets."""
        return makefile.gettarget('.PHONY').hasdependency(self.target)

    def hasdependency(self, t):
        for rule in self.rules:
            if t in rule.prerequisites:
                return True

        return False

    def resolveimplicitrule(self, makefile, targetstack, rulestack):
        """
        Try to resolve an implicit rule to build this target.
        """
        # The steps in the GNU make manual Implicit-Rule-Search.html are very detailed. I hope they can be trusted.

        indent = getindent(targetstack)

        _log.info("%sSearching for implicit rule to make '%s'", indent, self.target)

        dir, s, file = util.strrpartition(self.target, '/')
        dir = dir + s

        candidates = [] # list of PatternRuleInstance

        hasmatch = util.any((r.hasspecificmatch(file) for r in makefile.implicitrules))

        for r in makefile.implicitrules:
            if r in rulestack:
                _log.info("%s %s: Avoiding implicit rule recursion", indent, r.loc)
                continue

            if not len(r.commands):
                continue

            for ri in r.matchesfor(dir, file, hasmatch):
                candidates.append(ri)
            
        newcandidates = []

        for r in candidates:
            depfailed = None
            for p in r.prerequisites:
                t = makefile.gettarget(p)
                t.resolvevpath(makefile)
                if not t.explicit and t.mtime is None:
                    depfailed = p
                    break

            if depfailed is not None:
                if r.doublecolon:
                    _log.info("%s Terminal rule at %s doesn't match: prerequisite '%s' not mentioned and doesn't exist.", indent, r.loc, depfailed)
                else:
                    newcandidates.append(r)
                continue

            _log.info("%sFound implicit rule at %s for target '%s'", indent, r.loc, self.target)
            self.rules.append(r)
            return

        # Try again, but this time with chaining and without terminal (double-colon) rules

        for r in newcandidates:
            newrulestack = rulestack + [r.prule]

            depfailed = None
            for p in r.prerequisites:
                t = makefile.gettarget(p)
                try:
                    t.resolvedeps(makefile, targetstack, newrulestack, True)
                except ResolutionError:
                    depfailed = p
                    break

            if depfailed is not None:
                _log.info("%s Rule at %s doesn't match: prerequisite '%s' could not be made.", indent, r.loc, depfailed)
                continue

            _log.info("%sFound implicit rule at %s for target '%s'", indent, r.loc, self.target)
            self.rules.append(r)
            return

        _log.info("%sCouldn't find implicit rule to remake '%s'", indent, self.target)

    def ruleswithcommands(self):
        "The number of rules with commands"
        return reduce(lambda i, rule: i + (len(rule.commands) > 0), self.rules, 0)

    def resolvedeps(self, makefile, targetstack, rulestack, recursive):
        """
        Resolve the actual path of this target, using vpath if necessary.

        Recursively resolve dependencies of this target. This means finding implicit
        rules which match the target, if appropriate.

        Figure out whether this target needs to be rebuild, and set self.outofdate
        appropriately.

        @param targetstack is the current stack of dependencies being resolved. If
               this target is already in targetstack, bail to prevent infinite
               recursion.
        @param rulestack is the current stack of implicit rules being used to resolve
               dependencies. A rule chain cannot use the same implicit rule twice.
        """
        assert makefile.parsingfinished

        if self.target in targetstack:
            raise ResolutionError("Recursive dependency: %s -> %s" % (
                    " -> ".join(targetstack), self.target))

        targetstack = targetstack + [self.target]
        
        indent = getindent(targetstack)

        _log.info("%sConsidering target '%s'", indent, self.target)

        self.resolvevpath(makefile)

        # Sanity-check our rules. If we're single-colon, only one rule should have commands
        ruleswithcommands = self.ruleswithcommands()
        if len(self.rules) and not self.isdoublecolon():
            if ruleswithcommands > 1:
                # In GNU make this is a warning, not an error. I'm going to be stricter.
                # TODO: provide locations
                raise DataError("Target '%s' has multiple rules with commands." % self.target)

        if ruleswithcommands == 0:
            self.resolveimplicitrule(makefile, targetstack, rulestack)

        # If a target is mentioned, but doesn't exist, has no commands and no
        # prerequisites, it is special and exists just to say that targets which
        # depend on it are always out of date. This is like .FORCE but more
        # compatible with other makes.
        # Otherwise, we don't know how to make it.
        if not len(self.rules) and self.mtime is None and not util.any((len(rule.prerequisites) > 0
                                                                        for rule in self.rules)):
            raise ResolutionError("No rule to make target '%s' needed by %r" % (self.target,
                                                                                targetstack))

        if recursive:
            for r in self.rules:
                newrulestack = rulestack + [r]
                for d in r.prerequisites:
                    dt = makefile.gettarget(d)
                    if dt.explicit:
                        continue

                    dt.resolvedeps(makefile, targetstack, newrulestack, True)

        for v in makefile.getpatternvariablesfor(self.target):
            self.variables.merge(v)

    def resolvevpath(self, makefile):
        if self.vpathtarget is not None:
            return

        if self.isphony(makefile):
            self.vpathtarget = self.target
            self.mtime = None
            return

        if self.target.startswith('-l'):
            stem = self.target[2:]
            f, s, e = makefile.variables.get('.LIBPATTERNS')
            if e is not None:
                libpatterns = [Pattern(stripdotslash(s)) for s in e.resolvesplit(makefile, makefile.variables)]
                if len(libpatterns):
                    searchdirs = ['']
                    searchdirs.extend(makefile.getvpath(self.target))

                    for lp in libpatterns:
                        if not lp.ispattern():
                            raise DataError('.LIBPATTERNS contains a non-pattern')

                        libname = lp.resolve('', stem)

                        for dir in searchdirs:
                            libpath = util.normaljoin(dir, libname).replace('\\', '/')
                            fspath = util.normaljoin(makefile.workdir, libpath)
                            mtime = getmtime(fspath)
                            if mtime is not None:
                                self.vpathtarget = libpath
                                self.mtime = mtime
                                return

                    self.vpathtarget = self.target
                    self.mtime = None
                    return

        search = [self.target]
        if not os.path.isabs(self.target):
            search += [util.normaljoin(dir, self.target).replace('\\', '/')
                       for dir in makefile.getvpath(self.target)]

        targetandtime = self.searchinlocs(makefile, search)
        if targetandtime is not None:
            (self.vpathtarget, self.mtime) = targetandtime
            return

        self.vpathtarget = self.target
        self.mtime = None

    def searchinlocs(self, makefile, locs):
        """
        Look in the given locations relative to the makefile working directory
        for a file. Return a pair of the target and the mtime if found, None
        if not.
        """
        for t in locs:
            fspath = util.normaljoin(makefile.workdir, t).replace('\\', '/')
            mtime = getmtime(fspath)
#            _log.info("Searching %s ... checking %s ... mtime %r" % (t, fspath, mtime))
            if mtime is not None:
                return (t, mtime)

        return None
        
    def beingremade(self):
        """
        When we remake ourself, we have to drop any vpath prefixes.
        """
        self.vpathtarget = self.target
        self.wasremade = True

    def notifydone(self, makefile):
        assert self._state == MAKESTATE_WORKING, "State was %s" % self._state
        # If we were remade then resolve mtime again
        if self.wasremade:
            targetandtime = self.searchinlocs(makefile, [self.target])
            if targetandtime is not None:
                (_, self.mtime) = targetandtime
            else:
                self.mtime = None

        self._state = MAKESTATE_FINISHED
        for cb in self._callbacks:
            makefile.context.defer(cb, error=self.error, didanything=self.didanything)
        del self._callbacks 

    def make(self, makefile, targetstack, cb, avoidremakeloop=False, printerror=True):
        """
        If we are out of date, asynchronously make ourself. This is a multi-stage process, mostly handled
        by the helper objects RemakeTargetSerially, RemakeTargetParallel,
        RemakeRuleContext. These helper objects should keep us from developing
        any cyclical dependencies.

        * resolve dependencies (synchronous)
        * gather a list of rules to execute and related dependencies (synchronous)
        * for each rule (in parallel)
        ** remake dependencies (asynchronous)
        ** build list of commands to execute (synchronous)
        ** execute each command (asynchronous)
        * asynchronously notify when all rules are complete

        @param cb A callback function to notify when remaking is finished. It is called
               thusly: callback(error=True/False, didanything=True/False)
               If there is no asynchronous activity to perform, the callback may be called directly.
        """

        serial = makefile.context.jcount == 1
        
        if self._state == MAKESTATE_FINISHED:
            cb(error=self.error, didanything=self.didanything)
            return
            
        if self._state == MAKESTATE_WORKING:
            assert not serial
            self._callbacks.append(cb)
            return

        assert self._state == MAKESTATE_NONE

        self._state = MAKESTATE_WORKING
        self._callbacks = [cb]
        self.error = False
        self.didanything = False

        indent = getindent(targetstack)

        try:
            self.resolvedeps(makefile, targetstack, [], False)
        except util.MakeError, e:
            if printerror:
                print e
            self.error = True
            self.notifydone(makefile)
            return

        assert self.vpathtarget is not None, "Target was never resolved!"
        if not len(self.rules):
            self.notifydone(makefile)
            return

        if self.isdoublecolon():
            rulelist = [RemakeRuleContext(self, makefile, r, [(makefile.gettarget(p), False) for p in r.prerequisites], targetstack, avoidremakeloop) for r in self.rules]
        else:
            alldeps = []

            commandrule = None
            for r in self.rules:
                rdeps = [(makefile.gettarget(p), r.weakdeps) for p in r.prerequisites]
                if len(r.commands):
                    assert commandrule is None
                    commandrule = r
                    # The dependencies of the command rule are resolved before other dependencies,
                    # no matter the ordering of the other no-command rules
                    alldeps[0:0] = rdeps
                else:
                    alldeps.extend(rdeps)

            rulelist = [RemakeRuleContext(self, makefile, commandrule, alldeps, targetstack, avoidremakeloop)]

        targetstack = targetstack + [self.target]

        if serial:
            RemakeTargetSerially(self, makefile, indent, rulelist)
        else:
            RemakeTargetParallel(self, makefile, indent, rulelist)

def dirpart(p):
    d, s, f = util.strrpartition(p, '/')
    if d == '':
        return '.'

    return d

def filepart(p):
    d, s, f = util.strrpartition(p, '/')
    return f

def setautomatic(v, name, plist):
    v.set(name, Variables.FLAVOR_SIMPLE, Variables.SOURCE_AUTOMATIC, ' '.join(plist))
    v.set(name + 'D', Variables.FLAVOR_SIMPLE, Variables.SOURCE_AUTOMATIC, ' '.join((dirpart(p) for p in plist)))
    v.set(name + 'F', Variables.FLAVOR_SIMPLE, Variables.SOURCE_AUTOMATIC, ' '.join((filepart(p) for p in plist)))

def setautomaticvariables(v, makefile, target, prerequisites):
    prtargets = [makefile.gettarget(p) for p in prerequisites]
    prall = [pt.vpathtarget for pt in prtargets]
    proutofdate = [pt.vpathtarget for pt in withoutdups(prtargets)
                   if target.mtime is None or mtimeislater(pt.mtime, target.mtime)]
    
    setautomatic(v, '@', [target.vpathtarget])
    if len(prall):
        setautomatic(v, '<', [prall[0]])

    setautomatic(v, '?', proutofdate)
    setautomatic(v, '^', list(withoutdups(prall)))
    setautomatic(v, '+', prall)

def splitcommand(command):
    """
    Using the esoteric rules, split command lines by unescaped newlines.
    """
    start = 0
    i = 0
    while i < len(command):
        c = command[i]
        if c == '\\':
            i += 1
        elif c == '\n':
            yield command[start:i]
            i += 1
            start = i
            continue

        i += 1

    if i > start:
        yield command[start:i]

def findmodifiers(command):
    """
    Find any of +-@% prefixed on the command.
    @returns (command, isHidden, isRecursive, ignoreErrors, isNative)
    """

    isHidden = False
    isRecursive = False
    ignoreErrors = False
    isNative = False

    realcommand = command.lstrip(' \t\n@+-%')
    modset = set(command[:-len(realcommand)])
    return realcommand, '@' in modset, '+' in modset, '-' in modset, '%' in modset

class _CommandWrapper(object):
    def __init__(self, cline, ignoreErrors, loc, context, **kwargs):
        self.ignoreErrors = ignoreErrors
        self.loc = loc
        self.cline = cline
        self.kwargs = kwargs
        self.context = context

    def _cb(self, res):
        if res != 0 and not self.ignoreErrors:
            print "%s: command '%s' failed, return code %i" % (self.loc, self.cline, res)
            self.usercb(error=True)
        else:
            self.usercb(error=False)

    def __call__(self, cb):
        self.usercb = cb
        process.call(self.cline, loc=self.loc, cb=self._cb, context=self.context, **self.kwargs)

class _NativeWrapper(_CommandWrapper):
    def __init__(self, cline, ignoreErrors, loc, context,
                 pycommandpath, **kwargs):
        _CommandWrapper.__init__(self, cline, ignoreErrors, loc, context,
                                 **kwargs)
        if pycommandpath:
            self.pycommandpath = re.split('[%s\s]+' % os.pathsep,
                                          pycommandpath)
        else:
            self.pycommandpath = None

    def __call__(self, cb):
        # get the module and method to call
        parts, badchar = process.clinetoargv(self.cline, self.kwargs['cwd'])
        if parts is None:
            raise DataError("native command '%s': shell metacharacter '%s' in command line" % (self.cline, badchar), self.loc)
        if len(parts) < 2:
            raise DataError("native command '%s': no method name specified" % self.cline, self.loc)
        module = parts[0]
        method = parts[1]
        cline_list = parts[2:]
        self.usercb = cb
        process.call_native(module, method, cline_list,
                            loc=self.loc, cb=self._cb, context=self.context,
                            pycommandpath=self.pycommandpath, **self.kwargs)

def getcommandsforrule(rule, target, makefile, prerequisites, stem):
    v = Variables(parent=target.variables)
    setautomaticvariables(v, makefile, target, prerequisites)
    if stem is not None:
        setautomatic(v, '*', [stem])

    env = makefile.getsubenvironment(v)

    for c in rule.commands:
        cstring = c.resolvestr(makefile, v)
        for cline in splitcommand(cstring):
            cline, isHidden, isRecursive, ignoreErrors, isNative = findmodifiers(cline)
            if (isHidden or makefile.silent) and not makefile.justprint:
                echo = None
            else:
                echo = "%s$ %s" % (c.loc, cline)
            if not isNative:
                yield _CommandWrapper(cline, ignoreErrors=ignoreErrors, env=env, cwd=makefile.workdir, loc=c.loc, context=makefile.context,
                                      echo=echo, justprint=makefile.justprint)
            else:
                f, s, e = v.get("PYCOMMANDPATH", True)
                if e:
                    e = e.resolvestr(makefile, v, ["PYCOMMANDPATH"])
                yield _NativeWrapper(cline, ignoreErrors=ignoreErrors,
                                     env=env, cwd=makefile.workdir,
                                     loc=c.loc, context=makefile.context,
                                     echo=echo, justprint=makefile.justprint,
                                     pycommandpath=e)

class Rule(object):
    """
    A rule contains a list of prerequisites and a list of commands. It may also
    contain rule-specific variables. This rule may be associated with multiple targets.
    """

    def __init__(self, prereqs, doublecolon, loc, weakdeps):
        self.prerequisites = prereqs
        self.doublecolon = doublecolon
        self.commands = []
        self.loc = loc
        self.weakdeps = weakdeps

    def addcommand(self, c):
        assert isinstance(c, (Expansion, StringExpansion))
        self.commands.append(c)

    def getcommands(self, target, makefile):
        assert isinstance(target, Target)

        return getcommandsforrule(self, target, makefile, self.prerequisites, stem=None)
        # TODO: $* in non-pattern rules?

class PatternRuleInstance(object):
    weakdeps = False

    """
    A pattern rule instantiated for a particular target. It has the same API as Rule, but
    different internals, forwarding most information on to the PatternRule.
    """
    def __init__(self, prule, dir, stem, ismatchany):
        assert isinstance(prule, PatternRule)

        self.dir = dir
        self.stem = stem
        self.prule = prule
        self.prerequisites = prule.prerequisitesforstem(dir, stem)
        self.doublecolon = prule.doublecolon
        self.loc = prule.loc
        self.ismatchany = ismatchany
        self.commands = prule.commands

    def getcommands(self, target, makefile):
        assert isinstance(target, Target)
        return getcommandsforrule(self, target, makefile, self.prerequisites, stem=self.dir + self.stem)

    def __str__(self):
        return "Pattern rule at %s with stem '%s', matchany: %s doublecolon: %s" % (self.loc,
                                                                                    self.dir + self.stem,
                                                                                    self.ismatchany,
                                                                                    self.doublecolon)

class PatternRule(object):
    """
    An implicit rule or static pattern rule containing target patterns, prerequisite patterns,
    and a list of commands.
    """

    def __init__(self, targetpatterns, prerequisites, doublecolon, loc):
        self.targetpatterns = targetpatterns
        self.prerequisites = prerequisites
        self.doublecolon = doublecolon
        self.loc = loc
        self.commands = []

    def addcommand(self, c):
        assert isinstance(c, (Expansion, StringExpansion))
        self.commands.append(c)

    def ismatchany(self):
        return util.any((t.ismatchany() for t in self.targetpatterns))

    def hasspecificmatch(self, file):
        for p in self.targetpatterns:
            if not p.ismatchany() and p.match(file) is not None:
                return True

        return False

    def matchesfor(self, dir, file, skipsinglecolonmatchany):
        """
        Determine all the target patterns of this rule that might match target t.
        @yields a PatternRuleInstance for each.
        """

        for p in self.targetpatterns:
            matchany = p.ismatchany()
            if matchany:
                if skipsinglecolonmatchany and not self.doublecolon:
                    continue

                yield PatternRuleInstance(self, dir, file, True)
            else:
                stem = p.match(dir + file)
                if stem is not None:
                    yield PatternRuleInstance(self, '', stem, False)
                else:
                    stem = p.match(file)
                    if stem is not None:
                        yield PatternRuleInstance(self, dir, stem, False)

    def prerequisitesforstem(self, dir, stem):
        return [p.resolve(dir, stem) for p in self.prerequisites]

class _RemakeContext(object):
    def __init__(self, makefile, cb):
        self.makefile = makefile
        self.included = [(makefile.gettarget(f), required)
                         for f, required in makefile.included]
        self.toremake = list(self.included)
        self.cb = cb

        self.remakecb(error=False, didanything=False)

    def remakecb(self, error, didanything):
        assert error in (True, False)

        if error and self.required:
            print "Error remaking makefiles (ignored)"

        if len(self.toremake):
            target, self.required = self.toremake.pop(0)
            target.make(self.makefile, [], avoidremakeloop=True, cb=self.remakecb, printerror=False)
        else:
            for t, required in self.included:
                if t.wasremade:
                    _log.info("Included file %s was remade, restarting make", t.target)
                    self.cb(remade=True)
                    return
                elif required and t.mtime is None:
                    self.cb(remade=False, error=DataError("No rule to remake missing include file %s" % t.target))
                    return

            self.cb(remade=False)

class Makefile(object):
    """
    The top-level data structure for makefile execution. It holds Targets, implicit rules, and other
    state data.
    """

    def __init__(self, workdir=None, env=None, restarts=0, make=None,
                 makeflags='', makeoverrides='',
                 makelevel=0, context=None, targets=(), keepgoing=False,
                 silent=False, justprint=False):
        self.defaulttarget = None

        if env is None:
            env = os.environ
        self.env = env

        self.variables = Variables()
        self.variables.readfromenvironment(env)

        self.context = context
        self.exportedvars = {}
        self._targets = {}
        self.keepgoing = keepgoing
        self.silent = silent
        self.justprint = justprint
        self._patternvariables = [] # of (pattern, variables)
        self.implicitrules = []
        self.parsingfinished = False

        self._patternvpaths = [] # of (pattern, [dir, ...])

        if workdir is None:
            workdir = os.getcwd()
        workdir = os.path.realpath(workdir)
        self.workdir = workdir
        self.variables.set('CURDIR', Variables.FLAVOR_SIMPLE,
                           Variables.SOURCE_AUTOMATIC, workdir.replace('\\','/'))

        # the list of included makefiles, whether or not they existed
        self.included = []

        self.variables.set('MAKE_RESTARTS', Variables.FLAVOR_SIMPLE,
                           Variables.SOURCE_AUTOMATIC, restarts > 0 and str(restarts) or '')

        self.variables.set('.PYMAKE', Variables.FLAVOR_SIMPLE,
                           Variables.SOURCE_MAKEFILE, "1")
        if make is not None:
            self.variables.set('MAKE', Variables.FLAVOR_SIMPLE,
                               Variables.SOURCE_MAKEFILE, make)

        if makeoverrides != '':
            self.variables.set('-*-command-variables-*-', Variables.FLAVOR_SIMPLE,
                               Variables.SOURCE_AUTOMATIC, makeoverrides)
            makeflags += ' -- $(MAKEOVERRIDES)'

        self.variables.set('MAKEOVERRIDES', Variables.FLAVOR_RECURSIVE,
                           Variables.SOURCE_ENVIRONMENT,
                           '${-*-command-variables-*-}')

        self.variables.set('MAKEFLAGS', Variables.FLAVOR_RECURSIVE,
                           Variables.SOURCE_MAKEFILE, makeflags)
        self.exportedvars['MAKEFLAGS'] = True

        self.makelevel = makelevel
        self.variables.set('MAKELEVEL', Variables.FLAVOR_SIMPLE,
                           Variables.SOURCE_MAKEFILE, str(makelevel))

        self.variables.set('MAKECMDGOALS', Variables.FLAVOR_SIMPLE,
                           Variables.SOURCE_AUTOMATIC, ' '.join(targets))

        for vname, val in implicit.variables.iteritems():
            self.variables.set(vname,
                               Variables.FLAVOR_SIMPLE,
                               Variables.SOURCE_IMPLICIT, val)

    def foundtarget(self, t):
        """
        Inform the makefile of a target which is a candidate for being the default target,
        if there isn't already a default target.
        """
        if self.defaulttarget is None and t != '.PHONY':
            self.defaulttarget = t

    def getpatternvariables(self, pattern):
        assert isinstance(pattern, Pattern)

        for p, v in self._patternvariables:
            if p == pattern:
                return v

        v = Variables()
        self._patternvariables.append( (pattern, v) )
        return v

    def getpatternvariablesfor(self, target):
        for p, v in self._patternvariables:
            if p.match(target):
                yield v

    def hastarget(self, target):
        return target in self._targets

    def gettarget(self, target):
        assert isinstance(target, str_type)

        target = target.rstrip('/')

        assert target != '', "empty target?"

        if target.find('*') != -1 or target.find('?') != -1 or target.find('[') != -1:
            raise DataError("wildcards should have been expanded by the parser: '%s'" % (target,))

        t = self._targets.get(target, None)
        if t is None:
            t = Target(target, self)
            self._targets[target] = t
        return t

    def appendimplicitrule(self, rule):
        assert isinstance(rule, PatternRule)
        self.implicitrules.append(rule)

    def finishparsing(self):
        """
        Various activities, such as "eval", are not allowed after parsing is
        finished. In addition, various warnings and errors can only be issued
        after the parsing data model is complete. All dependency resolution
        and rule execution requires that parsing be finished.
        """
        self.parsingfinished = True

        flavor, source, value = self.variables.get('GPATH')
        if value is not None and value.resolvestr(self, self.variables, ['GPATH']).strip() != '':
            raise DataError('GPATH was set: pymake does not support GPATH semantics')

        flavor, source, value = self.variables.get('VPATH')
        if value is None:
            self._vpath = []
        else:
            self._vpath = filter(lambda e: e != '',
                                 re.split('[%s\s]+' % os.pathsep,
                                          value.resolvestr(self, self.variables, ['VPATH'])))

        targets = list(self._targets.itervalues())
        for t in targets:
            t.explicit = True
            for r in t.rules:
                for p in r.prerequisites:
                    self.gettarget(p).explicit = True

        np = self.gettarget('.NOTPARALLEL')
        if len(np.rules):
            self.context = process.getcontext(1)

        flavor, source, value = self.variables.get('.DEFAULT_GOAL')
        if value is not None:
            self.defaulttarget = value.resolvestr(self, self.variables, ['.DEFAULT_GOAL']).strip()

        self.error = False

    def include(self, path, required=True, weak=False, loc=None):
        """
        Include the makefile at `path`.
        """
        self.included.append((path, required))
        fspath = util.normaljoin(self.workdir, path)
        if os.path.exists(fspath):
            stmts = parser.parsefile(fspath)
            self.variables.append('MAKEFILE_LIST', Variables.SOURCE_AUTOMATIC, path, None, self)
            stmts.execute(self, weak=weak)
            self.gettarget(path).explicit = True

    def addvpath(self, pattern, dirs):
        """
        Add a directory to the vpath search for the given pattern.
        """
        self._patternvpaths.append((pattern, dirs))

    def clearvpath(self, pattern):
        """
        Clear vpaths for the given pattern.
        """
        self._patternvpaths = [(p, dirs)
                               for p, dirs in self._patternvpaths
                               if not p.match(pattern)]

    def clearallvpaths(self):
        self._patternvpaths = []

    def getvpath(self, target):
        vp = list(self._vpath)
        for p, dirs in self._patternvpaths:
            if p.match(target):
                vp.extend(dirs)

        return withoutdups(vp)

    def remakemakefiles(self, cb):
        mlist = []
        for f, required in self.included:
            t = self.gettarget(f)
            t.explicit = True
            t.resolvevpath(self)
            oldmtime = t.mtime

            mlist.append((t, oldmtime))

        _RemakeContext(self, cb)

    def getsubenvironment(self, variables):
        env = dict(self.env)
        for vname, v in self.exportedvars.iteritems():
            if v:
                flavor, source, val = variables.get(vname)
                if val is None:
                    strval = ''
                else:
                    strval = val.resolvestr(self, variables, [vname])
                env[vname] = strval
            else:
                env.pop(vname, None)

        makeflags = ''

        env['MAKELEVEL'] = str(self.makelevel + 1)
        return env
