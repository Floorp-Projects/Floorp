"""
Module for parsing Makefile syntax.

Makefiles use a line-based parsing system. Continuations and substitutions are handled differently based on the
type of line being parsed:

Lines with makefile syntax condense continuations to a single space, no matter the actual trailing whitespace
of the first line or the leading whitespace of the continuation. In other situations, trailing whitespace is
relevant.

Lines with command syntax do not condense continuations: the backslash and newline are part of the command.
(GNU Make is buggy in this regard, at least on mac).

Lines with an initial tab are commands if they can be (there is a rule or a command immediately preceding).
Otherwise, they are parsed as makefile syntax.

This file parses into the data structures defined in the parserdata module. Those classes are what actually
do the dirty work of "executing" the parsed data into a data.Makefile.

Four iterator functions are available:
* iterdata
* itermakefilechars
* itercommandchars

The iterators handle line continuations and comments in different ways, but share a common calling
convention:

Called with (data, startoffset, tokenlist, finditer)

yield 4-tuples (flatstr, token, tokenoffset, afteroffset)
flatstr is data, guaranteed to have no tokens (may be '')
token, tokenoffset, afteroffset *may be None*. That means there is more text
coming.
"""

import logging, re, os, sys
import data, functions, util, parserdata

_log = logging.getLogger('pymake.parser')

class SyntaxError(util.MakeError):
    pass

_skipws = re.compile('\S')
class Data(object):
    """
    A single virtual "line", which can be multiple source lines joined with
    continuations.
    """

    __slots__ = ('s', 'lstart', 'lend', 'loc')

    def __init__(self, s, lstart, lend, loc):
        self.s = s
        self.lstart = lstart
        self.lend = lend
        self.loc = loc

    @staticmethod
    def fromstring(s, path):
        return Data(s, 0, len(s), parserdata.Location(path, 1, 0))

    def getloc(self, offset):
        assert offset >= self.lstart and offset <= self.lend
        return self.loc.offset(self.s, self.lstart, offset)

    def skipwhitespace(self, offset):
        """
        Return the offset of the first non-whitespace character in data starting at offset, or None if there are
        only whitespace characters remaining.
        """
        m = _skipws.search(self.s, offset, self.lend)
        if m is None:
            return self.lend

        return m.start(0)

_linere = re.compile(r'\\*\n')
def enumeratelines(s, filename):
    """
    Enumerate lines in a string as Data objects, joining line
    continuations.
    """

    off = 0
    lineno = 1
    curlines = 0
    for m in _linere.finditer(s):
        curlines += 1
        start, end = m.span(0)

        if (start - end) % 2 == 0:
            # odd number of backslashes is a continuation
            continue

        yield Data(s, off, end - 1, parserdata.Location(filename, lineno, 0))

        lineno += curlines
        curlines = 0
        off = end

    yield Data(s, off, len(s), parserdata.Location(filename, lineno, 0))

_alltokens = re.compile(r'''\\*\# | # hash mark preceeded by any number of backslashes
                            := |
                            \+= |
                            \?= |
                            :: |
                            (?:\$(?:$|[\(\{](?:%s)\s+|.)) | # dollar sign followed by EOF, a function keyword with whitespace, or any character
                            :(?![\\/]) | # colon followed by anything except a slash (Windows path detection)
                            [=#{}();,|'"]''' % '|'.join(functions.functionmap.iterkeys()), re.VERBOSE)

def iterdata(d, offset, tokenlist, it):
    """
    Iterate over flat data without line continuations, comments, or any special escaped characters.

    Typically used to parse recursively-expanded variables.
    """

    assert len(tokenlist), "Empty tokenlist passed to iterdata is meaningless!"
    assert offset >= d.lstart and offset <= d.lend, "offset %i should be between %i and %i" % (offset, d.lstart, d.lend)

    if offset == d.lend:
        return

    s = d.s
    for m in it:
        mstart, mend = m.span(0)
        token = s[mstart:mend]
        if token in tokenlist or (token[0] == '$' and '$' in tokenlist):
            yield s[offset:mstart], token, mstart, mend
        else:
            yield s[offset:mend], None, None, mend
        offset = mend

    yield s[offset:d.lend], None, None, None

