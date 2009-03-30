import pymake.data, pymake.parser, pymake.parserdata, pymake.functions
import unittest
import logging

from cStringIO import StringIO

def multitest(cls):
    for name in cls.testdata.iterkeys():
        def m(self, name=name):
            return self.runSingle(*self.testdata[name])

        setattr(cls, 'test_%s' % name, m)
    return cls

class TestBase(unittest.TestCase):
    def assertEqual(self, a, b, msg=""):
        """Actually print the values which weren't equal, if things don't work out!"""
        unittest.TestCase.assertEqual(self, a, b, "%s got %r expected %r" % (msg, a, b))

class DataTest(TestBase):
    testdata = {
        'oneline':
            ("He\tllo", "f", 1, 0,
             ((0, "f", 1, 0), (2, "f", 1, 2), (3, "f", 1, 4))),
        'twoline':
            ("line1 \n\tl\tine2", "f", 1, 4,
             ((0, "f", 1, 4), (5, "f", 1, 9), (6, "f", 1, 10), (7, "f", 2, 0), (8, "f", 2, 4), (10, "f", 2, 8), (13, "f", 2, 11))),
    }

    def runSingle(self, data, filename, line, col, results):
        d = pymake.parser.Data(pymake.parserdata.Location(filename, line, col),
                               data)
        for pos, file, lineno, col in results:
            loc = d.getloc(pos)
            self.assertEqual(loc.path, file, "data file offset %i" % pos)
            self.assertEqual(loc.line, lineno, "data line offset %i" % pos)
            self.assertEqual(loc.column, col, "data col offset %i" % pos)
multitest(DataTest)

class TokenTest(TestBase):
    testdata = {
        'wsmatch': ('  ifdef FOO', 2, ('ifdef', 'else'), True, 'ifdef', 8),
        'wsnomatch': ('  unexpected FOO', 2, ('ifdef', 'else'), True, None, 2),
        'wsnows': ('  ifdefFOO', 2, ('ifdef', 'else'), True, None, 2),
        'paren': (' "hello"', 1, ('(', "'", '"'), False, '"', 2),
        }

    def runSingle(self, s, start, tlist, needws, etoken, eoffset):
        d = pymake.parser.Data.fromstring(s, None)
        tl = pymake.parser.TokenList.get(tlist)
        atoken, aoffset = d.findtoken(start, tl, needws)
        self.assertEqual(atoken, etoken)
        self.assertEqual(aoffset, eoffset)
multitest(TokenTest)

class IterTest(TestBase):
    testdata = {
        'plaindata': (
            pymake.parser.iterdata,
            "plaindata # test\n",
            "plaindata # test\n"
            ),
        'makecomment': (
            pymake.parser.itermakefilechars,
            "VAR = val # comment",
            "VAR = val "
            ),
        'makeescapedcomment': (
            pymake.parser.itermakefilechars,
            "VAR = val \# escaped hash\n",
            "VAR = val # escaped hash"
            ),
        'makeescapedslash': (
            pymake.parser.itermakefilechars,
            "VAR = val\\\\\n",
            "VAR = val\\\\",
            ),
        'makecontinuation': (
            pymake.parser.itermakefilechars,
            "VAR = VAL  \\\n  continuation # comment \\\n  continuation",
            "VAR = VAL continuation "
            ),
        'makecontinuation2': (
            pymake.parser.itermakefilechars,
            "VAR = VAL  \\  \\\n continuation",
            "VAR = VAL  \\ continuation"
            ),
        'makeawful': (
            pymake.parser.itermakefilechars,
            "VAR = VAL  \\\\# comment\n",
            "VAR = VAL  \\"
            ),
        'command': (
            pymake.parser.itercommandchars,
            "echo boo # comment\n",
            "echo boo # comment",
            ),
        'commandcomment': (
            pymake.parser.itercommandchars,
            "echo boo \# comment\n",
            "echo boo \# comment",
            ),
        'commandcontinue': (
            pymake.parser.itercommandchars,
            "echo boo # \\\n\t  command 2\n",
            "echo boo # \\\n  command 2"
            ),
        'define': (
            pymake.parser.iterdefinechars,
            "endef",
            ""
            ),
        'definenesting': (
            pymake.parser.iterdefinechars,
            """define BAR # comment
random text
endef not what you think!
endef # comment is ok\n""",
            """define BAR # comment
random text
endef not what you think!"""
            ),
        'defineescaped': (
            pymake.parser.iterdefinechars,
            """value   \\
endef
endef\n""",
            "value endef"
        ),
    }

    def runSingle(self, ifunc, idata, expected):
        fd = StringIO(idata)
        lineiter = enumerate(fd)

        d = pymake.parser.DynamicData(lineiter, 'PlainIterTest-data')

        actual = ''.join( (c for c, t, o, oo in ifunc(d, 0, pymake.parser._emptytokenlist)) )
        self.assertEqual(actual, expected)

        self.assertRaises(StopIteration, lambda: fd.next())
multitest(IterTest)

