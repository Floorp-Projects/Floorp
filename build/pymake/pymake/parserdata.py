import logging, re, os
import data, parser, functions, util
from cStringIO import StringIO
from pymake.globrelative import hasglob, glob

_log = logging.getLogger('pymake.data')
_tabwidth = 4

class Location(object):
    """
    A location within a makefile.

    For the moment, locations are just path/line/column, but in the future
    they may reference parent locations for more accurate "included from"
    or "evaled at" error reporting.
    """
    __slots__ = ('path', 'line', 'column')

    def __init__(self, path, line, column):
        self.path = path
        self.line = line
        self.column = column

    def offset(self, s, start, end):
        """
        Returns a new location offset by
        the specified string.
        """

        if start == end:
            return self

        skiplines = s.count('\n', start, end)
        line = self.line + skiplines
        if skiplines:
            lastnl = s.rfind('\n', start, end)
            assert lastnl != -1
            start = lastnl + 1
            column = 0
        else:
            column = self.column

        while True:
            j = s.find('\t', start, end)
            if j == -1:
                column += end - start
                break

            column += j - start
            column += _tabwidth
            column -= column % _tabwidth
            start = j + 1

        return Location(self.path, line, column)

    def __str__(self):
        return "%s:%s:%s" % (self.path, self.line, self.column)

def _expandwildcards(makefile, tlist):
    for t in tlist:
        if not hasglob(t):
            yield t
        else:
            l = glob(makefile.workdir, t)
            for r in l:
                yield r

_flagescape = re.compile(r'([\s\\])')

def parsecommandlineargs(args):
    """
    Given a set of arguments from a command-line invocation of make,
    parse out the variable definitions and return (stmts, arglist, overridestr)
    """

    overrides = []
    stmts = StatementList()
    r = []
    for i in xrange(0, len(args)):
        a = args[i]

        vname, t, val = util.strpartition(a, ':=')
        if t == '':
            vname, t, val = util.strpartition(a, '=')
        if t != '':
            overrides.append(_flagescape.sub(r'\\\1', a))

            vname = vname.strip()
            vnameexp = data.Expansion.fromstring(vname, "Command-line argument")

            stmts.append(SetVariable(vnameexp, token=t,
                                     value=val, valueloc=Location('<command-line>', i, len(vname) + len(t)),
                                     targetexp=None, source=data.Variables.SOURCE_COMMANDLINE))
        else:
            r.append(a)

    return stmts, r, ' '.join(overrides)

class Statement(object):
    """
    Represents parsed make file syntax.

    This is an abstract base class. Child classes are expected to implement
    `execute()`.
    """

    def execute(self, makefile, context):
        raise Exception("Must implement execute() in child classes.")

class DummyRule(object):
    __slots__ = ()

    def addcommand(self, r):
        pass

class Rule(Statement):
    """
    Rules represent how to make specific targets.

    See https://www.gnu.org/software/make/manual/make.html#Rules.

    An individual rule is composed of a target, dependencies, and a recipe.
    This class only contains references to the first 2. The recipe will be
    contained in Command classes which follow this one in a stream of Statement
    instances.

    Instances also contain a boolean property `doublecolon` which says whether
    this is a doublecolon rule. Doublecolon rules are rules that are always
    executed, if they are evaluated. Normally, rules are only executed if their
    target is out of date.
    """
    __slots__ = ('targetexp', 'depexp', 'doublecolon')

    def __init__(self, targetexp, depexp, doublecolon):
        assert isinstance(targetexp, (data.Expansion, data.StringExpansion))
        assert isinstance(depexp, (data.Expansion, data.StringExpansion))

        self.targetexp = targetexp
        self.depexp = depexp
        self.doublecolon = doublecolon

    def execute(self, makefile, context):
        atargets = data.stripdotslashes(self.targetexp.resolvesplit(makefile, makefile.variables))
        targets = [data.Pattern(p) for p in _expandwildcards(makefile, atargets)]

        if not len(targets):
            context.currule = DummyRule()
            return

        ispatterns = set((t.ispattern() for t in targets))
        if len(ispatterns) == 2:
            raise data.DataError("Mixed implicit and normal rule", self.targetexp.loc)
        ispattern, = ispatterns

        if ispattern and context.weak:
            raise data.DataError("Pattern rules not allowed in includedeps", self.targetexp.loc)

        deps = [p for p in _expandwildcards(makefile, data.stripdotslashes(self.depexp.resolvesplit(makefile, makefile.variables)))]
        if ispattern:
            rule = data.PatternRule(targets, map(data.Pattern, deps), self.doublecolon, loc=self.targetexp.loc)
            makefile.appendimplicitrule(rule)
        else:
            rule = data.Rule(deps, self.doublecolon, loc=self.targetexp.loc, weakdeps=context.weak)
            for t in targets:
                makefile.gettarget(t.gettarget()).addrule(rule)

            makefile.foundtarget(targets[0].gettarget())

        context.currule = rule

    def dump(self, fd, indent):
        print >>fd, "%sRule %s: %s" % (indent, self.targetexp, self.depexp)

