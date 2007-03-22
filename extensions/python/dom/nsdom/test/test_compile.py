# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code
#
# The Initial Developer of the Original Code is mozilla.org.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Mark Hammond: Initial Author
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# The Regression Tests for the nsdom package.
import sys
import unittest
import new
from nsdom.domcompile import compile, compile_function

class TestCompile(unittest.TestCase):
    def testSyntaxError(self):
        globs = {}
        try:
            co = compile(u"\ndef class()", "foo.py", lineno=100)
            raise AssertionError, "not reached!"
        except SyntaxError, why:
            self.failUnlessEqual(why.lineno, 102)
            self.failUnlessEqual(why.filename, "foo.py")

    def testFilenameAndOffset(self):
        src = u"a=1\nraise RuntimeError"
        globs = {}
        co = compile_function(src, "foo.py", "func_name", ["x", "y", "z"], 
                              lineno=100)
        exec co in globs
        f = globs['func_name']
        # re-create the function - so we can bind to different globals
        # Note we need to manually setup __builtins__ for this new globs.
        globs = {'g':'call_globs', '__debug__': __debug__,
                 "__builtins__": globs["__builtins__"]}
        f = new.function(f.func_code, globs, f.func_name)
        try:
            f(1,2,3)
            raise AssertionError, "not reached!"
        except RuntimeError:
            # The function line number is 1 frames back. It should point to
            # line 102
            tb = sys.exc_info()[2].tb_next
            self.failUnlessEqual(tb.tb_frame.f_lineno, 102)
            self.failUnlessEqual(tb.tb_frame.f_code.co_filename, "foo.py")

    def testFunction(self):
        # Compile a test function, then execute it.
        src = u"assert g=='call_globs'\nreturn 'wow'"
   
        globs = {'g':'compile_globs'}
        co = compile_function(src, "foo.py", "func_name", ["x", "y", "z"], 
                              lineno=100)
        exec co in globs
        f = globs['func_name']
        # re-create the function - so we can bind to different globals
        globs = {'g':'call_globs', '__debug__': __debug__}
        f = new.function(f.func_code, globs, f.func_name)
        self.failUnlessEqual(f(1,2,3), 'wow')

    def testFunctionArgs(self):
        # Compile a test function, then execute it.
        src = u"assert a==1\nassert b=='b'\nassert c is None\nreturn 'yes'"
   
        defs = (1, "b", None)
        co = compile_function(src, "foo.py", "func_name",
                              ["a", "b", "c"],
                              defs,
                              lineno=100)
        globs = {}
        exec co in globs
        f = globs['func_name']
        # re-create the function - so we can bind to different globals
        globs = {'__debug__': __debug__}
        f = new.function(f.func_code, globs, f.func_name, defs)
        self.failUnlessEqual(f(), 'yes')

    def testSimple(self):
        globs = {}
        # And the simple compile.
        co = compile(u"a=1", "foo.py")
        exec co in globs
        self.failUnlessEqual(globs['a'], 1)

    def testTrailingWhitespace(self):
        globs = {}
        # And the simple compile.
        co = compile(u"a=1\n  ", "foo.py")
        exec co in globs
        self.failUnlessEqual(globs['a'], 1)

    def _testNewlines(self, sep):
        src = u"a=1" + sep + "b=2"
        globs = {}
        co = compile(src, "foo.py")
        exec co in globs
        self.failUnlessEqual(globs['a'], 1)
        self.failUnlessEqual(globs['b'], 2)
        
    def testMacNewlines(self):
        self._testNewlines("\r")

    def testWindowsNewlines(self):
        self._testNewlines("\r\n")

    def testUnixNewlines(self):
        self._testNewlines("\n")

if __name__=='__main__':
    unittest.main()
