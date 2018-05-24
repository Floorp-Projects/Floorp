# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

import shutil
import os
import sys
import random
import copy
from string import letters

'''
Test case infrastructure for MozZipFile.

This isn't really a unit test, but a test case generator and runner.
For a given set of files, lengths, and number of writes, we create
a testcase for every combination of the three. There are some
symmetries used to reduce the number of test cases, the first file
written is always the first file, the second is either the first or
the second, the third is one of the first three. That is, if we
had 4 files, but only three writes, the fourth file would never even
get tried.

The content written to the jars is pseudorandom with a fixed seed.
'''

if not __file__:
    __file__ = sys.argv[0]
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

from MozZipFile import ZipFile
import zipfile

leafs = (
    'firstdir/oneleaf',
    'seconddir/twoleaf',
    'thirddir/with/sub/threeleaf')
_lengths = map(lambda n: n * 64, [16, 64, 80])
lengths = 3
writes = 5


def givenlength(i):
    '''Return a length given in the _lengths array to allow manual
    tuning of which lengths of zip entries to use.
    '''
    return _lengths[i]


def prod(*iterables):
    ''''Tensor product of a list of iterables.

    This generator returns lists of items, one of each given
    iterable. It iterates over all possible combinations.
    '''
    for item in iterables[0]:
        if len(iterables) == 1:
            yield [item]
        else:
            for others in prod(*iterables[1:]):
                yield [item] + others


def getid(descs):
    'Convert a list of ints to a string.'
    return reduce(lambda x, y: x+'{0}{1}'.format(*tuple(y)), descs, '')


def getContent(length):
    'Get pseudo random content of given length.'
    rv = [None] * length
    for i in xrange(length):
        rv[i] = random.choice(letters)
    return ''.join(rv)


def createWriter(sizer, *items):
    'Helper method to fill in tests, one set of writes, one for each item'
    locitems = copy.deepcopy(items)
    for item in locitems:
        item['length'] = sizer(item.pop('length', 0))

    def helper(self):
        mode = 'w'
        if os.path.isfile(self.f):
            mode = 'a'
        zf = ZipFile(self.f, mode, self.compression)
        for item in locitems:
            self._write(zf, **item)
        zf = None
        pass
    return helper


def createTester(name, *writes):
    '''Helper method to fill in tests, calls into a list of write
    helper methods.
    '''
    _writes = copy.copy(writes)

    def tester(self):
        for w in _writes:
            getattr(self, w)()
        self._verifyZip()
        pass
    # unit tests get confused if the method name isn't test...
    tester.__name__ = name
    return tester


class TestExtensiveStored(unittest.TestCase):
    '''Unit tests for MozZipFile

    The testcase are actually populated by code following the class
    definition.
    '''

    stage = "mozzipfilestage"
    compression = zipfile.ZIP_STORED

    def leaf(self, *leafs):
        return os.path.join(self.stage, *leafs)

    def setUp(self):
        if os.path.exists(self.stage):
            shutil.rmtree(self.stage)
        os.mkdir(self.stage)
        self.f = self.leaf('test.jar')
        self.ref = {}
        self.seed = 0

    def tearDown(self):
        self.f = None
        self.ref = None

    def _verifyZip(self):
        zf = zipfile.ZipFile(self.f)
        badEntry = zf.testzip()
        self.failIf(badEntry, badEntry)
        zlist = zf.namelist()
        zlist.sort()
        vlist = self.ref.keys()
        vlist.sort()
        self.assertEqual(zlist, vlist)
        for leaf, content in self.ref.iteritems():
            zcontent = zf.read(leaf)
            self.assertEqual(content, zcontent)

    def _write(self, zf, seed=None, leaf=0, length=0):
        if seed is None:
            seed = self.seed
            self.seed += 1
        random.seed(seed)
        leaf = leafs[leaf]
        content = getContent(length)
        self.ref[leaf] = content
        zf.writestr(leaf, content)
        dir = os.path.dirname(self.leaf('stage', leaf))
        if not os.path.isdir(dir):
            os.makedirs(dir)
        open(self.leaf('stage', leaf), 'w').write(content)


# all leafs in all lengths
atomics = list(prod(xrange(len(leafs)), xrange(lengths)))

# populate TestExtensiveStore with testcases
for w in xrange(writes):
    # Don't iterate over all files for the the first n passes,
    # those are redundant as long as w < lengths.
    # There are symmetries in the trailing end, too, but I don't know
    # how to reduce those out right now.
    nonatomics = [list(prod(range(min(i, len(leafs))), xrange(lengths)))
                  for i in xrange(1, w+1)] + [atomics]
    for descs in prod(*nonatomics):
        suffix = getid(descs)
        dicts = [dict(leaf=leaf, length=length) for leaf, length in descs]
        setattr(TestExtensiveStored, '_write' + suffix,
                createWriter(givenlength, *dicts))
        setattr(TestExtensiveStored, 'test' + suffix,
                createTester('test' + suffix, '_write' + suffix))

# now create another round of tests, with two writing passes
# first, write all file combinations into the jar, close it,
# and then write all atomics again.
# This should catch more or less all artifacts generated
# by the final ordering step when closing the jar.
files = [list(prod([i], xrange(lengths))) for i in xrange(len(leafs))]
allfiles = reduce(lambda l, r: l+r,
                  [list(prod(*files[:(i+1)])) for i in xrange(len(leafs))])

for first in allfiles:
    testbasename = 'test{0}_'.format(getid(first))
    test = [None, '_write' + getid(first), None]
    for second in atomics:
        test[0] = testbasename + getid([second])
        test[2] = '_write' + getid([second])
        setattr(TestExtensiveStored, test[0], createTester(*test))


class TestExtensiveDeflated(TestExtensiveStored):
    'Test all that has been tested with ZIP_STORED with DEFLATED, too.'
    compression = zipfile.ZIP_DEFLATED


if __name__ == '__main__':
    unittest.main()