class StaticPatternRule(Statement):
    """
    Static pattern rules are rules which specify multiple targets based on a
    string pattern.

    See https://www.gnu.org/software/make/manual/make.html#Static-Pattern

    They are like `Rule` instances except an added property, `patternexp` is
    present. It contains the Expansion which represents the rule pattern.
    """
    __slots__ = ('targetexp', 'patternexp', 'depexp', 'doublecolon')

    def __init__(self, targetexp, patternexp, depexp, doublecolon):
        assert isinstance(targetexp, (data.Expansion, data.StringExpansion))
        assert isinstance(patternexp, (data.Expansion, data.StringExpansion))
        assert isinstance(depexp, (data.Expansion, data.StringExpansion))

        self.targetexp = targetexp
        self.patternexp = patternexp
        self.depexp = depexp
        self.doublecolon = doublecolon

    def execute(self, makefile, context):
        if context.weak:
            raise data.DataError("Static pattern rules not allowed in includedeps", self.targetexp.loc)

        targets = list(_expandwildcards(makefile, data.stripdotslashes(self.targetexp.resolvesplit(makefile, makefile.variables))))

        if not len(targets):
            context.currule = DummyRule()
            return

        patterns = list(data.stripdotslashes(self.patternexp.resolvesplit(makefile, makefile.variables)))
        if len(patterns) != 1:
            raise data.DataError("Static pattern rules must have a single pattern", self.patternexp.loc)
        pattern = data.Pattern(patterns[0])

        deps = [data.Pattern(p) for p in _expandwildcards(makefile, data.stripdotslashes(self.depexp.resolvesplit(makefile, makefile.variables)))]

        rule = data.PatternRule([pattern], deps, self.doublecolon, loc=self.targetexp.loc)

        for t in targets:
            if data.Pattern(t).ispattern():
                raise data.DataError("Target '%s' of a static pattern rule must not be a pattern" % (t,), self.targetexp.loc)
            stem = pattern.match(t)
            if stem is None:
                raise data.DataError("Target '%s' does not match the static pattern '%s'" % (t, pattern), self.targetexp.loc)
            makefile.gettarget(t).addrule(data.PatternRuleInstance(rule, '', stem, pattern.ismatchany()))

        makefile.foundtarget(targets[0])
        context.currule = rule

    def dump(self, fd, indent):
        print >>fd, "%sStaticPatternRule %s: %s: %s" % (indent, self.targetexp, self.patternexp, self.depexp)

class Command(Statement):
    """
    Commands are things that get executed by a rule.

    A rule's recipe is composed of 0 or more Commands.

    A command is simply an expansion. Commands typically represent strings to
    be executed in a shell (e.g. via system()). Although, since make files
    allow arbitrary shells to be used for command execution, this isn't a
    guarantee.
    """
    __slots__ = ('exp',)

    def __init__(self, exp):
        assert isinstance(exp, (data.Expansion, data.StringExpansion))
        self.exp = exp

    def execute(self, makefile, context):
        assert context.currule is not None
        if context.weak:
            raise data.DataError("rules not allowed in includedeps", self.exp.loc)

        context.currule.addcommand(self.exp)

    def dump(self, fd, indent):
        print >>fd, "%sCommand %s" % (indent, self.exp,)

