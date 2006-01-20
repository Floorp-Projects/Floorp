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
# The Original Code is the Python XPCOM language bindings.
#
# The Initial Developer of the Original Code is ActiveState Tool Corp.
# Portions created by ActiveState Tool Corp. are Copyright (C) 2000, 2001
# ActiveState Tool Corp.  All Rights Reserved.
#
# Contributor(s):
#   Mark Hammond <MarkH@ActiveState.com> (original author)
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

# Test pyxpcom exception.

from xpcom import components, nsError, ServerException, COMException, logger
from xpcom.server import WrapObject
from pyxpcom_test_tools import testmain

import unittest
import logging

class PythonFailingComponent:
    # Re-use the test interface for this test.
    _com_interfaces_ = components.interfaces.nsIPythonTestInterfaceExtra

    def do_boolean(self, p1, p2):
        # This should cause the caller to see a "silent" NS_ERROR_FAILURE exception.
        raise ServerException()

    def do_octet(self, p1, p2):
        # This should cause the caller to see a "silent" NS_ERROR_NOT_IMPLEMENTED exception.
        raise ServerException(nsError.NS_ERROR_NOT_IMPLEMENTED)

    def do_short(self, p1, p2):
        # This should cause the caller to see a "debug" NS_ERROR_FAILURE exception.
        raise COMException(nsError.NS_ERROR_NOT_IMPLEMENTED)

    def do_unsigned_short(self, p1, p2):
        # This should cause the caller to see a "debug" NS_ERROR_FAILURE exception.
        raise "Foo"

    def do_long(self, p1, p2):
        # This should cause the caller to see a "silent" NS_ERROR_FAILURE exception.
        raise ServerException

    def do_unsigned_long(self, p1, p2):
        # This should cause the caller to see a "silent" NS_ERROR_NOT_IMPLEMENTED exception.
        raise ServerException, nsError.NS_ERROR_NOT_IMPLEMENTED

    def do_long_long(self, p1, p2):
        # This should cause the caller to see a "silent" NS_ERROR_NOT_IMPLEMENTED exception.
        raise ServerException, (nsError.NS_ERROR_NOT_IMPLEMENTED, "testing")

    def do_unsigned_long_long(self, p1, p2):
        # Report of a crash in this case - test it!
        raise ServerException, "A bad exception param"

class TestHandler(logging.Handler):
    def __init__(self, level=logging.ERROR): # only counting error records
        logging.Handler.__init__(self, level)
        self.records = []
    
    def reset(self):
        self.records = []

    def handle(self, record):
        self.records.append(record)

class ExceptionTests(unittest.TestCase):

    def _testit(self, expected_errno, num_tracebacks, func, *args):

        # Screw with the logger
        old_handlers = logger.handlers
        test_handler = TestHandler()
        logger.handlers = [test_handler]

        try:
            try:
                apply(func, args)
            except COMException, what:
                if what.errno != expected_errno:
                    raise
        finally:
            logger.handlers = old_handlers
        self.failUnlessEqual(num_tracebacks, len(test_handler.records))

    def testEmAll(self):
        ob = WrapObject( PythonFailingComponent(), components.interfaces.nsIPythonTestInterfaceExtra)
        self._testit(nsError.NS_ERROR_FAILURE, 0, ob.do_boolean, 0, 0)
        self._testit(nsError.NS_ERROR_NOT_IMPLEMENTED, 0, ob.do_octet, 0, 0)
        self._testit(nsError.NS_ERROR_FAILURE, 1, ob.do_short, 0, 0)
        self._testit(nsError.NS_ERROR_FAILURE, 1, ob.do_unsigned_short, 0, 0)
        self._testit(nsError.NS_ERROR_FAILURE, 0, ob.do_long, 0, 0)
        self._testit(nsError.NS_ERROR_NOT_IMPLEMENTED, 0, ob.do_unsigned_long, 0, 0)
        self._testit(nsError.NS_ERROR_NOT_IMPLEMENTED, 0, ob.do_long_long, 0, 0)
        self._testit(nsError.NS_ERROR_FAILURE, 1, ob.do_unsigned_long_long, 0, 0)

if __name__=='__main__':
    testmain()
