import pymake.data, pymake.functions, pymake.util
import unittest
import re
from cStringIO import StringIO

def multitest(cls):
    for name in cls.testdata.iterkeys():
        def m(self, name=name):
            return self.runSingle(*self.testdata[name])

        setattr(cls, 'test_%s' % name, m)
    return cls

class SplitWordsTest(unittest.TestCase):
    testdata = (
        (' test test.c test.o ', ['test', 'test.c', 'test.o']),
        ('\ttest\t  test.c \ntest.o', ['test', 'test.c', 'test.o']),
    )

    def runTest(self):
        for s, e in self.testdata:
            w = s.split()
            self.assertEqual(w, e, 'splitwords(%r)' % (s,))

class GetPatSubstTest(unittest.TestCase):
    testdata = (
        ('%.c', '%.o', ' test test.c test.o ', 'test test.o test.o'),
        ('%', '%.o', ' test.c test.o ', 'test.c.o test.o.o'),
        ('foo', 'bar', 'test foo bar', 'test bar bar'),
        ('foo', '%bar', 'test foo bar', 'test %bar bar'),
        ('%', 'perc_%', 'path', 'perc_path'),
        ('\\%', 'sub%', 'p %', 'p sub%'),
        ('%.c', '\\%%.o', 'foo.c bar.o baz.cpp', '%foo.o bar.o baz.cpp'),
    )

    def runTest(self):
        for s, r, d, e in self.testdata:
            words = d.split()
            p = pymake.data.Pattern(s)
            a = ' '.join((p.subst(r, word, False)
                          for word in words))
            self.assertEqual(a, e, 'Pattern(%r).subst(%r, %r)' % (s, r, d))

class LRUTest(unittest.TestCase):
    # getkey, expected, funccount, debugitems
    expected = (
        (0, '', 1, (0,)),
        (0, '', 2, (0,)),
        (1, ' ', 3, (1, 0)),
        (1, ' ', 3, (1, 0)),
        (0, '', 4, (0, 1)),
        (2, '  ', 5, (2, 0, 1)),
        (1, ' ', 5, (1, 2, 0)),
        (3, '   ', 6, (3, 1, 2)),
    )

    def spaceFunc(self, l):
        self.funccount += 1
        return ''.ljust(l)

    def runTest(self):
        self.funccount = 0
        c = pymake.util.LRUCache(3, self.spaceFunc, lambda k, v: k % 2)
        self.assertEqual(tuple(c.debugitems()), ())

        for i in xrange(0, len(self.expected)):
            k, e, fc, di = self.expected[i]

            v = c.get(k)
            self.assertEqual(v, e)
            self.assertEqual(self.funccount, fc,
                             "funccount, iteration %i, got %i expected %i" % (i, self.funccount, fc))
            goti = tuple(c.debugitems())
            self.assertEqual(goti, di,
                             "debugitems, iteration %i, got %r expected %r" % (i, goti, di))

class EqualityTest(unittest.TestCase):
    def test_string_expansion(self):
        s1 = pymake.data.StringExpansion('foo bar', None)
        s2 = pymake.data.StringExpansion('foo bar', None)

        self.assertEqual(s1, s2)

    def test_expansion_simple(self):
        s1 = pymake.data.Expansion(None)
        s2 = pymake.data.Expansion(None)

        self.assertEqual(s1, s2)

        s1.appendstr('foo')
        s2.appendstr('foo')
        self.assertEqual(s1, s2)

    def test_expansion_string_finish(self):
        """Adjacent strings should normalize to same value."""
        s1 = pymake.data.Expansion(None)
        s2 = pymake.data.Expansion(None)

        s1.appendstr('foo')
        s2.appendstr('foo')

        s1.appendstr(' bar')
        s1.appendstr(' baz')
        s2.appendstr(' bar baz')

        self.assertEqual(s1, s2)

    def test_function(self):
        s1 = pymake.data.Expansion(None)
        s2 = pymake.data.Expansion(None)

        n1 = pymake.data.StringExpansion('FOO', None)
        n2 = pymake.data.StringExpansion('FOO', None)

        v1 = pymake.functions.VariableRef(None, n1)
        v2 = pymake.functions.VariableRef(None, n2)

        s1.appendfunc(v1)
        s2.appendfunc(v2)

        self.assertEqual(s1, s2)


class StringExpansionTest(unittest.TestCase):
    def test_base_expansion_interface(self):
        s1 = pymake.data.StringExpansion('FOO', None)

        self.assertTrue(s1.is_static_string)

        funcs = list(s1.functions())
        self.assertEqual(len(funcs), 0)

        funcs = list(s1.functions(True))
        self.assertEqual(len(funcs), 0)

        refs = list(s1.variable_references())
        self.assertEqual(len(refs), 0)


class ExpansionTest(unittest.TestCase):
    def test_is_static_string(self):
        e1 = pymake.data.Expansion()
        e1.appendstr('foo')

        self.assertTrue(e1.is_static_string)

        e1.appendstr('bar')
        self.assertTrue(e1.is_static_string)

        vname = pymake.data.StringExpansion('FOO', None)
        func = pymake.functions.VariableRef(None, vname)

        e1.appendfunc(func)

        self.assertFalse(e1.is_static_string)

    def test_get_functions(self):
        e1 = pymake.data.Expansion()
        e1.appendstr('foo')

        vname1 = pymake.data.StringExpansion('FOO', None)
        vname2 = pymake.data.StringExpansion('BAR', None)

        func1 = pymake.functions.VariableRef(None, vname1)
        func2 = pymake.functions.VariableRef(None, vname2)

        e1.appendfunc(func1)
        e1.appendfunc(func2)

        funcs = list(e1.functions())
        self.assertEqual(len(funcs), 2)

        func3 = pymake.functions.SortFunction(None)
        func3.append(vname1)

        e1.appendfunc(func3)

        funcs = list(e1.functions())
        self.assertEqual(len(funcs), 3)

        refs = list(e1.variable_references())
        self.assertEqual(len(refs), 2)

    def test_get_functions_descend(self):
        e1 = pymake.data.Expansion()
        vname1 = pymake.data.StringExpansion('FOO', None)
        func1 = pymake.functions.VariableRef(None, vname1)
        e2 = pymake.data.Expansion()
        e2.appendfunc(func1)

        func2 = pymake.functions.SortFunction(None)
        func2.append(e2)

        e1.appendfunc(func2)

        funcs = list(e1.functions())
        self.assertEqual(len(funcs), 1)

        funcs = list(e1.functions(True))
        self.assertEqual(len(funcs), 2)

        self.assertTrue(isinstance(funcs[0], pymake.functions.SortFunction))

    def test_is_filesystem_dependent(self):
        e = pymake.data.Expansion()
        vname1 = pymake.data.StringExpansion('FOO', None)
        func1 = pymake.functions.VariableRef(None, vname1)
        e.appendfunc(func1)

        self.assertFalse(e.is_filesystem_dependent)

        func2 = pymake.functions.WildcardFunction(None)
        func2.append(vname1)
        e.appendfunc(func2)

        self.assertTrue(e.is_filesystem_dependent)

    def test_is_filesystem_dependent_descend(self):
        sort = pymake.functions.SortFunction(None)
        wildcard = pymake.functions.WildcardFunction(None)

        e = pymake.data.StringExpansion('foo/*', None)
        wildcard.append(e)

        e = pymake.data.Expansion(None)
        e.appendfunc(wildcard)

        sort.append(e)

        e = pymake.data.Expansion(None)
        e.appendfunc(sort)

        self.assertTrue(e.is_filesystem_dependent)


if __name__ == '__main__':
    unittest.main()
