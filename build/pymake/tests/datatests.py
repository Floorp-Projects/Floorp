import pymake.data, pymake.util
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

if __name__ == '__main__':
    unittest.main()