class SetVariable(Statement):
    """
    Represents a variable assignment.

    Variable assignment comes in two different flavors.

    Simple assignment has the form:

      <Expansion> <Assignment Token> <string>

    e.g. FOO := bar

    These correspond to the fields `vnameexp`, `token`, and `value`. In
    addition, `valueloc` will be a Location and `source` will be a
    pymake.data.Variables.SOURCE_* constant.

    There are also target-specific variables. These are variables that only
    apply in the context of a specific target. They are like the aforementioned
    assignment except the `targetexp` field is set to an Expansion representing
    the target they apply to.
    """
    __slots__ = ('vnameexp', 'token', 'value', 'valueloc', 'targetexp', 'source')

    def __init__(self, vnameexp, token, value, valueloc, targetexp, source=None):
        assert isinstance(vnameexp, (data.Expansion, data.StringExpansion))
        assert isinstance(value, str)
        assert targetexp is None or isinstance(targetexp, (data.Expansion, data.StringExpansion))

        if source is None:
            source = data.Variables.SOURCE_MAKEFILE

        self.vnameexp = vnameexp
        self.token = token
        self.value = value
        self.valueloc = valueloc
        self.targetexp = targetexp
        self.source = source

    def execute(self, makefile, context):
        vname = self.vnameexp.resolvestr(makefile, makefile.variables)
        if len(vname) == 0:
            raise data.DataError("Empty variable name", self.vnameexp.loc)

        if self.targetexp is None:
            setvariables = [makefile.variables]
        else:
            setvariables = []

            targets = [data.Pattern(t) for t in data.stripdotslashes(self.targetexp.resolvesplit(makefile, makefile.variables))]
            for t in targets:
                if t.ispattern():
                    setvariables.append(makefile.getpatternvariables(t))
                else:
                    setvariables.append(makefile.gettarget(t.gettarget()).variables)

        for v in setvariables:
            if self.token == '+=':
                v.append(vname, self.source, self.value, makefile.variables, makefile)
                continue

            if self.token == '?=':
                flavor = data.Variables.FLAVOR_RECURSIVE
                oldflavor, oldsource, oldval = v.get(vname, expand=False)
                if oldval is not None:
                    continue
                value = self.value
            elif self.token == '=':
                flavor = data.Variables.FLAVOR_RECURSIVE
                value = self.value
            else:
                assert self.token == ':='

                flavor = data.Variables.FLAVOR_SIMPLE
                d = parser.Data.fromstring(self.value, self.valueloc)
                e, t, o = parser.parsemakesyntax(d, 0, (), parser.iterdata)
                value = e.resolvestr(makefile, makefile.variables)

            v.set(vname, flavor, self.source, value)

    def dump(self, fd, indent):
        print >>fd, "%sSetVariable<%s> %s %s\n%s %r" % (indent, self.valueloc, self.vnameexp, self.token, indent, self.value)

class Condition(object):
    """
    An abstract "condition", either ifeq or ifdef, perhaps negated.

    See https://www.gnu.org/software/make/manual/make.html#Conditional-Syntax

    Subclasses must implement:

    def evaluate(self, makefile)
    """

class EqCondition(Condition):
    """
    Represents an ifeq or ifneq conditional directive.

    This directive consists of two Expansions which are compared for equality.

    The `expected` field is a bool indicating what the condition must evaluate
    to in order for its body to be executed. If True, this is an "ifeq"
    conditional directive. If False, an "ifneq."
    """
    __slots__ = ('exp1', 'exp2', 'expected')

    def __init__(self, exp1, exp2):
        assert isinstance(exp1, (data.Expansion, data.StringExpansion))
        assert isinstance(exp2, (data.Expansion, data.StringExpansion))

        self.expected = True
        self.exp1 = exp1
        self.exp2 = exp2

    def evaluate(self, makefile):
        r1 = self.exp1.resolvestr(makefile, makefile.variables)
        r2 = self.exp2.resolvestr(makefile, makefile.variables)
        return (r1 == r2) == self.expected

    def __str__(self):
        return "ifeq (expected=%s) %s %s" % (self.expected, self.exp1, self.exp2)

