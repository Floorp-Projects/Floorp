# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with the
# License. You may obtain a copy of the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
# the specific language governing rights and limitations under the License.
#
# The Original Code is the Python XPCOM language bindings.
#
# The Initial Developer of the Original Code is ActiveState Tool Corp.
# Portions created by ActiveState Tool Corp. are Copyright (C) 2000, 2001
# ActiveState Tool Corp.  All Rights Reserved.
#
# Contributor(s): Mark Hammond <MarkH@ActiveState.com> (original author)
#

import xpcom
import xpcom.client
import xpcom.server
import xpcom._xpcom
import xpcom.components
import string

import unittest

import traceback, getopt, sys

verbose_level = 0

reportedSampleMissing = 0

def get_sample_component_cpp():
    global reportedSampleMissing
    contractid = "@mozilla.org/sample;1" # The C++ version.
    try:
        return xpcom.components.classes[contractid].createInstance()
    except xpcom.COMException:
        if not reportedSampleMissing:
            print "***"
            print "*** This test requires an XPCOM sample component,"
            print "*** which does not exist.  To build this test, you"
            print "*** should change to the 'mozilla/xpcom/sample' directory,"
            print "*** and run 'make', then run this test again."
            print "***"
            reportedSampleMissing = 1
        else:
            print "(skipping - no C++ sample...) ",
        return None

def get_sample_component_js():
    # This should *always* exist - no special make process.
    contractid = "@mozilla.org/jssample;1" # the JS version
    return xpcom.components.classes[contractid].createInstance()
    
class TestDumpInterfaces(unittest.TestCase):
    def testAllInterfaces(self):
        "Dump every interface under the sun!"
        import xpcom, xpcom.xpt, xpcom._xpcom
        iim = xpcom._xpcom.XPTI_GetInterfaceInfoManager()
    
        if verbose_level:
            print "Dumping every interface I can find"
        enum = iim.EnumerateInterfaces()
        rc = enum.First()
        num = 0
        while rc==0:
            item = enum.CurrentItem(xpcom._xpcom.IID_nsIInterfaceInfo)
            try:
                iid = item.GetIID()
            except xpcom.COMException:
                if verbose_level:
                    print "Can't dump", item
                continue # Dont bother dumping this.
            interface = xpcom.xpt.Interface(iid)
            num = num + 1
            text = interface.Describe()
            if verbose_level:
                print text
    
            rc = enum.Next()
        if num < 200:
            print "Only found", num, "interfaces - this seems unusually low!"

class TestEnumContractIDs(unittest.TestCase):
    def testContractIDs(self):
        """Enumerate all the ContractIDs registered"""
        cm = xpcom._xpcom.NS_GetGlobalComponentManager()
        enum = cm.EnumerateContractIDs()
        rc = enum.First()
        n = 0
        while rc == 0:
            n = n + 1
            if verbose_level:
                print "ContractID:", enum.CurrentItem()
            rc = enum.Next()
        if n < 200:
            print "Only found", n, "ContractIDs - this seems unusually low!"

class TestSampleComponent(unittest.TestCase):
    def _doTestSampleComponent(self, test_flat = 0):
        """Test the standard Netscape 'sample' sample"""
        c = get_sample_component_cpp()
        if c is None:
            return
        if not test_flat:
            c = c.queryInterface(xpcom.components.interfaces.nsISample)
        self.failUnlessEqual(c.value, "initial value")
        c.value = "new value"
        self.failUnlessEqual(c.value, "new value")
        c.poke("poked value")
        self.failUnlessEqual(c.value, "poked value")
        c.writeValue("Python just poked:")

    def testSampleComponentFlat(self):
        """Test the standard Netscape 'sample' sample using interface flattening"""
        self._doTestSampleComponent(1)

    def testSampleComponentOld(self):
        """Test the standard Netscape 'sample' sample using explicit QI"""
        self._doTestSampleComponent(0)
    
    def _doTestHash(self, c):
        "Test that hashing COM objects works"
        d = {}
        d[c] = None
        if not d.has_key(c):
            raise RuntimeError, "Can't get the exact same object back!"
        if not d.has_key(c.queryInterface(xpcom.components.interfaces.nsISupports)):
            raise RuntimeError, "Can't get back as nsISupports"

        # And the same in reverse - stick an nsISupports in, and make sure an explicit interface comes back.
        d = {}