# multiple backslashes before a newline are unescaped, halving their total number
_makecontinuations = re.compile(r'(?:\s*|((?:\\\\)+))\\\n\s*')
def _replacemakecontinuations(m):
    start, end = m.span(1)
    if start == -1:
        return ' '
    return ' '.rjust((end - start) / 2 + 1, '\\')

def itermakefilechars(d, offset, tokenlist, it, ignorecomments=False):
    """
    Iterate over data in makefile syntax. Comments are found at unescaped # characters, and escaped newlines
    are converted to single-space continuations.
    """

    assert offset >= d.lstart and offset <= d.lend, "offset %i should be between %i and %i" % (offset, d.lstart, d.lend)

    if offset == d.lend:
        return

    s = d.s
    for m in it:
        mstart, mend = m.span(0)
        token = s[mstart:mend]

        starttext = _makecontinuations.sub(_replacemakecontinuations, s[offset:mstart])

        if token[-1] == '#' and not ignorecomments:
            l = mend - mstart
            # multiple backslashes before a hash are unescaped, halving their total number
            if l % 2:
                # found a comment
                yield starttext + token[:(l - 1) / 2], None, None, None
                return
            else:
                yield starttext + token[-l / 2:], None, None, mend
        elif token in tokenlist or (token[0] == '$' and '$' in tokenlist):
            yield starttext, token, mstart, mend
        else:
            yield starttext + token, None, None, mend
        offset = mend

    yield _makecontinuations.sub(_replacemakecontinuations, s[offset:d.lend]), None, None, None

_findcomment = re.compile(r'\\*\#')
def flattenmakesyntax(d, offset):
    """
    A shortcut method for flattening line continuations and comments in makefile syntax without
    looking for other tokens.
    """

    assert offset >= d.lstart and offset <= d.lend, "offset %i should be between %i and %i" % (offset, d.lstart, d.lend)
    if offset == d.lend:
        return ''

    s = _makecontinuations.sub(_replacemakecontinuations, d.s[offset:d.lend])

    elements = []
    offset = 0
    for m in _findcomment.finditer(s):
        mstart, mend = m.span(0)
        elements.append(s[offset:mstart])
        if (mend - mstart) % 2:
            # even number of backslashes... it's a comment
            elements.append(''.ljust((mend - mstart - 1) / 2, '\\'))
            return ''.join(elements)

        # odd number of backslashes
        elements.append(''.ljust((mend - mstart - 2) / 2, '\\') + '#')
        offset = mend

    elements.append(s[offset:])
    return ''.join(elements)

def itercommandchars(d, offset, tokenlist, it):
    """
    Iterate over command syntax. # comment markers are not special, and escaped newlines are included
    in the output text.
    """

    assert offset >= d.lstart and offset <= d.lend, "offset %i should be between %i and %i" % (offset, d.lstart, d.lend)

    if offset == d.lend:
        return

    s = d.s
    for m in it:
        mstart, mend = m.span(0)
        token = s[mstart:mend]
        starttext = s[offset:mstart].replace('\n\t', '\n')

        if token in tokenlist or (token[0] == '$' and '$' in tokenlist):
            yield starttext, token, mstart, mend
        else:
            yield starttext + token, None, None, mend
        offset = mend

    yield s[offset:d.lend].replace('\n\t', '\n'), None, None, None

_redefines = re.compile('\s*define|\s*endef')
def iterdefinelines(it, startloc):
    """
    Process the insides of a define. Most characters are included literally. Escaped newlines are treated
    as they would be in makefile syntax. Internal define/endef pairs are ignored.
    """

    results = []

    definecount = 1
    for d in it:
        m = _redefines.match(d.s, d.lstart, d.lend)
        if m is not None:
            directive = m.group(0).strip()
            if directive == 'endef':
                definecount -= 1
                if definecount == 0:
                    return _makecontinuations.sub(_replacemakecontinuations, '\n'.join(results))
            else:
                definecount += 1

        results.append(d.s[d.lstart:d.lend])

    # Falling off the end is an unterminated define!
    raise SyntaxError("define without matching endef", startloc)