class IfdefCondition(Condition):
    """
    Represents an ifdef or ifndef conditional directive.

    This directive consists of a single expansion which represents the name of
    a variable (without the leading '$') which will be checked for definition.

    The `expected` field is a bool and has the same behavior as EqCondition.
    If it is True, this represents a "ifdef" conditional. If False, "ifndef."
    """
    __slots__ = ('exp', 'expected')

    def __init__(self, exp):
        assert isinstance(exp, (data.Expansion, data.StringExpansion))
        self.exp = exp
        self.expected = True

    def evaluate(self, makefile):
        vname = self.exp.resolvestr(makefile, makefile.variables)
        flavor, source, value = makefile.variables.get(vname, expand=False)

        if value is None:
            return not self.expected

        return (len(value) > 0) == self.expected

    def __str__(self):
        return "ifdef (expected=%s) %s" % (self.expected, self.exp)

class ElseCondition(Condition):
    """
    Represents the transition between branches in a ConditionBlock.
    """
    __slots__ = ()

    def evaluate(self, makefile):
        return True

    def __str__(self):
        return "else"

class ConditionBlock(Statement):
    """
    A set of related Conditions.

    This is essentially a list of 2-tuples of (Condition, list(Statement)).

    The parser creates a ConditionBlock for all statements related to the same
    conditional group. If iterating over the parser's output, where you think
    you would see an ifeq, you will see a ConditionBlock containing an IfEq. In
    other words, the parser collapses separate statements into this container
    class.

    ConditionBlock instances may exist within other ConditionBlock if the
    conditional logic is multiple levels deep.
    """
    __slots__ = ('loc', '_groups')

    def __init__(self, loc, condition):
        self.loc = loc
        self._groups = []
        self.addcondition(loc, condition)

    def getloc(self):
        return self.loc

    def addcondition(self, loc, condition):
        assert isinstance(condition, Condition)
        condition.loc = loc

        if len(self._groups) and isinstance(self._groups[-1][0], ElseCondition):
            raise parser.SyntaxError("Multiple else conditions for block starting at %s" % self.loc, loc)

        self._groups.append((condition, StatementList()))

    def append(self, statement):
        self._groups[-1][1].append(statement)

    def execute(self, makefile, context):
        i = 0
        for c, statements in self._groups:
            if c.evaluate(makefile):
                _log.debug("Condition at %s met by clause #%i", self.loc, i)
                statements.execute(makefile, context)
                return

            i += 1

    def dump(self, fd, indent):
        print >>fd, "%sConditionBlock" % (indent,)

        indent2 = indent + '  '
        for c, statements in self._groups:
            print >>fd, "%s Condition %s" % (indent, c)
            statements.dump(fd, indent2)
            print >>fd, "%s ~Condition" % (indent,)
        print >>fd, "%s~ConditionBlock" % (indent,)

    def __iter__(self):
        return iter(self._groups)

    def __len__(self):
        return len(self._groups)

    def __getitem__(self, i):
        return self._groups[i]

class Include(Statement):
    """
    Represents the include directive.

    See https://www.gnu.org/software/make/manual/make.html#Include

    The file to be included is represented by the Expansion defined in the
    field `exp`. `required` is a bool indicating whether execution should fail
    if the specified file could not be processed.
    """
    __slots__ = ('exp', 'required', 'deps')

    def __init__(self, exp, required, weak):
        assert isinstance(exp, (data.Expansion, data.StringExpansion))
        self.exp = exp
        self.required = required
        self.weak = weak

    def execute(self, makefile, context):
        files = self.exp.resolvesplit(makefile, makefile.variables)
        for f in files:
            makefile.include(f, self.required, loc=self.exp.loc, weak=self.weak)

    def dump(self, fd, indent):
        print >>fd, "%sInclude %s" % (indent, self.exp)