#        contractid = "@mozilla.org/sample;1" # The C++ version.
#        c = xpcom.components.classes[contractid].createInstance() \
#            .queryInterface(xpcom.components.interfaces.nsISupports)
        d[c] = None
        if not d.has_key(c):
            raise RuntimeError, "Can't get the exact same object back!"
        if not d.has_key(c.queryInterface(xpcom.components.interfaces.nsISample)):
            raise RuntimeError, "Can't get back as nsISupports"

    def testHashJS(self):
        c = get_sample_component_js()
        self._doTestHash(c)

    def testHashCPP(self):
        c = get_sample_component_cpp()
        if c is not None:
            self._doTestHash(c)


class TestIIDs(unittest.TestCase):
    def TestIIDs(self):
        "Do some basic IID semantic tests."
        iid_str = "{7ee4bdc6-cb53-42c1-a9e4-616b8e012aba}"
        IID = xpcom._xpcom.IID
        self.failUnlessEqual(IID(iid_str), IID(iid_str))
        self.failUnlessEqual(hash(IID(iid_str)), hash(IID(iid_str)))
        self.failUnlessEqual(IID(iid_str), IID(iid_str.upper()))
        self.failUnlessEqual(hash(IID(iid_str)), hash(IID(iid_str.upper())))
        # If the above work, this shoud too, but WTF
        dict = {}
        dict[IID(iid_str)] = None
        self.failUnless(dict.has_key(IID(iid_str)), "hashes failed in dictionary")
        self.failUnless(dict.has_key(IID(iid_str.upper())), "uppercase hash failed in dictionary")

class TestRepr(unittest.TestCase):
    def _doTestRepr(self, progid, interfaces):
        if isinstance(progid, str):
            ob = xpcom.components.classes[progid].createInstance()
        else:
            ob = progid
        self.failUnless(repr(ob).find(progid) >= 0, repr(ob))
        for interface_name in interfaces.split():
            self.failUnless(repr(ob).find(interface_name) >= 0, repr(ob))

    def testReprPython(self):
        "Test repr() of Python objects"
        self._doTestRepr("Python.TestComponent", "nsIPythonTestInterfaceDOMStrings nsIPythonTestInterfaceExtra nsIPythonTestInterface")

    # JS does not provide class-info :(
    #def testReprJS(self):
    #    self._doTestRepr("@mozilla.org/jssample;1", "nsISample")

    def testReprSample(self):
        "Test repr() of non-Python objects"
        ob = get_sample_component_cpp()
        if ob is None:
            return
        self._doTestRepr(ob, "nsISample")

class TestUnwrap(unittest.TestCase):
    "Test the unwrap facilities"
    def testUnwrap(self):
        # First test that a Python object can be unwrapped.
        ob = xpcom.components.classes["Python.TestComponent"].createInstance()
        pyob = xpcom.server.UnwrapObject(ob)
        # This depends on our __repr__ implementation, but that's OK - it
        # can be updated should our __repr__ change :)
        self.failUnless(str(pyob).startswith("<component:py_test_component.PythonTestComponent"))
        # Test that a non-Python implemented object can NOT be unwrapped.
        ob = get_sample_component_cpp()
        if ob is None:
            return
        self.failUnlessRaises(ValueError, xpcom.server.UnwrapObject, ob)

if __name__=='__main__':
    unittest.main()
    xpcom._xpcom.NS_ShutdownXPCOM()
    ni = xpcom._xpcom._GetInterfaceCount()
    ng = xpcom._xpcom._GetGatewayCount()
    if ni or ng:
        print "********* WARNING - Leaving with %d/%d objects alive" % (ni,ng)
