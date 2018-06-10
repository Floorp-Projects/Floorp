import copy
import re
import os
import subprocess
import sys
import tempfile

# We test the compiler indirectly, rather than reaching into the ipdl/
# module, to make the testing framework as general as possible.


class IPDLCompile:
    def __init__(self, specfilename, ipdlargv=['python', 'ipdl.py']):
        self.argv = copy.deepcopy(ipdlargv)
        self.specfilename = specfilename
        self.stdout = None
        self.stderr = None
        self.returncode = None

    def run(self):
        '''Run |self.specstring| through the IPDL compiler.'''
        assert self.returncode is None

        tmpoutdir = tempfile.mkdtemp(prefix='ipdl_unit_test')

        try:
            self.argv.extend([
                '-d', tmpoutdir,
                self.specfilename
            ])

            proc = subprocess.Popen(args=self.argv,
                                    stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE)
            self.stdout, self.stderr = proc.communicate()

            self.returncode = proc.returncode
            assert self.returncode is not None

        finally:
            for root, dirs, files in os.walk(tmpoutdir, topdown=0):
                for name in files:
                    os.remove(os.path.join(root, name))
                for name in dirs:
                    os.rmdir(os.path.join(root, name))
            os.rmdir(tmpoutdir)

            if proc.returncode is None:
                proc.kill()

    def completed(self):
        return (self.returncode is not None
                and isinstance(self.stdout, str)
                and isinstance(self.stderr, str))

    def error(self, expectedError):
        '''Return True iff compiling self.specstring resulted in an
IPDL compiler error.'''
        assert self.completed()

        errorRe = re.compile(re.escape(expectedError))
        return None is not re.search(errorRe, self.stderr)

    def exception(self):
        '''Return True iff compiling self.specstring resulted in a Python
exception being raised.'''
        assert self.completed()

        return None is not re.search(r'Traceback (most recent call last):',
                                     self.stderr)

    def ok(self):
        '''Return True iff compiling self.specstring was successful.'''
        assert self.completed()

        return (not self.exception()
                and not self.error("error:")
                and (0 == self.returncode))