def _ensureend(d, offset, msg):
    """
    Ensure that only whitespace remains in this data.
    """

    s = flattenmakesyntax(d, offset)
    if s != '' and not s.isspace():
        raise SyntaxError(msg, d.getloc(offset))

_eqargstokenlist = ('(', "'", '"')

def ifeq(d, offset):
    if offset > d.lend - 1:
        raise SyntaxError("No arguments after conditional", d.getloc(offset))

    # the variety of formats for this directive is rather maddening
    token = d.s[offset]
    if token not in _eqargstokenlist:
        raise SyntaxError("No arguments after conditional", d.getloc(offset))

    offset += 1

    if token == '(':
        arg1, t, offset = parsemakesyntax(d, offset, (',',), itermakefilechars)
        if t is None:
            raise SyntaxError("Expected two arguments in conditional", d.getloc(d.lend))

        arg1.rstrip()

        offset = d.skipwhitespace(offset)
        arg2, t, offset = parsemakesyntax(d, offset, (')',), itermakefilechars)
        if t is None:
            raise SyntaxError("Unexpected text in conditional", d.getloc(offset))

        _ensureend(d, offset, "Unexpected text after conditional")
    else:
        arg1, t, offset = parsemakesyntax(d, offset, (token,), itermakefilechars)
        if t is None:
            raise SyntaxError("Unexpected text in conditional", d.getloc(d.lend))

        offset = d.skipwhitespace(offset)
        if offset == d.lend:
            raise SyntaxError("Expected two arguments in conditional", d.getloc(offset))

        token = d.s[offset]
        if token not in '\'"':
            raise SyntaxError("Unexpected text in conditional", d.getloc(offset))

        arg2, t, offset = parsemakesyntax(d, offset + 1, (token,), itermakefilechars)

        _ensureend(d, offset, "Unexpected text after conditional")

    return parserdata.EqCondition(arg1, arg2)

def ifneq(d, offset):
    c = ifeq(d, offset)
    c.expected = False
    return c

def ifdef(d, offset):
    e, t, offset = parsemakesyntax(d, offset, (), itermakefilechars)
    e.rstrip()

    return parserdata.IfdefCondition(e)

def ifndef(d, offset):
    c = ifdef(d, offset)
    c.expected = False
    return c

_conditionkeywords = {
    'ifeq': ifeq,
    'ifneq': ifneq,
    'ifdef': ifdef,
    'ifndef': ifndef
    }

_conditiontokens = tuple(_conditionkeywords.iterkeys())
_conditionre = re.compile(r'(%s)(?:$|\s+)' % '|'.join(_conditiontokens))

_directivestokenlist = _conditiontokens + \
    ('else', 'endif', 'define', 'endef', 'override', 'include', '-include', 'includedeps', '-includedeps', 'vpath', 'export', 'unexport')

_directivesre = re.compile(r'(%s)(?:$|\s+)' % '|'.join(_directivestokenlist))

_varsettokens = (':=', '+=', '?=', '=')

def _parsefile(pathname):
    fd = open(pathname, "rU")
    stmts = parsestring(fd.read(), pathname)
    stmts.mtime = os.fstat(fd.fileno()).st_mtime
    fd.close()
    return stmts

def _checktime(path, stmts):
    mtime = os.path.getmtime(path)
    if mtime != stmts.mtime:
        _log.debug("Re-parsing makefile '%s': mtimes differ", path)
        return False

    return True

_parsecache = util.MostUsedCache(50, _parsefile, _checktime)

def parsefile(pathname):
    """
    Parse a filename into a parserdata.StatementList. A cache is used to avoid re-parsing
    makefiles that have already been parsed and have not changed.
    """

    pathname = os.path.realpath(pathname)
    return _parsecache.get(pathname)

