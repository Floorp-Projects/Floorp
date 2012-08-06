# This file contains test code for the formatting of parsed statements back to
# make file "source." It essentially verifies to to_source() functions
# scattered across the tree.

import glob
import logging
import os.path
import unittest

from pymake.data import Expansion
from pymake.data import StringExpansion
from pymake.functions import BasenameFunction
from pymake.functions import SubstitutionRef
from pymake.functions import VariableRef
from pymake.functions import WordlistFunction
from pymake.parserdata import Include
from pymake.parserdata import SetVariable
from pymake.parser import parsestring
from pymake.parser import SyntaxError

class TestBase(unittest.TestCase):
    pass

class VariableRefTest(TestBase):
    def test_string_name(self):
        e = StringExpansion('foo', None)
        v = VariableRef(None, e)

        self.assertEqual(v.to_source(), '$(foo)')

    def test_special_variable(self):
        e = StringExpansion('<', None)
        v = VariableRef(None, e)

        self.assertEqual(v.to_source(), '$<')

    def test_expansion_simple(self):
        e = Expansion()
        e.appendstr('foo')
        e.appendstr('bar')

        v = VariableRef(None, e)

        self.assertEqual(v.to_source(), '$(foobar)')

class StandardFunctionTest(TestBase):
    def test_basename(self):
        e1 = StringExpansion('foo', None)
        v = VariableRef(None, e1)
        e2 = Expansion(None)
        e2.appendfunc(v)

        b = BasenameFunction(None)
        b.append(e2)

        self.assertEqual(b.to_source(), '$(basename $(foo))')

    def test_wordlist(self):
        e1 = StringExpansion('foo', None)
        e2 = StringExpansion('bar ', None)
        e3 = StringExpansion(' baz', None)

        w = WordlistFunction(None)
        w.append(e1)
        w.append(e2)
        w.append(e3)

        self.assertEqual(w.to_source(), '$(wordlist foo,bar , baz)')

    def test_curly_brackets(self):
        e1 = Expansion(None)
        e1.appendstr('foo')

        e2 = Expansion(None)
        e2.appendstr('foo ( bar')

        f = WordlistFunction(None)
        f.append(e1)
        f.append(e2)

        self.assertEqual(f.to_source(), '${wordlist foo,foo ( bar}')

class StringExpansionTest(TestBase):
    def test_simple(self):
        e = StringExpansion('foobar', None)
        self.assertEqual(e.to_source(), 'foobar')

        e = StringExpansion('$var', None)
        self.assertEqual(e.to_source(), '$var')

    def test_escaping(self):
        e = StringExpansion('$var', None)
        self.assertEqual(e.to_source(escape_variables=True), '$$var')

        e = StringExpansion('this is # not a comment', None)
        self.assertEqual(e.to_source(escape_comments=True),
                         'this is \# not a comment')

    def test_empty(self):
        e = StringExpansion('', None)
        self.assertEqual(e.to_source(), '')

        e = StringExpansion(' ', None)
        self.assertEqual(e.to_source(), ' ')

class ExpansionTest(TestBase):
    def test_single_string(self):
        e = Expansion()
        e.appendstr('foo')

        self.assertEqual(e.to_source(), 'foo')

    def test_multiple_strings(self):
        e = Expansion()
        e.appendstr('hello')
        e.appendstr('world')

        self.assertEqual(e.to_source(), 'helloworld')

    def test_string_escape(self):
        e = Expansion()
        e.appendstr('$var')
        self.assertEqual(e.to_source(), '$var')
        self.assertEqual(e.to_source(escape_variables=True), '$$var')

        e = Expansion()
        e.appendstr('foo')
        e.appendstr(' $bar')
        self.assertEqual(e.to_source(escape_variables=True), 'foo $$bar')

class SubstitutionRefTest(TestBase):
    def test_simple(self):
        name = StringExpansion('foo', None)
        c = StringExpansion('%.c', None)
        o = StringExpansion('%.o', None)
        s = SubstitutionRef(None, name, c, o)

        self.assertEqual(s.to_source(), '$(foo:%.c=%.o)')

class SetVariableTest(TestBase):
    def test_simple(self):
        v = SetVariable(StringExpansion('foo', None), '=', 'bar', None, None)
        self.assertEqual(v.to_source(), 'foo = bar')

    def test_multiline(self):
        s = 'hello\nworld'
        foo = StringExpansion('FOO', None)

        v = SetVariable(foo, '=', s, None, None)

        self.assertEqual(v.to_source(), 'define FOO\nhello\nworld\nendef')

    def test_multiline_immediate(self):
        source = 'define FOO :=\nhello\nworld\nendef'

        statements = parsestring(source, 'foo.mk')
        self.assertEqual(statements.to_source(), source)

    def test_target_specific(self):
        foo = StringExpansion('FOO', None)
        bar = StringExpansion('BAR', None)

        v = SetVariable(foo, '+=', 'value', None, bar)

        self.assertEqual(v.to_source(), 'BAR: FOO += value')

