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
# The Initial Developer of the Original Code is
# ActiveState Tool Corp.
# Portions created by the Initial Developer are Copyright (C) 2000, 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#        Mark Hammond <MarkH@ActiveState.com> (original author)
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

# Test our support for the interfaces defined in nsISupportsPrimitives.idl
#
# The framework supports nsISupportsCString and nsISupportsString, but
# only if our class doesn't provide explicit support.

from xpcom import components
from xpcom import primitives
import xpcom.server, xpcom.client
from pyxpcom_test_tools import testmain
import unittest

class NoSupportsString:
    _com_interfaces_ = [components.interfaces.nsISupports]

class ImplicitSupportsString:
    _com_interfaces_ = [components.interfaces.nsISupports]
    def __str__(self):
        return "<MyImplicitStrObject>"

class ExplicitSupportsString:
    _com_interfaces_ = [components.interfaces.nsISupportsPrimitive,
                        components.interfaces.nsISupportsCString]
    type = components.interfaces.nsISupportsPrimitive.TYPE_CSTRING
    test_data = "<MyExplicitStrObject>"
    # __str__ will be ignored by XPCOM, as we have _explicit_ support.
    def __str__(self):
        return "<MyImplicitStrObject>"
    # These are the ones that will be used.
    def get_data(self):
        return self.test_data
    def toString(self):
        return self.test_data

class ImplicitSupportsUnicode:
    _com_interfaces_ = [components.interfaces.nsISupports]
    test_data = u"Copyright \xa9 the initial developer"
    def __unicode__(self):
        # An extended character in unicode tests can't hurt!
        return self.test_data

class ExplicitSupportsUnicode:
    _com_interfaces_ = [components.interfaces.nsISupportsPrimitive,
                        components.interfaces.nsISupportsString]
    type = components.interfaces.nsISupportsPrimitive.TYPE_STRING
    # __unicode__ will be ignored by XPCOM, as we have _explicit_ support.
    test_data = u"Copyright \xa9 the initial developer"
    def __unicode__(self):
        return self.test_data
    def get_data(self):
        return self.test_data

class ImplicitSupportsInt:
    _com_interfaces_ = [components.interfaces.nsISupports]
    def __int__(self):
        return 99

class ExplicitSupportsInt:
    _com_interfaces_ = [components.interfaces.nsISupportsPrimitive,
                        components.interfaces.nsISupportsPRInt32]
    type = components.interfaces.nsISupportsPrimitive.TYPE_PRINT32
    def get_data(self):
        return 99

class ImplicitSupportsLong:
    _com_interfaces_ = [components.interfaces.nsISupports]
    def __long__(self):
        return 99L

class ExplicitSupportsLong:
    _com_interfaces_ = [components.interfaces.nsISupportsPrimitive,
                        components.interfaces.nsISupportsPRInt64]
    type = components.interfaces.nsISupportsPrimitive.TYPE_PRINT64
    def get_data(self):
        return 99

class ExplicitSupportsFloat:
    _com_interfaces_ = [components.interfaces.nsISupportsPrimitive,
                        components.interfaces.nsISupportsDouble]
    type = components.interfaces.nsISupportsPrimitive.TYPE_DOUBLE
    def get_data(self):
        return 99.99

class ImplicitSupportsFloat:
    _com_interfaces_ = [components.interfaces.nsISupports]
    def __float__(self):
        return 99.99