# colon followed by anything except a slash (Windows path detection)
_depfilesplitter = re.compile(r':(?![\\/])')

def parsedepfile(pathname):
    """
    Parse a filename listing only depencencies into a parserdata.StatementList.
    """
    def continuation_iter(lines):
        current_line = []
        for line in lines:
            line = line.rstrip()
            if line.endswith("\\"):
                current_line.append(line.rstrip("\\"))
                continue
            if not len(line):
                continue
            current_line.append(line)
            yield ''.join(current_line)
            current_line = []
        if current_line:
            yield ''.join(current_line)

    pathname = os.path.realpath(pathname)
    stmts = parserdata.StatementList()
    for line in continuation_iter(open(pathname).readlines()):
        target, deps = _depfilesplitter.split(line, 1)
        stmts.append(parserdata.Rule(data.StringExpansion(target, None),
                                     data.StringExpansion(deps, None), False))
    return stmts

def parsestring(s, filename):
    """
    Parse a string containing makefile data into a parserdata.StatementList.
    """

    currule = False
    condstack = [parserdata.StatementList()]

    fdlines = enumeratelines(s, filename)
    for d in fdlines:
        assert len(condstack) > 0

        offset = d.lstart

        if currule and offset < d.lend and d.s[offset] == '\t':
            e, token, offset = parsemakesyntax(d, offset + 1, (), itercommandchars)
            assert token is None
            assert offset is None
            condstack[-1].append(parserdata.Command(e))
            continue

        # To parse Makefile syntax, we first strip leading whitespace and
        # look for initial keywords. If there are no keywords, it's either
        # setting a variable or writing a rule.

        offset = d.skipwhitespace(offset)
        if offset is None:
            continue

        m = _directivesre.match(d.s, offset, d.lend)
        if m is not None:
            kword = m.group(1)
            offset = m.end(0)

            if kword == 'endif':
                _ensureend(d, offset, "Unexpected data after 'endif' directive")
                if len(condstack) == 1:
                    raise SyntaxError("unmatched 'endif' directive",
                                      d.getloc(offset))

                condstack.pop().endloc = d.getloc(offset)
                continue
            
            if kword == 'else':
                if len(condstack) == 1:
                    raise SyntaxError("unmatched 'else' directive",
                                      d.getloc(offset))

                m = _conditionre.match(d.s, offset, d.lend)
                if m is None:
                    _ensureend(d, offset, "Unexpected data after 'else' directive.")
                    condstack[-1].addcondition(d.getloc(offset), parserdata.ElseCondition())
                else:
                    kword = m.group(1)
                    if kword not in _conditionkeywords:
                        raise SyntaxError("Unexpected condition after 'else' directive.",
                                          d.getloc(offset))

                    startoffset = offset
                    offset = d.skipwhitespace(m.end(1))
                    c = _conditionkeywords[kword](d, offset)
                    condstack[-1].addcondition(d.getloc(startoffset), c)
                continue

            if kword in _conditionkeywords:
                c = _conditionkeywords[kword](d, offset)
                cb = parserdata.ConditionBlock(d.getloc(d.lstart), c)
                condstack[-1].append(cb)
                condstack.append(cb)
                continue

            if kword == 'endef':
                raise SyntaxError("endef without matching define", d.getloc(offset))

            if kword == 'define':
                currule = False
                vname, t, i = parsemakesyntax(d, offset, (), itermakefilechars)
                vname.rstrip()

                startloc = d.getloc(d.lstart)
                value = iterdefinelines(fdlines, startloc)
                condstack[-1].append(parserdata.SetVariable(vname, value=value, valueloc=startloc, token='=', targetexp=None))
                continue

            if kword in ('include', '-include', 'includedeps', '-includedeps'):
                if kword.startswith('-'):
                    required = False
                    kword = kword[1:]
                else:
                    required = True

                deps = kword == 'includedeps'

                currule = False
                incfile, t, offset = parsemakesyntax(d, offset, (), itermakefilechars)
                condstack[-1].append(parserdata.Include(incfile, required, deps))

                continue

            if kword == 'vpath':
                currule = False
                e, t, offset = parsemakesyntax(d, offset, (), itermakefilechars)
                condstack[-1].append(parserdata.VPathDirective(e))
                continue

            if kword == 'override':
                currule = False
                vname, token, offset = parsemakesyntax(d, offset, _varsettokens, itermakefilechars)
                vname.lstrip()
                vname.rstrip()

                if token is None:
                    raise SyntaxError("Malformed override directive, need =", d.getloc(d.lstart))

                value = flattenmakesyntax(d, offset).lstrip()

                condstack[-1].append(parserdata.SetVariable(vname, value=value, valueloc=d.getloc(offset), token=token, targetexp=None, source=data.Variables.SOURCE_OVERRIDE))
                continue

            if kword == 'export':
                currule = False
                e, token, offset = parsemakesyntax(d, offset, _varsettokens, itermakefilechars)
                e.lstrip()
                e.rstrip()

                if token is None:
                    condstack[-1].append(parserdata.ExportDirective(e, concurrent_set=False))
                else:
                    condstack[-1].append(parserdata.ExportDirective(e, concurrent_set=True))

                    value = flattenmakesyntax(d, offset).lstrip()
                    condstack[-1].append(parserdata.SetVariable(e, value=value, valueloc=d.getloc(offset), token=token, targetexp=None))

                continue

            if kword == 'unexport':
                e, token, offset = parsemakesyntax(d, offset, (), itermakefilechars)
                condstack[-1].append(parserdata.UnexportDirective(e))
                continue

        e, token, offset = parsemakesyntax(d, offset, _varsettokens + ('::', ':'), itermakefilechars)
        if token is None:
            e.rstrip()
            e.lstrip()
            if not e.isempty():
                condstack[-1].append(parserdata.EmptyDirective(e))
            continue

        # if we encountered real makefile syntax, the current rule is over
        currule = False

        if token in _varsettokens:
            e.lstrip()
            e.rstrip()

            value = flattenmakesyntax(d, offset).lstrip()

            condstack[-1].append(parserdata.SetVariable(e, value=value, valueloc=d.getloc(offset), token=token, targetexp=None))
        else:
            doublecolon = token == '::'

            # `e` is targets or target patterns, which can end up as
            # * a rule
            # * an implicit rule
            # * a static pattern rule
            # * a target-specific variable definition
            # * a pattern-specific variable definition
            # any of the rules may have order-only prerequisites
            # delimited by |, and a command delimited by ;
            targets = e

            e, token, offset = parsemakesyntax(d, offset,
                                               _varsettokens + (':', '|', ';'),
                                               itermakefilechars)
            if token in (None, ';'):
                condstack[-1].append(parserdata.Rule(targets, e, doublecolon))
                currule = True

                if token == ';':
                    offset = d.skipwhitespace(offset)
                    e, t, offset = parsemakesyntax(d, offset, (), itercommandchars)
                    condstack[-1].append(parserdata.Command(e))

            elif token in _varsettokens:
                e.lstrip()
                e.rstrip()

                value = flattenmakesyntax(d, offset).lstrip()
                condstack[-1].append(parserdata.SetVariable(e, value=value, valueloc=d.getloc(offset), token=token, targetexp=targets))
            elif token == '|':
                raise SyntaxError('order-only prerequisites not implemented', d.getloc(offset))
            else:
                assert token == ':'
                # static pattern rule

                pattern = e

                deps, token, offset = parsemakesyntax(d, offset, (';',), itermakefilechars)

                condstack[-1].append(parserdata.StaticPatternRule(targets, pattern, deps, doublecolon))
                currule = True

                if token == ';':
                    offset = d.skipwhitespace(offset)
                    e, token, offset = parsemakesyntax(d, offset, (), itercommandchars)
                    condstack[-1].append(parserdata.Command(e))

    if len(condstack) != 1:
        raise SyntaxError("Condition never terminated with endif", condstack[-1].loc)

    return condstack[0]