class VPathDirective(Statement):
    """
    Represents the vpath directive.

    See https://www.gnu.org/software/make/manual/make.html#Selective-Search
    """
    __slots__ = ('exp',)

    def __init__(self, exp):
        assert isinstance(exp, (data.Expansion, data.StringExpansion))
        self.exp = exp

    def execute(self, makefile, context):
        words = list(data.stripdotslashes(self.exp.resolvesplit(makefile, makefile.variables)))
        if len(words) == 0:
            makefile.clearallvpaths()
        else:
            pattern = data.Pattern(words[0])
            mpaths = words[1:]

            if len(mpaths) == 0:
                makefile.clearvpath(pattern)
            else:
                dirs = []
                for mpath in mpaths:
                    dirs.extend((dir for dir in mpath.split(os.pathsep)
                                 if dir != ''))
                if len(dirs):
                    makefile.addvpath(pattern, dirs)

    def dump(self, fd, indent):
        print >>fd, "%sVPath %s" % (indent, self.exp)

class ExportDirective(Statement):
    """
    Represents the "export" directive.

    This is used to control exporting variables to sub makes.

    See https://www.gnu.org/software/make/manual/make.html#Variables_002fRecursion

    The `single` field defines whether this statement occurred with or without
    a variable assignment. If True, no variable assignment was present. If
    False, the SetVariable immediately following this statement originally came
    from this export directive (the parser splits it into multiple statements).
    """
    __slots__ = ('exp', 'single')

    def __init__(self, exp, single):
        assert isinstance(exp, (data.Expansion, data.StringExpansion))
        self.exp = exp
        self.single = single

    def execute(self, makefile, context):
        if self.single:
            vlist = [self.exp.resolvestr(makefile, makefile.variables)]
        else:
            vlist = list(self.exp.resolvesplit(makefile, makefile.variables))
            if not len(vlist):
                raise data.DataError("Exporting all variables is not supported", self.exp.loc)

        for v in vlist:
            makefile.exportedvars[v] = True

    def dump(self, fd, indent):
        print >>fd, "%sExport (single=%s) %s" % (indent, self.single, self.exp)

class UnexportDirective(Statement):
    """
    Represents the "unexport" directive.

    This is the opposite of ExportDirective.
    """
    __slots__ = ('exp',)

    def __init__(self, exp):
        self.exp = exp

    def execute(self, makefile, context):
        vlist = list(self.exp.resolvesplit(makefile, makefile.variables))
        for v in vlist:
            makefile.exportedvars[v] = False

    def dump(self, fd, indent):
        print >>fd, "%sUnexport %s" % (indent, self.exp)

class EmptyDirective(Statement):
    """
    Represents a standalone statement, usually an Expansion.

    You will encounter EmptyDirective instances if there is a function
    or similar at the top-level of a make file (e.g. outside of a rule or
    variable assignment). You can also find them as the bodies of
    ConditionBlock branches.
    """
    __slots__ = ('exp',)

    def __init__(self, exp):
        assert isinstance(exp, (data.Expansion, data.StringExpansion))
        self.exp = exp

    def execute(self, makefile, context):
        v = self.exp.resolvestr(makefile, makefile.variables)
        if v.strip() != '':
            raise data.DataError("Line expands to non-empty value", self.exp.loc)

    def dump(self, fd, indent):
        print >>fd, "%sEmptyDirective: %s" % (indent, self.exp)

class _EvalContext(object):
    __slots__ = ('currule', 'weak')

    def __init__(self, weak):
        self.weak = weak

class StatementList(list):
    """
    A list of Statement instances.

    This is what is generated by the parser when a make file is parsed.

    Consumers can iterate over all Statement instances in this collection to
    statically inspect (and even modify) make files before they are executed.
    """
    __slots__ = ('mtime',)

    def append(self, statement):
        assert isinstance(statement, Statement)
        list.append(self, statement)

    def execute(self, makefile, context=None, weak=False):
        if context is None:
            context = _EvalContext(weak=weak)

        for s in self:
            s.execute(makefile, context)

    def dump(self, fd, indent):
        for s in self:
            s.dump(fd, indent)

    def __str__(self):
        fd = StringIO()
        self.dump(fd, '')
        return fd.getvalue()

def iterstatements(stmts):
    for s in stmts:
        yield s
        if isinstance(s, ConditionBlock):
            for c, sl in s:
                for s2 in iterstatments(sl): yield s2
