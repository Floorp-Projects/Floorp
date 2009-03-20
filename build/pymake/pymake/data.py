"""
A representation of makefile data structures.
"""

import logging, re, os
import parserdata, parser, functions, process, util, builtins
from cStringIO import StringIO

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
        return s[2:]
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

class StringExpansion(object):
    __slots__ = ('s',)
    loc = None
    simple = True
    
    def __init__(self, s):
        assert isinstance(s, str)
        self.s = s

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
        e = Expansion()
        e.appendstr(self.s)
        return e

    def __len__(self):
        return 1

    def __getitem__(self, i):
        assert i == 0
        return self.s, False

class Expansion(list):
    """
    A representation of expanded data, such as that for a recursively-expanded variable, a command, etc.
    """

    __slots__ = ('loc', 'hasfunc')
    simple = False

    def __init__(self, loc=None):
        # A list of (element, isfunc) tuples
        # element is either a string or a function
        self.loc = loc
        self.hasfunc = False

    @staticmethod
    def fromstring(s):
        return StringExpansion(s)

    def clone(self):
        e = Expansion()
        e.extend(self)
        return e

    def appendstr(self, s):
        assert isinstance(s, str)
        if s == '':
            return

        self.append((s, False))

    def appendfunc(self, func):
        assert isinstance(func, functions.Function)
        self.append((func, True))
        self.hasfunc = True

    def concat(self, o):
        """Concatenate the other expansion on to this one."""
        if o.simple:
            self.appendstr(o.s)
        else:
            self.extend(o)
            self.hasfunc = self.hasfunc or o.hasfunc

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
        if self.hasfunc:
            return self

        return StringExpansion(''.join([i for i, isfunc in self]))

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
                assert isinstance(e, str)
                fd.write(e)
                    
    def resolvestr(self, makefile, variables, setting=[]):
        fd = StringIO()
        self.resolve(makefile, variables, fd, setting)
        return fd.getvalue()

    def resolvesplit(self, makefile, variables, setting=[]):
        return self.resolvestr(makefile, variables, setting).split()

    def __repr__(self):
        return "<Expansion with elements: %r>" % ([e for e, isfunc in self],)

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
            self.set(k, self.FLAVOR_SIMPLE, self.SOURCE_ENVIRONMENT, v)

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
                val = Expansion.fromstring(valuestr)

            return flavor, source, val

        if self.parent is not None:
            return self.parent.get(name, expand)

        return (None, None, None)

    def set(self, name, flavor, source, value):
        assert flavor in (self.FLAVOR_RECURSIVE, self.FLAVOR_SIMPLE)
        assert source in (self.SOURCE_OVERRIDE, self.SOURCE_COMMANDLINE, self.SOURCE_MAKEFILE, self.SOURCE_ENVIRONMENT, self.SOURCE_AUTOMATIC, self.SOURCE_IMPLICIT)
        assert isinstance(value, str), "expected str, got %s" % type(value)

        prevflavor, prevsource, prevvalue = self.get(name)
        if prevsource is not None and source > prevsource:
            # TODO: give a location for this warning
            _log.info("not setting variable '%s', set by higher-priority source to value '%s'" % (name, prevvalue))
            return

        self._map[name] = flavor, source, value, None

    def append(self, name, source, value, variables, makefile):
        assert source in (self.SOURCE_OVERRIDE, self.SOURCE_MAKEFILE, self.SOURCE_AUTOMATIC)
        assert isinstance(value, str)

        def expand():
            try:
                d = parser.Data.fromstring(value, parserdata.Location("Expansion of variable '%s'" % (name,), 1, 0))
                valueexp, t, o = parser.parsemakesyntax(d, 0, (), parser.iterdata)
                return valueexp, None
            except parser.SyntaxError, e:
                return None, e
        
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
        assert isinstance(replacement, str)

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
        if not self.ispattern:
            return self._backre.sub(r'\\\1', self.data[0])

        return self._backre.sub(r'\\\1', self.data[0]) + '%' + self.data[1]

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

    def __init__(self, target, makefile):
        assert isinstance(target, str)
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
        phony = makefile.gettarget('.PHONY').hasdependency(self.target)

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
                            libpath = os.path.join(dir, libname).replace('\\', '/')
                            fspath = os.path.join(makefile.workdir, libpath)
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
            search += [os.path.join(dir, self.target).replace('\\', '/')
                       for dir in makefile.getvpath(self.target)]

        for t in search:
            fspath = os.path.join(makefile.workdir, t).replace('\\', '/')
            mtime = getmtime(fspath)
            if mtime is not None:
                self.vpathtarget = t
                self.mtime = mtime
                return

        self.vpathtarget = self.target
        self.mtime = None
        
    def _beingremade(self):
        """
        When we remake ourself, we need to reset our mtime and vpathtarget.

        We store our old mtime so that $? can calculate out-of-date prerequisites.
        """
        self.realmtime = self.mtime
        self.mtime = None
        self.vpathtarget = self.target

    def _notifydone(self, makefile):
        assert self._state == MAKESTATE_WORKING

        self._state = MAKESTATE_FINISHED
        for cb in self._callbacks:
            makefile.context.defer(cb, error=self._makeerror, didanything=self._didanything)
        del self._callbacks 

    def make(self, makefile, targetstack, rulestack, cb, avoidremakeloop=False):
        """
        If we are out of date, asynchronously make ourself. This is a multi-stage process, mostly handled
        by enclosed functions:

        * resolve dependencies (synchronous)
        * gather a list of rules to execute and related dependencies (synchronous)
        * for each rule (rulestart)
        ** remake dependencies (asynchronous, toplevel, callback to start each dependency is `depstart`,
           callback when each is finished is `depfinished``
        ** build list of commands to execute (synchronous, in `runcommands`)
        ** execute each command (asynchronous, runcommands.commandcb)
        * asynchronously notify rulefinished when each rule is complete

        @param cb A callback function to notify when remaking is finished. It is called
               thusly: callback(error=exception/None, didanything=True/False/None)
               If there is no asynchronous activity to perform, the callback may be called directly.
        """

        serial = makefile.context.jcount == 1
        
        if self._state == MAKESTATE_FINISHED:
            cb(error=self._makeerror, didanything=self._didanything)
            return
            
        if self._state == MAKESTATE_WORKING:
            assert not serial
            self._callbacks.append(cb)
            return

        assert self._state == MAKESTATE_NONE

        self._state = MAKESTATE_WORKING
        self._callbacks = [cb]
        self._makeerror = None
        self._didanything = False

        indent = getindent(targetstack)

        if serial:
            def rulefinished():
                if self._makeerror is not None:
                    self._notifydone(makefile)
                    return

                if len(rulelist):
                    rule, deps = rulelist.pop(0)
                    makefile.context.defer(startrule, rule, deps)
                else:
                    self._notifydone(makefile)
        else:
            makeclosure = util.makeobject(('rulesremaining',))
            def rulefinished():
                makeclosure.rulesremaining -= 1
                if makeclosure.rulesremaining == 0:
                    self._notifydone(makefile)

        def startrule(r, deps):
            if serial:
                def depfinished(error, didanything):
                    if error is not None:
                        self._makeerror = error
                        self._notifydone(makefile)
                        return

                    self._didanything = didanything or self._didanything

                    if len(deplist):
                        dep = deplist.pop(0)
                        makefile.context.defer(startdep, dep)
                    else:
                        runcommands()
            else:
                ruleclosure = util.makeobject(('depsremaining',), depsremaining=len(deps))
                def depfinished(error, didanything):
                    if error is not None:
                        self._makeerror = error
                    else:
                        self._didanything = didanything or self._didanything

                    ruleclosure.depsremaining -= 1
                    if ruleclosure.depsremaining == 0:
                        if self._makeerror is not None:
                            rulefinished()
                        else:
                            runcommands()

            def startdep(dep):
                if self._makeerror is not None:
                    depfinished(None, False)
                    return

                dep.make(makefile, targetstack, [], cb=depfinished)

            def runcommands():
                """
                Asynchronous dependency-making is finished. Now run our commands (if any).
                """

                if r is None or not len(r.commands):
                    if self.mtime is None:
                        self._beingremade()
                    else:
                        for d in deps:
                            if mtimeislater(d.mtime, self.mtime):
                                self._beingremade()
                                break
                    rulefinished()
                    return

                def commandcb(error):
                    if error is not None:
                        self._makeerror = error
                        rulefinished()
                        return

                    if len(commands):
                        commands.pop(0)(commandcb)
                    else:
                        rulefinished()

                remake = False
                if self.mtime is None:
                    remake = True
                    _log.info("%sRemaking %s using rule at %s: target doesn't exist or is a forced target", indent, self.target, r.loc)

                if not remake:
                    if r.doublecolon:
                        if len(deps) == 0:
                            if avoidremakeloop:
                                _log.info("%sNot remaking %s using rule at %s because it would introduce an infinite loop.", indent, self.target, r.loc)
                            else:
                                _log.info("%sRemaking %s using rule at %s because there are no prerequisites listed for a double-colon rule.", indent, self.target, r.loc)
                                remake = True

                if not remake:
                    for d in deps:
                        if mtimeislater(d.mtime, self.mtime):
                            _log.info("%sRemaking %s using rule at %s because %s is newer.", indent, self.target, r.loc, d.target)
                            remake = True
                            break

                if remake:
                    self._beingremade()
                    self._didanything = True
                    try:
                        commands = [c for c in r.getcommands(self, makefile)]
                    except util.MakeError, e:
                        self._makeerror = e
                        rulefinished()
                        return
                    commandcb(None)
                else:
                    rulefinished()

            if not len(deps):
                runcommands()
            else:
                if serial:
                    deplist = list(deps)
                    startdep(deplist.pop(0))
                else:
                    ruleclosure.depsremaining = len(deps)
                    for d in deps:
                        makefile.context.defer(startdep, d)

        try:
            self.resolvedeps(makefile, targetstack, rulestack, False)
        except util.MakeError, e:
            self._makeerror = e
            self._notifydone(makefile)
            return

        assert self.vpathtarget is not None, "Target was never resolved!"
        if not len(self.rules):
            self._notifydone(makefile)
            return

        if self.isdoublecolon():
            rulelist = [(r, [makefile.gettarget(p) for p in r.prerequisites]) for r in self.rules]
        else:
            alldeps = []

            commandrule = None
            for r in self.rules:
                rdeps = [makefile.gettarget(p) for p in r.prerequisites]
                if len(r.commands):
                    assert commandrule is None
                    commandrule = r
                    # The dependencies of the command rule are resolved before other dependencies,
                    # no matter the ordering of the other no-command rules
                    alldeps[0:0] = rdeps
                else:
                    alldeps.extend(rdeps)

            rulelist = [(commandrule, alldeps)]

        targetstack = targetstack + [self.target]

        if serial:
            rulefinished()
        else:
            makeclosure.rulesremaining = len(rulelist)
            for r, deps in rulelist:
                makefile.context.defer(startrule, r, deps)

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
                   if target.realmtime is None or mtimeislater(pt.mtime, target.realmtime)]
    
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
    Find any of +-@ prefixed on the command.
    @returns (command, isHidden, isRecursive, ignoreErrors)
    """

    isHidden = False
    isRecursive = False
    ignoreErrors = False

    realcommand = command.lstrip(' \t\n@+-')
    modset = set(command[:-len(realcommand)])
    return realcommand, '@' in modset, '+' in modset, '-' in modset

class _CommandWrapper(object):
    def __init__(self, cline, ignoreErrors, loc, context, **kwargs):
        self.ignoreErrors = ignoreErrors
        self.loc = loc
        self.cline = cline
        self.kwargs = kwargs
        self.context = context

    def _cb(self, res):
        if res != 0 and not self.ignoreErrors:
            self.usercb(error=DataError("command '%s' failed, return code %s" % (self.cline, res), self.loc))
        else:
            self.usercb(error=None)

    def __call__(self, cb):
        self.usercb = cb
        process.call(self.cline, loc=self.loc, cb=self._cb, context=self.context, **self.kwargs)

def getcommandsforrule(rule, target, makefile, prerequisites, stem):
    v = Variables(parent=target.variables)
    setautomaticvariables(v, makefile, target, prerequisites)
    if stem is not None:
        setautomatic(v, '*', [stem])

    env = makefile.getsubenvironment(v)

    for c in rule.commands:
        cstring = c.resolvestr(makefile, v)
        for cline in splitcommand(cstring):
            cline, isHidden, isRecursive, ignoreErrors = findmodifiers(cline)
            if isHidden:
                echo = None
            else:
                echo = "%s$ %s" % (c.loc, cline)
            yield _CommandWrapper(cline, ignoreErrors=ignoreErrors, env=env, cwd=makefile.workdir, loc=c.loc, context=makefile.context,
                                 echo=echo)

class Rule(object):
    """
    A rule contains a list of prerequisites and a list of commands. It may also
    contain rule-specific variables. This rule may be associated with multiple targets.
    """

    def __init__(self, prereqs, doublecolon, loc):
        self.prerequisites = prereqs
        self.doublecolon = doublecolon
        self.commands = []
        self.loc = loc

    def addcommand(self, c):
        assert isinstance(c, (Expansion, StringExpansion))
        self.commands.append(c)

    def getcommands(self, target, makefile):
        assert isinstance(target, Target)

        return getcommandsforrule(self, target, makefile, self.prerequisites, stem=None)
        # TODO: $* in non-pattern rules?

class PatternRuleInstance(object):
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

class Makefile(object):
    """
    The top-level data structure for makefile execution. It holds Targets, implicit rules, and other
    state data.
    """

    def __init__(self, workdir=None, env=None, restarts=0, make=None, makeflags=None, makelevel=0, context=None):
        self.defaulttarget = None

        if env is None:
            env = os.environ
        self.env = env

        self.variables = Variables()
        self.variables.readfromenvironment(env)

        self.context = context
        self.exportedvars = set()
        self.overrides = []
        self._targets = {}
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

        if make is not None:
            self.variables.set('MAKE', Variables.FLAVOR_SIMPLE,
                               Variables.SOURCE_MAKEFILE, make)

        if makeflags is not None:
            self.variables.set('MAKEFLAGS', Variables.FLAVOR_SIMPLE,
                               Variables.SOURCE_MAKEFILE, makeflags)

        self.makelevel = makelevel
        self.variables.set('MAKELEVEL', Variables.FLAVOR_SIMPLE,
                           Variables.SOURCE_MAKEFILE, str(makelevel))

        for vname, val in builtins.variables.iteritems():
            self.variables.set(vname, Variables.FLAVOR_SIMPLE,
                               Variables.SOURCE_IMPLICIT, val)

    def foundtarget(self, t):
        """
        Inform the makefile of a target which is a candidate for being the default target,
        if there isn't already a default target.
        """
        if self.defaulttarget is None:
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
        assert isinstance(target, str)

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

    def include(self, path, required=True, loc=None):
        """
        Include the makefile at `path`.
        """
        fspath = os.path.join(self.workdir, path)
        if os.path.exists(fspath):
            self.included.append(path)
            stmts = parser.parsefile(fspath)
            self.variables.append('MAKEFILE_LIST', Variables.SOURCE_AUTOMATIC, path, None, self)
            stmts.execute(self)
            self.gettarget(path).explicit = True
        elif required:
            raise DataError("Attempting to include file '%s' which doesn't exist." % (path,), loc)

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
        reparse = False

        serial = self.context.jcount == 1

        def remakedone():
            for t, oldmtime in mlist:
                if t.mtime != oldmtime:
                    cb(remade=True)
                    return
            cb(remade=False)

        mlist = []
        for f in self.included:
            t = self.gettarget(f)
            t.explicit = True
            t.resolvevpath(self)
            oldmtime = t.mtime

            mlist.append((t, oldmtime))

        if serial:
            remakelist = [self.gettarget(f) for f in self.included]
            def remakecb(error, didanything):
                if error is not None:
                    print "Error remaking makefiles (ignored): ", error

                if len(remakelist):
                    t = remakelist.pop(0)
                    t.make(self, [], [], avoidremakeloop=True, cb=remakecb)
                else:
                    remakedone()

            remakelist.pop(0).make(self, [], [], avoidremakeloop=True, cb=remakecb)
        else:
            o = util.makeobject(('remakesremaining',), remakesremaining=len(self.included))
            def remakecb(error, didanything):
                if error is not None:
                    print "Error remaking makefiles (ignored): ", error

                o.remakesremaining -= 1
                if o.remakesremaining == 0:
                    remakedone()

            for t, mtime in mlist:
                t.make(self, [], [], avoidremakeloop=True, cb=remakecb)

    flagescape = re.compile(r'([\s\\])')

    def getsubenvironment(self, variables):
        env = dict(self.env)
        for vname in self.exportedvars:
            flavor, source, val = variables.get(vname)
            if val is None:
                strval = ''
            else:
                strval = val.resolvestr(self, variables, [vname])
            env[vname] = strval

        makeflags = ''

        flavor, source, val = variables.get('MAKEFLAGS')
        if val is not None:
            flagsval = val.resolvestr(self, variables, ['MAKEFLAGS'])
            if flagsval != '':
                makeflags = flagsval

        makeflags += ' -- '
        makeflags += ' '.join((self.flagescape.sub(r'\\\1', o) for o in self.overrides))

        env['MAKEFLAGS'] = makeflags

        env['MAKELEVEL'] = str(self.makelevel + 1)
        return env
