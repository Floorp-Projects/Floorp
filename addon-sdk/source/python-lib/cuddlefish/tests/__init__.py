# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import unittest
import doctest
import glob

env_root = os.environ['CUDDLEFISH_ROOT']

def get_tests():
    import cuddlefish
    import cuddlefish.tests

    tests = []
    packages = [cuddlefish, cuddlefish.tests]
    for package in packages:
        path = os.path.abspath(package.__path__[0])
        pynames = glob.glob(os.path.join(path, '*.py'))
        for filename in pynames:
            basename = os.path.basename(filename)
            module_name = os.path.splitext(basename)[0]
            full_name = "%s.%s" % (package.__name__, module_name)
            module = __import__(full_name, fromlist=[package.__name__])

            loader = unittest.TestLoader()
            suite = loader.loadTestsFromModule(module)
            for test in suite:
                tests.append(test)

            finder = doctest.DocTestFinder()
            doctests = finder.find(module)
            for test in doctests:
                if len(test.examples) > 0:
                    tests.append(doctest.DocTestCase(test))

    md_dir = os.path.join(env_root, 'dev-guide')
    doctest_opts = (doctest.NORMALIZE_WHITESPACE |
                    doctest.REPORT_UDIFF)
    for dirpath, dirnames, filenames in os.walk(md_dir):
        for filename in filenames:
            if filename.endswith('.md'):
                absname = os.path.join(dirpath, filename)
                tests.append(doctest.DocFileTest(
                        absname,
                        module_relative=False,
                        optionflags=doctest_opts
                        ))

    return tests

def run(verbose=False):
    if verbose:
        verbosity = 2
    else:
        verbosity = 1

    tests = get_tests()
    suite = unittest.TestSuite(tests)
    runner = unittest.TextTestRunner(verbosity=verbosity)
    return runner.run(suite)

if __name__ == '__main__':
    run()