_PARSESTATE_TOPLEVEL = 0    # at the top level
_PARSESTATE_FUNCTION = 1    # expanding a function call
_PARSESTATE_VARNAME = 2     # expanding a variable expansion.
_PARSESTATE_SUBSTFROM = 3   # expanding a variable expansion substitution "from" value
_PARSESTATE_SUBSTTO = 4     # expanding a variable expansion substitution "to" value
_PARSESTATE_PARENMATCH = 5  # inside nested parentheses/braces that must be matched

class ParseStackFrame(object):
    __slots__ = ('parsestate', 'parent', 'expansion', 'tokenlist', 'openbrace', 'closebrace', 'function', 'loc', 'varname', 'substfrom')

    def __init__(self, parsestate, parent, expansion, tokenlist, openbrace, closebrace, function=None, loc=None):
        self.parsestate = parsestate
        self.parent = parent
        self.expansion = expansion
        self.tokenlist = tokenlist
        self.openbrace = openbrace
        self.closebrace = closebrace
        self.function = function
        self.loc = loc

    def __str__(self):
        return "<state=%i expansion=%s tokenlist=%s openbrace=%s closebrace=%s>" % (self.parsestate, self.expansion, self.tokenlist, self.openbrace, self.closebrace)

_matchingbrace = {
    '(': ')',
    '{': '}',
    }

