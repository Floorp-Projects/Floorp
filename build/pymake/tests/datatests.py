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

if __name__ == '__main__':
    unittest.main()
