import os
import unittest

from IPDLCompile import IPDLCompile


class IPDLTestCase(unittest.TestCase):
    def __init__(self, ipdlargv, filename):
        unittest.TestCase.__init__(self, 'test')
        self.filename = filename
        self.compile = IPDLCompile(filename, ipdlargv)

    def test(self):
        self.compile.run()
        self.assertFalse(self.compile.exception(), self.mkFailMsg())
        self.checkPassed()

    def mkCustomMsg(self, msg):
        return '''
### Command: %s
### %s
### stderr:
%s''' % (' '.join(self.compile.argv), msg, self.compile.stderr)

    def mkFailMsg(self):
        return '''
### Command: %s
### stderr:
%s''' % (' '.join(self.compile.argv), self.compile.stderr)

    def shortDescription(self):
        return '%s test of "%s"' % (self.__class__.__name__, self.filename)


class OkTestCase(IPDLTestCase):
    '''An invocation of the IPDL compiler on a valid specification.
The IPDL compiler should not produce errors or exceptions.'''

    def __init__(self, ipdlargv, filename):
        IPDLTestCase.__init__(self, ipdlargv, filename)

    def checkPassed(self):
        self.assertTrue(self.compile.ok(), self.mkFailMsg())


class ErrorTestCase(IPDLTestCase):
    '''An invocation of the IPDL compiler on an *invalid* specification.
The IPDL compiler *should* produce errors but not exceptions.'''

    def __init__(self, ipdlargv, filename):
        IPDLTestCase.__init__(self, ipdlargv, filename)

        # Look for expected errors in the input file.
        f = open(filename, 'r')
        self.expectedErrorMessage = []
        for l in f:
            if l.startswith("//error:"):
                self.expectedErrorMessage.append(l[2:-1])
        f.close()

    def checkPassed(self):
        self.assertNotEqual(self.expectedErrorMessage, [],
                            self.mkCustomMsg("Error test should contain at least " +
                                             "one line starting with //error: " +
                                             "that indicates the expected failure."))

        for e in self.expectedErrorMessage:
            self.assertTrue(self.compile.error(e),
                            self.mkCustomMsg('Did not see expected error "' +
                                             e + '"'))


if __name__ == '__main__':
    import sys

    okdir = sys.argv[1]
    assert os.path.isdir(okdir)
    errordir = sys.argv[2]
    assert os.path.isdir(errordir)

    ipdlargv = []
    oksuite = unittest.TestSuite()
    errorsuite = unittest.TestSuite()

    oktests, errortests = 0, 0
    for arg in sys.argv[3:]:
        if errortests:
            errorsuite.addTest(ErrorTestCase(ipdlargv + ['-I', errordir],
                                             arg))
        elif oktests:
            if 'ERRORTESTS' == arg:
                errortests = 1
                continue
            oksuite.addTest(OkTestCase(ipdlargv + ['-I', okdir],
                                       arg))
        else:
            if 'OKTESTS' == arg:
                oktests = 1
                continue
            ipdlargv.append(arg)

    (unittest.TextTestRunner()).run(
        unittest.TestSuite([oksuite, errorsuite]))
