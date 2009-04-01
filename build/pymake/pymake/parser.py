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
* iterdefinechars

The iterators handle line continuations and comments in different ways, but share a common calling
convention:

Called with (data, startoffset, tokenlist)

yield 4-tuples (flatstr, token, tokenoffset, afteroffset)
flatstr is data, guaranteed to have no tokens (may be '')
token, tokenoffset, afteroffset *may be None*. That means there is more text
coming.
"""

import logging, re, os, bisect
import data, functions, util, parserdata

_log = logging.getLogger('pymake.parser')

class SyntaxError(util.MakeError):
    pass

class Data(object):
    """
    A single virtual "line", which can be multiple source lines joined with
    continuations.
    """

    __slots__ = ('data', 'startloc')

    def __init__(self, startloc, data):
        self.data = data
        self.startloc = startloc

    @staticmethod
    def fromstring(str, path):
        return Data(parserdata.Location(path, 1, 0), str)

    def append(self, data):
        self.data += data

    def getloc(self, offset):
        return self.startloc + self.data[:offset]

    def skipwhitespace(self, offset):
        """
        Return the offset into data after skipping whitespace.
        """
        while offset < len(self.data):
            c = self.data[offset]
            if not c.isspace():
                break
            offset += 1
        return offset

    def findtoken(self, o, tlist, skipws):
        """
        Check data at position o for any of the tokens in tlist followed by whitespace
        or end-of-data.

        If a token is found, skip trailing whitespace and return (token, newoffset).
        Otherwise return None, oldoffset
        """
        assert isinstance(tlist, TokenList)

        if skipws:
            m = tlist.wslist.match(self.data, pos=o)
            if m is not None:
                return m.group(1), m.end(0)
        else:
            m = tlist.simplere.match(self.data, pos=o)
            if m is not None:
                return m.group(0), m.end(0)

        return None, o

class DynamicData(Data):
    """
    If we're reading from a stream, allows reading additional data dynamically.
    """
    __slots__ = Data.__slots__ + ('lineiter',)

    def __init__(self, lineiter, path):
        try:
            lineno, line = lineiter.next()
            Data.__init__(self, parserdata.Location(path, lineno + 1, 0), line)
            self.lineiter = lineiter
        except StopIteration:
            self.data = None

    def readline(self):
        try:
            lineno, line = self.lineiter.next()
            self.append(line)
            return True
        except StopIteration:
            return False

_makefiletokensescaped = [r'\\\\#', r'\\#', '\\\\\n', '\\\\\\s+\\\\\n', r'\\.', '#', '\n']
_continuationtokensescaped = ['\\\\\n', r'\\.', '\n']

class TokenList(object):
    """
    A list of tokens to search. Because these lists are static, we can perform
    optimizations (such as escaping and compiling regexes) on construction.
    """

    __slots__ = ('tlist', 'emptylist', 'escapedlist', 'simplere', 'makefilere', 'continuationre', 'wslist')

    def __init__(self, tlist):
        self.tlist = tlist
        self.emptylist = len(tlist) == 0
        self.escapedlist = [re.escape(t) for t in tlist]

    def __getattr__(self, name):
        if name == 'simplere':
            self.simplere = re.compile('|'.join(self.escapedlist))
            return self.simplere

        if name == 'makefilere':
            self.makefilere = re.compile('|'.join(self.escapedlist + _makefiletokensescaped))
            return self.makefilere

        if name == 'continuationre':
            self.continuationre = re.compile('|'.join(self.escapedlist + _continuationtokensescaped))
            return self.continuationre

        if name == 'wslist':
            self.wslist = re.compile('(' + '|'.join(self.escapedlist) + ')' + r'(\s+|$)')
            return self.wslist

        raise AttributeError(name)

    _imap = {}

    @staticmethod
    def get(s):
        i = TokenList._imap.get(s, None)
        if i is None:
            i = TokenList(s)
            TokenList._imap[s] = i

        return i

_emptytokenlist = TokenList.get('')

def iterdata(d, offset, tokenlist):
    """
    Iterate over flat data without line continuations, comments, or any special escaped characters.

    Typically used to parse recursively-expanded variables.
    """

    if tokenlist.emptylist:
        yield d.data, None, None, None
        return

    s = tokenlist.simplere
    datalen = len(d.data)

    while offset < datalen:
        m = s.search(d.data, pos=offset)
        if m is None:
            yield d.data[offset:], None, None, None
            return

        yield d.data[offset:m.start(0)], m.group(0), m.start(0), m.end(0)
        offset = m.end(0)

def itermakefilechars(d, offset, tokenlist, ignorecomments=False):
    """
    Iterate over data in makefile syntax. Comments are found at unescaped # characters, and escaped newlines
    are converted to single-space continuations.
    """

    s = tokenlist.makefilere

    while offset < len(d.data):
        m = s.search(d.data, pos=offset)
        if m is None:
            yield d.data[offset:], None, None, None
            return

        token = m.group(0)
        start = m.start(0)
        end = m.end(0)

        if token == '\n':
            assert end == len(d.data)
            yield d.data[offset:start], None, None, None
            return

        if token == '#':
            if ignorecomments:
                yield d.data[offset:end], None, None, None
                offset = end
                continue

            yield d.data[offset:start], None, None, None
            for s in itermakefilechars(d, end, _emptytokenlist): pass
            return

        if token == '\\\\#':
            # see escape-chars.mk VARAWFUL
            yield d.data[offset:start + 1], None, None, None
            for s in itermakefilechars(d, end, _emptytokenlist): pass
            return

        if token == '\\\n':
            yield d.data[offset:start].rstrip() + ' ', None, None, None
            d.readline()
            offset = d.skipwhitespace(end)
            continue

        if token.startswith('\\') and token.endswith('\n'):
            assert end == len(d.data)
            yield d.data[offset:start] + '\\ ', None, None, None
            d.readline()
            offset = d.skipwhitespace(end)
            continue

        if token == '\\#':
            yield d.data[offset:start] + '#', None, None, None
        elif token.startswith('\\'):
            if token[1:] in tokenlist.tlist:
                yield d.data[offset:start + 1], token[1:], start + 1, end
            else:
                yield d.data[offset:end], None, None, None
        else:
            yield d.data[offset:start], token, start, end

        offset = end

def itercommandchars(d, offset, tokenlist):
    """
    Iterate over command syntax. # comment markers are not special, and escaped newlines are included
    in the output text.
    """

    s = tokenlist.continuationre

    while offset < len(d.data):
        m = s.search(d.data, pos=offset)
        if m is None:
            yield d.data[offset:], None, None, None
            return

        token = m.group(0)
        start = m.start(0)
        end = m.end(0)

        if token == '\n':
            assert end == len(d.data)
            yield d.data[offset:start], None, None, None
            return

        if token == '\\\n':
            yield d.data[offset:end], None, None, None
            d.readline()
            offset = end
            if offset < len(d.data) and d.data[offset] == '\t':
                offset += 1
            continue
        
        if token.startswith('\\'):
            if token[1:] in tokenlist.tlist:
                yield d.data[offset:start + 1], token[1:], start + 1, end
            else:
                yield d.data[offset:end], None, None, None
        else:
            yield d.data[offset:start], token, start, end

        offset = end

_definestokenlist = TokenList.get(('define', 'endef'))

def iterdefinechars(d, offset, tokenlist):
    """
    Iterate over define blocks. Most characters are included literally. Escaped newlines are treated
    as they would be in makefile syntax. Internal define/endef pairs are ignored.
    """

    def checkfortoken(o):
        """
        Check for a define or endef token on the line starting at o.
        Return an integer for the direction of definecount.
        """
        if o >= len(d.data):
            return 0

        if d.data[o] == '\t':
            return 0

        o = d.skipwhitespace(o)
        token, o = d.findtoken(o, _definestokenlist, True)
        if token == 'define':
            return 1

        if token == 'endef':
            return -1
        
        return 0

    startoffset = offset
    definecount = 1 + checkfortoken(offset)
    if definecount == 0:
        return

    s = tokenlist.continuationre

    while offset < len(d.data):
        m = s.search(d.data, pos=offset)
        if m is None:
            yield d.data[offset:], None, None, None
            break

        token = m.group(0)
        start = m.start(0)
        end = m.end(0)

        if token == '\\\n':
            yield d.data[offset:start].rstrip() + ' ', None, None, None
            d.readline()
            offset = d.skipwhitespace(end)
            continue

        if token == '\n':
            assert end == len(d.data), "end: %r len(d.data): %r" % (end, len(d.data))
            d.readline()
            definecount += checkfortoken(end)
            if definecount == 0:
                yield d.data[offset:start], None, None, None
                return

            yield d.data[offset:end], None, None, None
        elif token.startswith('\\'):
            if token[1:] in tokenlist.tlist:
                yield d.data[offset:start + 1], token[1:], start + 1, end
            else:
                yield d.data[offset:end], None, None, None
        else:
            yield d.data[offset:start], token, start, end

        offset = end

    # Unlike the other iterators, if you fall off this one there is an unterminated
    # define.
    raise SyntaxError("Unterminated define", d.getloc(startoffset))

def _iterflatten(iter, data, offset):
    return ''.join((str for str, t, o, oo in iter(data, offset, _emptytokenlist)))

def _ensureend(d, offset, msg, ifunc=itermakefilechars):
    """
    Ensure that only whitespace remains in this data.
    """

    for c, t, o, oo in ifunc(d, offset, _emptytokenlist):
        if c != '' and not c.isspace():
            raise SyntaxError(msg, d.getloc(o))

_eqargstokenlist = TokenList.get(('(', "'", '"'))

def ifeq(d, offset):
    # the variety of formats for this directive is rather maddening
    token, offset = d.findtoken(offset, _eqargstokenlist, False)
    if token is None:
        raise SyntaxError("No arguments after conditional", d.getloc(offset))

    if token == '(':
        arg1, t, offset = parsemakesyntax(d, offset, (',',), itermakefilechars)
        if t is None:
            raise SyntaxError("Expected two arguments in conditional", d.getloc(offset))

        arg1.rstrip()

        offset = d.skipwhitespace(offset)
        arg2, t, offset = parsemakesyntax(d, offset, (')',), itermakefilechars)
        if t is None:
            raise SyntaxError("Unexpected text in conditional", d.getloc(offset))

        _ensureend(d, offset, "Unexpected text after conditional")
    else:
        arg1, t, offset = parsemakesyntax(d, offset, (token,), itermakefilechars)
        if t is None:
            raise SyntaxError("Unexpected text in conditional", d.getloc(offset))

        offset = d.skipwhitespace(offset)
        if offset == len(d.data):
            raise SyntaxError("Expected two arguments in conditional", d.getloc(offset))

        token = d.data[offset]
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
_directivestokenlist = TokenList.get(_conditiontokens + \
    ('else', 'endif', 'define', 'endef', 'override', 'include', '-include', 'vpath', 'export', 'unexport'))
_conditionkeywordstokenlist = TokenList.get(_conditiontokens)

_varsettokens = (':=', '+=', '?=', '=')

def _parsefile(pathname):
    fd = open(pathname, "rU")
    stmts = parsestream(fd, pathname)
    stmts.mtime = os.fstat(fd.fileno()).st_mtime
    fd.close()
    return stmts

def _checktime(path, stmts):
    mtime = os.path.getmtime(path)
    if mtime != stmts.mtime:
        _log.debug("Re-parsing makefile '%s': mtimes differ", path)
        return False

    return True

_parsecache = util.MostUsedCache(15, _parsefile, _checktime)

def parsefile(pathname):
    """
    Parse a filename into a parserdata.StatementList. A cache is used to avoid re-parsing
    makefiles that have already been parsed and have not changed.
    """

    pathname = os.path.realpath(pathname)
    return _parsecache.get(pathname)

def parsestream(fd, filename):
    """
    Parse a stream of makefile into a parserdata.StatementList. To parse a file system file, use
    parsefile instead of this method.

    @param fd A file-like object containing the makefile data.
    """

    currule = False
    condstack = [parserdata.StatementList()]

    fdlines = enumerate(fd)

    while True:
        assert len(condstack) > 0

        d = DynamicData(fdlines, filename)
        if d.data is None:
            break

        if len(d.data) > 0 and d.data[0] == '\t' and currule:
            e, t, o = parsemakesyntax(d, 1, (), itercommandchars)
            assert t == None
            condstack[-1].append(parserdata.Command(e))
        else:
            # To parse Makefile syntax, we first strip leading whitespace and
            # look for initial keywords. If there are no keywords, it's either
            # setting a variable or writing a rule.

            offset = d.skipwhitespace(0)

            kword, offset = d.findtoken(offset, _directivestokenlist, True)
            if kword == 'endif':
                _ensureend(d, offset, "Unexpected data after 'endif' directive")
                if len(condstack) == 1:
                    raise SyntaxError("unmatched 'endif' directive",
                                      d.getloc(offset))

                condstack.pop()
                continue
            
            if kword == 'else':
                if len(condstack) == 1:
                    raise SyntaxError("unmatched 'else' directive",
                                      d.getloc(offset))

                kword, offset = d.findtoken(offset, _conditionkeywordstokenlist, True)
                if kword is None:
                    _ensureend(d, offset, "Unexpected data after 'else' directive.")
                    condstack[-1].addcondition(d.getloc(offset), parserdata.ElseCondition())
                else:
                    if kword not in _conditionkeywords:
                        raise SyntaxError("Unexpected condition after 'else' directive.",
                                          d.getloc(offset))

                    c = _conditionkeywords[kword](d, offset)
                    condstack[-1].addcondition(d.getloc(offset), c)
                continue

            if kword in _conditionkeywords:
                c = _conditionkeywords[kword](d, offset)
                cb = parserdata.ConditionBlock(d.getloc(0), c)
                condstack[-1].append(cb)
                condstack.append(cb)
                continue

            if kword == 'endef':
                raise SyntaxError("Unmatched endef", d.getloc(offset))

            if kword == 'define':
                currule = False
                vname, t, i = parsemakesyntax(d, offset, (), itermakefilechars)
                vname.rstrip()

                startpos = len(d.data)
                if not d.readline():
                    raise SyntaxError("Unterminated define", d.getloc())

                value = _iterflatten(iterdefinechars, d, startpos)
                condstack[-1].append(parserdata.SetVariable(vname, value=value, valueloc=d.getloc(0), token='=', targetexp=None))
                continue

            if kword in ('include', '-include'):
                currule = False
                incfile, t, offset = parsemakesyntax(d, offset, (), itermakefilechars)
                condstack[-1].append(parserdata.Include(incfile, kword == 'include'))

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
                    raise SyntaxError("Malformed override directive, need =", d.getloc(offset))

                value = _iterflatten(itermakefilechars, d, offset).lstrip()

                condstack[-1].append(parserdata.SetVariable(vname, value=value, valueloc=d.getloc(offset), token=token, targetexp=None, source=data.Variables.SOURCE_OVERRIDE))
                continue

            if kword == 'export':
                currule = False
                e, token, offset = parsemakesyntax(d, offset, _varsettokens, itermakefilechars)
                e.lstrip()
                e.rstrip()

                if token is None:
                    condstack[-1].append(parserdata.ExportDirective(e, single=False))
                else:
                    condstack[-1].append(parserdata.ExportDirective(e, single=True))

                    value = _iterflatten(itermakefilechars, d, offset).lstrip()
                    condstack[-1].append(parserdata.SetVariable(e, value=value, valueloc=d.getloc(offset), token=token, targetexp=None))

                continue

            if kword == 'unexport':
                e, token, offset = parsemakesyntax(d, offset, (), itermakefilechars)
                condstack[-1].append(parserdata.UnexportDirective(e))
                continue

            assert kword is None, "unexpected kword: %r" % (kword,)

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

                value = _iterflatten(itermakefilechars, d, offset).lstrip()

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

                    value = _iterflatten(itermakefilechars, d, offset).lstrip()
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

_functiontokenlist = None

_matchingbrace = {
    '(': ')',
    '{': '}',
    }

def parsemakesyntax(d, startat, stopon, iterfunc):
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

    # print "parsemakesyntax(%r)" % d.data

    global _functiontokenlist
    if _functiontokenlist is None:
        functiontokens = list(functions.functionmap.iterkeys())
        functiontokens.sort(key=len, reverse=True)
        _functiontokenlist = TokenList.get(tuple(functiontokens))

    assert callable(iterfunc)

    stacktop = ParseStackFrame(_PARSESTATE_TOPLEVEL, None, data.Expansion(loc=d.getloc(startat)),
                               tokenlist=TokenList.get(stopon + ('$',)),
                               openbrace=None, closebrace=None)

    di = iterfunc(d, startat, stacktop.tokenlist)
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

        if token == '$':
            if len(d.data) == offset:
                # an unterminated $ expands to nothing
                break

            loc = d.getloc(tokenoffset)

            c = d.data[offset]
            if c == '$':
                stacktop.expansion.appendstr('$')
                offset = offset + 1
            elif c in ('(', '{'):
                closebrace = _matchingbrace[c]

                # look forward for a function name
                fname, offset = d.findtoken(offset + 1, _functiontokenlist, True)
                if fname is not None:
                    fn = functions.functionmap[fname](loc)
                    e = data.Expansion()
                    if len(fn) + 1 == fn.maxargs:
                        tokenlist = TokenList.get((c, closebrace, '$'))
                    else:
                        tokenlist = TokenList.get((',', c, closebrace, '$'))

                    stacktop = ParseStackFrame(_PARSESTATE_FUNCTION, stacktop,
                                               e, tokenlist, function=fn,
                                               openbrace=c, closebrace=closebrace)
                else:
                    e = data.Expansion()
                    tokenlist = TokenList.get((':', c, closebrace, '$'))
                    stacktop = ParseStackFrame(_PARSESTATE_VARNAME, stacktop,
                                               e, tokenlist,
                                               openbrace=c, closebrace=closebrace, loc=loc)
            else:
                e = data.Expansion.fromstring(c, loc)
                stacktop.expansion.appendfunc(functions.VariableRef(loc, e))
                offset += 1
        elif token in ('(', '{'):
            assert token == stacktop.openbrace

            stacktop.expansion.appendstr(token)
            stacktop = ParseStackFrame(_PARSESTATE_PARENMATCH, stacktop,
                                       stacktop.expansion,
                                       TokenList.get((token, stacktop.closebrace,)),
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
                    tokenlist = TokenList.get((stacktop.openbrace, stacktop.closebrace, '$'))
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
                stacktop.tokenlist = TokenList.get(('=', stacktop.openbrace, stacktop.closebrace, '$'))
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
                stacktop.tokenlist = TokenList.get((stacktop.openbrace, stacktop.closebrace, '$'))
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
            di = itermakefilechars(d, offset, stacktop.tokenlist,
                                   ignorecomments=True)
        else:
            di = iterfunc(d, offset, stacktop.tokenlist)

    if stacktop.parent is not None:
        raise SyntaxError("Unterminated function call", d.getloc(offset))

    assert stacktop.parsestate == _PARSESTATE_TOPLEVEL

    return stacktop.expansion.finish(), None, None