class IncludeTest(TestBase):
    def test_include(self):
        e = StringExpansion('rules.mk', None)
        i = Include(e, True, False)
        self.assertEqual(i.to_source(), 'include rules.mk')

        i = Include(e, False, False)
        self.assertEqual(i.to_source(), '-include rules.mk')

class IfdefTest(TestBase):
    def test_simple(self):
        source = 'ifdef FOO\nbar := $(value)\nendif'

        statements = parsestring(source, 'foo.mk')
        self.assertEqual(statements[0].to_source(), source)

    def test_nested(self):
        source = 'ifdef FOO\nifdef BAR\nhello = world\nendif\nendif'

        statements = parsestring(source, 'foo.mk')
        self.assertEqual(statements[0].to_source(), source)

    def test_negation(self):
        source = 'ifndef FOO\nbar += value\nendif'

        statements = parsestring(source, 'foo.mk')
        self.assertEqual(statements[0].to_source(), source)

class IfeqTest(TestBase):
    def test_simple(self):
        source = 'ifeq ($(foo),bar)\nhello = $(world)\nendif'

        statements = parsestring(source, 'foo.mk')
        self.assertEqual(statements[0].to_source(), source)

    def test_negation(self):
        source = 'ifneq (foo,bar)\nhello = world\nendif'

        statements = parsestring(source, 'foo.mk')
        self.assertEqual(statements.to_source(), source)

class ConditionBlocksTest(TestBase):
    def test_mixed_conditions(self):
        source = 'ifdef FOO\nifeq ($(FOO),bar)\nvar += $(value)\nendif\nendif'

        statements = parsestring(source, 'foo.mk')
        self.assertEqual(statements.to_source(), source)

    def test_extra_statements(self):
        source = 'ifdef FOO\nF := 1\nifdef BAR\nB += 1\nendif\nC = 1\nendif'

        statements = parsestring(source, 'foo.mk')
        self.assertEqual(statements.to_source(), source)

    def test_whitespace_preservation(self):
        source = "ifeq ' x' 'x '\n$(error stripping)\nendif"

        statements = parsestring(source, 'foo.mk')
        self.assertEqual(statements.to_source(), source)

        source = 'ifneq (x , x)\n$(error stripping)\nendif'
        statements = parsestring(source, 'foo.mk')
        self.assertEqual(statements.to_source(),
                'ifneq (x,x)\n$(error stripping)\nendif')

class MakefileCorupusTest(TestBase):
    """Runs the make files from the pymake corpus through the formatter.

    All the above tests are child's play compared to this.
    """

    # Our reformatting isn't perfect. We ignore files with known failures until
    # we make them work.
    # TODO Address these formatting corner cases.
    _IGNORE_FILES = [
        # We are thrown off by backslashes at end of lines.
        'comment-parsing.mk',
        'escape-chars.mk',
        'include-notfound.mk',
    ]

    def _get_test_files(self):
        ourdir = os.path.dirname(os.path.abspath(__file__))

        for makefile in glob.glob(os.path.join(ourdir, '*.mk')):
            if os.path.basename(makefile) in self._IGNORE_FILES:
                continue

            source = None
            with open(makefile, 'rU') as fh:
                source = fh.read()

            try:
                yield (makefile, source, parsestring(source, makefile))
            except SyntaxError:
                continue

    def test_reparse_consistency(self):
        for filename, source, statements in self._get_test_files():
            reformatted = statements.to_source()

            # We should be able to parse the reformatted source fine.
            new_statements = parsestring(reformatted, filename)

            # If we do the formatting again, the representation shouldn't
            # change. i.e. the only lossy change should be the original
            # (whitespace and some semantics aren't preserved).
            reformatted_again = new_statements.to_source()
            self.assertEqual(reformatted, reformatted_again,
                '%s has lossless reformat.' % filename)

            self.assertEqual(len(statements), len(new_statements))

            for i in xrange(0, len(statements)):
                original = statements[i]
                formatted = new_statements[i]

                self.assertEqual(original, formatted, '%s %d: %s != %s' % (filename,
                    i, original, formatted))

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    unittest.main()
