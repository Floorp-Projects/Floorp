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

from xpcom import components, nsError, ServerException, COMException
from xpcom.server import WrapObject

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

def _testit(expected_errno, func, *args):
    try:
        apply(func, args)
    except COMException, what:
        if what.errno != expected_errno:
            raise

def test():
    # For the benefit of the test suite, we print some reassuring messages.
    import sys
    sys.__stderr__.write("***** NOTE: Three tracebacks below this is normal\n")
    ob = WrapObject( PythonFailingComponent(), components.interfaces.nsIPythonTestInterfaceExtra)
    _testit(nsError.NS_ERROR_FAILURE, ob.do_boolean, 0, 0)
    _testit(nsError.NS_ERROR_NOT_IMPLEMENTED, ob.do_octet, 0, 0)
    _testit(nsError.NS_ERROR_FAILURE, ob.do_short, 0, 0)
    _testit(nsError.NS_ERROR_FAILURE, ob.do_unsigned_short, 0, 0)
    _testit(nsError.NS_ERROR_FAILURE, ob.do_long, 0, 0)
    _testit(nsError.NS_ERROR_NOT_IMPLEMENTED, ob.do_unsigned_long, 0, 0)
    _testit(nsError.NS_ERROR_NOT_IMPLEMENTED, ob.do_long_long, 0, 0)
    _testit(nsError.NS_ERROR_FAILURE, ob.do_unsigned_long_long, 0, 0)
    print "The xpcom exception tests passed"
    # For the benefit of the test suite, some more reassuring messages.
    sys.__stderr__.write("***** NOTE: Three tracebacks printed above this is normal\n")
    sys.__stderr__.write("*****       It is testing the Python XPCOM Exception semantics\n")

test()