class PrimitivesTestCase(unittest.TestCase):
    def testNoSupports(self):
        ob = xpcom.server.WrapObject( NoSupportsString(), components.interfaces.nsISupports)
        if not str(ob).startswith("<XPCOM "):
            raise RuntimeError, "Wrong str() value: %s" % (ob,)

    def testImplicitString(self):
        ob = xpcom.server.WrapObject( ImplicitSupportsString(), components.interfaces.nsISupports)
        self.failUnlessEqual(str(ob), "<MyImplicitStrObject>")

    def testExplicitString(self):
        ob = xpcom.server.WrapObject( ExplicitSupportsString(), components.interfaces.nsISupports)
        self.failUnlessEqual(str(ob), "<MyExplicitStrObject>")

    def testImplicitUnicode(self):
        ob = xpcom.server.WrapObject( ImplicitSupportsUnicode(), components.interfaces.nsISupports)
        self.failUnlessEqual(unicode(ob), ImplicitSupportsUnicode.test_data)

    def testExplicitUnicode(self):
        ob = xpcom.server.WrapObject( ExplicitSupportsUnicode(), components.interfaces.nsISupports)
        self.failUnlessEqual(unicode(ob), ExplicitSupportsUnicode.test_data)

    def testConvertInt(self):
        # Try our conversions.
        ob = xpcom.server.WrapObject( ExplicitSupportsString(), components.interfaces.nsISupports)
        self.failUnlessRaises( ValueError, int, ob)

    def testExplicitInt(self):
        ob = xpcom.server.WrapObject( ExplicitSupportsInt(), components.interfaces.nsISupports)
        self.failUnlessAlmostEqual(float(ob), 99.0)
        self.failUnlessEqual(int(ob), 99)

    def testImplicitInt(self):
        ob = xpcom.server.WrapObject( ImplicitSupportsInt(), components.interfaces.nsISupports)
        self.failUnlessAlmostEqual(float(ob), 99.0)
        self.failUnlessEqual(int(ob), 99)

    def testExplicitLong(self):
        ob = xpcom.server.WrapObject( ExplicitSupportsLong(), components.interfaces.nsISupports)
        if long(ob) != 99 or not repr(long(ob)).endswith("L"):
            raise RuntimeError, "Bad value: %s" % (repr(long(ob)),)
        self.failUnlessAlmostEqual(float(ob), 99.0)

    def testImplicitLong(self):
        ob = xpcom.server.WrapObject( ImplicitSupportsLong(), components.interfaces.nsISupports)
        if long(ob) != 99 or not repr(long(ob)).endswith("L"):
            raise RuntimeError, "Bad value: %s" % (repr(long(ob)),)
        self.failUnlessAlmostEqual(float(ob), 99.0)

    def testExplicitFloat(self):
        ob = xpcom.server.WrapObject( ExplicitSupportsFloat(), components.interfaces.nsISupports)
        self.failUnlessEqual(float(ob), 99.99)
        self.failUnlessEqual(int(ob), 99)

    def testImplicitFloat(self):
        ob = xpcom.server.WrapObject( ImplicitSupportsFloat(), components.interfaces.nsISupports)
        self.failUnlessEqual(float(ob), 99.99)
        self.failUnlessEqual(int(ob), 99)

class PrimitivesModuleTestCase(unittest.TestCase):
    def testExplicitString(self):
        ob = xpcom.server.WrapObject( ExplicitSupportsString(), components.interfaces.nsISupports)
        self.failUnlessEqual(primitives.GetPrimitive(ob), "<MyExplicitStrObject>")

    def testExplicitUnicode(self):
        ob = xpcom.server.WrapObject( ExplicitSupportsUnicode(), components.interfaces.nsISupports)
        self.failUnlessEqual(primitives.GetPrimitive(ob), ExplicitSupportsUnicode.test_data)
        self.failUnlessEqual(type(primitives.GetPrimitive(ob)), unicode)

    def testExplicitInt(self):
        ob = xpcom.server.WrapObject( ExplicitSupportsInt(), components.interfaces.nsISupports)
        self.failUnlessEqual(primitives.GetPrimitive(ob), 99)

    def testExplicitLong(self):
        ob = xpcom.server.WrapObject( ExplicitSupportsLong(), components.interfaces.nsISupports)
        self.failUnlessEqual(primitives.GetPrimitive(ob), 99)

    def testExplicitFloat(self):
        ob = xpcom.server.WrapObject( ExplicitSupportsFloat(), components.interfaces.nsISupports)
        self.failUnlessEqual(primitives.GetPrimitive(ob), 99.99)

if __name__=='__main__':
    testmain()