class MakeSyntaxTest(TestBase):
    # (string, startat, stopat, stopoffset, expansion
    testdata = {
        'text': ('hello world', 0, (), None, ['hello world']),
        'singlechar': ('hello $W', 0, (), None,
                       ['hello ',
                        {'type': 'VariableRef',
                         '.vname': ['W']}
                        ]),
        'stopat': ('hello: world', 0, (':', '='), 6, ['hello']),
        'funccall': ('h $(flavor FOO)', 0, (), None,
                     ['h ',
                      {'type': 'FlavorFunction',
                       '[0]': ['FOO']}
                      ]),
        'escapedollar': ('hello$$world', 0, (), None, ['hello$world']),
        'varref': ('echo $(VAR)', 0, (), None,
                   ['echo ',
                    {'type': 'VariableRef',
                     '.vname': ['VAR']}
                    ]),
        'dynamicvarname': ('echo $($(VARNAME):.c=.o)', 0, (':',), None,
                           ['echo ',
                            {'type': 'SubstitutionRef',
                             '.vname': [{'type': 'VariableRef',
                                         '.vname': ['VARNAME']}
                                        ],
                             '.substfrom': ['.c'],
                             '.substto': ['.o']}
                            ]),
        'substref': ('  $(VAR:VAL) := $(VAL)', 0, (':=', '+=', '=', ':'), 15,
                     ['  ',
                      {'type': 'VariableRef',
                       '.vname': ['VAR:VAL']},
                      ' ']),
        'vadsubstref': ('  $(VAR:VAL) = $(VAL)', 15, (), None,
                        [{'type': 'VariableRef',
                          '.vname': ['VAL']},
                         ]),
        }

    def compareRecursive(self, actual, expected, path):
        self.assertEqual(len(actual), len(expected),
                         "compareRecursive: %s" % (path,))
        for i in xrange(0, len(actual)):
            ipath = path + [i]

            a, isfunc = actual[i]
            e = expected[i]
            if isinstance(e, str):
                self.assertEqual(a, e, "compareRecursive: %s" % (ipath,))
            else:
                self.assertEqual(type(a), getattr(pymake.functions, e['type']),
                                 "compareRecursive: %s" % (ipath,))
                for k, v in e.iteritems():
                    if k == 'type':
                        pass
                    elif k[0] == '[':
                        item = int(k[1:-1])
                        proppath = ipath + [item]
                        self.compareRecursive(a[item], v, proppath)
                    elif k[0] == '.':
                        item = k[1:]
                        proppath = ipath + [item]
                        self.compareRecursive(getattr(a, item), v, proppath)
                    else:
                        raise Exception("Unexpected property at %s: %s" % (ipath, k))

    def runSingle(self, s, startat, stopat, stopoffset, expansion):
        d = pymake.parser.Data.fromstring(s, pymake.parserdata.Location('testdata', 1, 0))

        a, t, offset = pymake.parser.parsemakesyntax(d, startat, stopat, pymake.parser.itermakefilechars)
        self.compareRecursive(a, expansion, [])
        self.assertEqual(offset, stopoffset)

multitest(MakeSyntaxTest)

class VariableTest(TestBase):
    testdata = """
    VAR = value
    VARNAME = TESTVAR
    $(VARNAME) = testvalue
    $(VARNAME:VAR=VAL) = moretesting
    IMM := $(VARNAME) # this is a comment
    MULTIVAR = val1 \\
  val2
    VARNAME = newname
    """
    expected = {'VAR': 'value',
                'VARNAME': 'newname',
                'TESTVAR': 'testvalue',
                'TESTVAL': 'moretesting',
                'IMM': 'TESTVAR ',
                'MULTIVAR': 'val1 val2',
                'UNDEF': None}

    def runTest(self):
        stream = StringIO(self.testdata)
        stmts = pymake.parser.parsestream(stream, 'testdata')

        m = pymake.data.Makefile()
        stmts.execute(m)
        for k, v in self.expected.iteritems():
            flavor, source, val = m.variables.get(k)
            if val is None:
                self.assertEqual(val, v, 'variable named %s' % k)
            else:
                self.assertEqual(val.resolvestr(m, m.variables), v, 'variable named %s' % k)

class SimpleRuleTest(TestBase):
    testdata = """
    VAR = value
TSPEC = dummy
all: TSPEC = myrule
all:: test test2 $(VAR)
	echo "Hello, $(TSPEC)"

%.o: %.c
	$(CC) -o $@ $<
"""

    def runTest(self):
        stream = StringIO(self.testdata)
        stmts = pymake.parser.parsestream(stream, 'testdata')

        m = pymake.data.Makefile()
        stmts.execute(m)
        self.assertEqual(m.defaulttarget, 'all', "Default target")

        self.assertTrue(m.hastarget('all'), "Has 'all' target")
        target = m.gettarget('all')
        rules = target.rules
        self.assertEqual(len(rules), 1, "Number of rules")
        prereqs = rules[0].prerequisites
        self.assertEqual(prereqs, ['test', 'test2', 'value'], "Prerequisites")
        commands = rules[0].commands
        self.assertEqual(len(commands), 1, "Number of commands")
        expanded = commands[0].resolvestr(m, target.variables)
        self.assertEqual(expanded, 'echo "Hello, myrule"')

        irules = m.implicitrules
        self.assertEqual(len(irules), 1, "Number of implicit rules")

        irule = irules[0]
        self.assertEqual(len(irule.targetpatterns), 1, "%.o target pattern count")
        self.assertEqual(len(irule.prerequisites), 1, "%.o prerequisite count")
        self.assertEqual(irule.targetpatterns[0].match('foo.o'), 'foo', "%.o stem")

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    unittest.main()