def parsemakesyntax(d, offset, stopon, iterfunc):
    """
    Given Data, parse it into a data.Expansion.

    @param stopon (sequence)
        Indicate characters where toplevel parsing should stop.

    @param iterfunc (generator function)
        A function which is used to iterate over d, yielding (char, offset, loc)
        @see iterdata
        @see itermakefilechars
        @see itercommandchars
 
    @return a tuple (expansion, token, offset). If all the data is consumed,
    token and offset will be None
    """

    assert callable(iterfunc)

    stacktop = ParseStackFrame(_PARSESTATE_TOPLEVEL, None, data.Expansion(loc=d.getloc(d.lstart)),
                               tokenlist=stopon + ('$',),
                               openbrace=None, closebrace=None)

    tokeniterator = _alltokens.finditer(d.s, offset, d.lend)

    di = iterfunc(d, offset, stacktop.tokenlist, tokeniterator)
    while True: # this is not a for loop because `di` changes during the function
        assert stacktop is not None
        try:
            s, token, tokenoffset, offset = di.next()
        except StopIteration:
            break

        stacktop.expansion.appendstr(s)
        if token is None:
            continue

        parsestate = stacktop.parsestate

        if token[0] == '$':
            if tokenoffset + 1 == d.lend:
                # an unterminated $ expands to nothing
                break

            loc = d.getloc(tokenoffset)
            c = token[1]
            if c == '$':
                assert len(token) == 2
                stacktop.expansion.appendstr('$')
            elif c in ('(', '{'):
                closebrace = _matchingbrace[c]

                if len(token) > 2:
                    fname = token[2:].rstrip()
                    fn = functions.functionmap[fname](loc)
                    e = data.Expansion()
                    if len(fn) + 1 == fn.maxargs:
                        tokenlist = (c, closebrace, '$')
                    else:
                        tokenlist = (',', c, closebrace, '$')

                    stacktop = ParseStackFrame(_PARSESTATE_FUNCTION, stacktop,
                                               e, tokenlist, function=fn,
                                               openbrace=c, closebrace=closebrace)
                else:
                    e = data.Expansion()
                    tokenlist = (':', c, closebrace, '$')
                    stacktop = ParseStackFrame(_PARSESTATE_VARNAME, stacktop,
                                               e, tokenlist,
                                               openbrace=c, closebrace=closebrace, loc=loc)
            else:
                assert len(token) == 2
                e = data.Expansion.fromstring(c, loc)
                stacktop.expansion.appendfunc(functions.VariableRef(loc, e))
        elif token in ('(', '{'):
            assert token == stacktop.openbrace

            stacktop.expansion.appendstr(token)
            stacktop = ParseStackFrame(_PARSESTATE_PARENMATCH, stacktop,
                                       stacktop.expansion,
                                       (token, stacktop.closebrace, '$'),
                                       openbrace=token, closebrace=stacktop.closebrace, loc=d.getloc(tokenoffset))
        elif parsestate == _PARSESTATE_PARENMATCH:
            assert token == stacktop.closebrace
            stacktop.expansion.appendstr(token)
            stacktop = stacktop.parent
        elif parsestate == _PARSESTATE_TOPLEVEL:
            assert stacktop.parent is None
            return stacktop.expansion.finish(), token, offset
        elif parsestate == _PARSESTATE_FUNCTION:
            if token == ',':
                stacktop.function.append(stacktop.expansion.finish())

                stacktop.expansion = data.Expansion()
                if len(stacktop.function) + 1 == stacktop.function.maxargs:
                    tokenlist = (stacktop.openbrace, stacktop.closebrace, '$')
                    stacktop.tokenlist = tokenlist
            elif token in (')', '}'):
                fn = stacktop.function
                fn.append(stacktop.expansion.finish())
                fn.setup()
                
                stacktop = stacktop.parent
                stacktop.expansion.appendfunc(fn)
            else:
                assert False, "Not reached, _PARSESTATE_FUNCTION"
        elif parsestate == _PARSESTATE_VARNAME:
            if token == ':':
                stacktop.varname = stacktop.expansion
                stacktop.parsestate = _PARSESTATE_SUBSTFROM
                stacktop.expansion = data.Expansion()
                stacktop.tokenlist = ('=', stacktop.openbrace, stacktop.closebrace, '$')
            elif token in (')', '}'):
                fn = functions.VariableRef(stacktop.loc, stacktop.expansion.finish())
                stacktop = stacktop.parent
                stacktop.expansion.appendfunc(fn)
            else:
                assert False, "Not reached, _PARSESTATE_VARNAME"
        elif parsestate == _PARSESTATE_SUBSTFROM:
            if token == '=':
                stacktop.substfrom = stacktop.expansion
                stacktop.parsestate = _PARSESTATE_SUBSTTO
                stacktop.expansion = data.Expansion()
                stacktop.tokenlist = (stacktop.openbrace, stacktop.closebrace, '$')
            elif token in (')', '}'):
                # A substitution of the form $(VARNAME:.ee) is probably a mistake, but make
                # parses it. Issue a warning. Combine the varname and substfrom expansions to
                # make the compatible varname. See tests/var-substitutions.mk SIMPLE3SUBSTNAME
                _log.warning("%s: Variable reference looks like substitution without =", stacktop.loc)
                stacktop.varname.appendstr(':')
                stacktop.varname.concat(stacktop.expansion)
                fn = functions.VariableRef(stacktop.loc, stacktop.varname.finish())
                stacktop = stacktop.parent
                stacktop.expansion.appendfunc(fn)
            else:
                assert False, "Not reached, _PARSESTATE_SUBSTFROM"
        elif parsestate == _PARSESTATE_SUBSTTO:
            assert token in  (')','}'), "Not reached, _PARSESTATE_SUBSTTO"

            fn = functions.SubstitutionRef(stacktop.loc, stacktop.varname.finish(),
                                           stacktop.substfrom.finish(), stacktop.expansion.finish())
            stacktop = stacktop.parent
            stacktop.expansion.appendfunc(fn)
        else:
            assert False, "Unexpected parse state %s" % stacktop.parsestate

        if stacktop.parent is not None and iterfunc == itercommandchars:
            di = itermakefilechars(d, offset, stacktop.tokenlist, tokeniterator,
                                   ignorecomments=True)
        else:
            di = iterfunc(d, offset, stacktop.tokenlist, tokeniterator)

    if stacktop.parent is not None:
        raise SyntaxError("Unterminated function call", d.getloc(offset))

    assert stacktop.parsestate == _PARSESTATE_TOPLEVEL

    return stacktop.expansion.finish(), None, None
