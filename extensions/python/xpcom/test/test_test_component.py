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

import sys, os
import xpcom.components
import xpcom._xpcom
import xpcom.nsError

num_errors = 0

component_iid = xpcom.components.ID("{7EE4BDC6-CB53-42c1-A9E4-616B8E012ABA}")
new_iid = xpcom.components.ID("{2AF747D3-ECBC-457b-9AF9-5C5D80EDC360}")

contractid = "Python.TestComponent"

really_big_string = "This is really repetitive!" * 10000
really_big_wstring = u"This is really repetitive!" * 10000

def print_error(error):
    print error
    global num_errors
    num_errors = num_errors + 1

def _test_value(what, got, expecting):
    ok = got == expecting
    if type(got)==type(expecting)==type(0.0):
        ok = abs(got-expecting) < 0.001
    if not ok:
        print_error("*** Error %s - got '%r', but expecting '%r'" % (what, got, expecting))

def test_attribute(ob, attr_name, expected_init, new_value, new_value_really = None):
    if xpcom.verbose:
        print "Testing attribute %s" % (attr_name,)
    if new_value_really is None:
        new_value_really = new_value # Handy for eg bools - set a BOOL to 2, you still get back 1!
        
    _test_value( "getting initial attribute value (%s)" % (attr_name,), getattr(ob, attr_name), expected_init)
    setattr(ob, attr_name, new_value)
    _test_value( "getting new attribute value (%s)" % (attr_name,), getattr(ob, attr_name), new_value_really)
    # And set it back to the expected init.
    setattr(ob, attr_name, expected_init)
    _test_value( "getting back initial attribute value after change (%s)" % (attr_name,), getattr(ob, attr_name), expected_init)

def test_attribute_failure(ob, attr_name, new_value, expected_exception):
    try:
        setattr(ob, attr_name, new_value)
        print_error("*** Setting attribute '%s' to '%r' didnt yield an exception!" % (attr_name, new_value) )
    except:
        exc_typ = sys.exc_info()[0]
        ok = issubclass(exc_typ, expected_exception)
        if not ok:
            print_error("*** Wrong exception setting '%s' to '%r'- got '%s: %s', expected '%s'" % (attr_name, new_value, exc_typ, exc_val, expected_exception))


def test_method(method, args, expected_results):
    if xpcom.verbose:
        print "Testing %s%s" % (method.__name__, `args`)
    ret = method(*args)
    if ret != expected_results:
        print_error("calling method %s - expected %s, but got %s" % (method.__name__, expected_results, ret))

def test_int_method(meth):
    test_method(meth, (0,0), (0,0,0))
    test_method(meth, (1,1), (2,0,1))
    test_method(meth, (5,2), (7,3,10))
#    test_method(meth, (2,5), (7,-3,10))

def test_constant(ob, cname, val):
    v = getattr(ob, cname)
    if v != val:
        print_error("Bad value for constant '%s' - got '%r'" % (cname, v))
    try:
        setattr(ob, cname, 0)
        print_error("The object allowed us to set the constant '%s'" % (cname,))
    except AttributeError:
        pass

def test_base_interface(c):
    test_attribute(c, "boolean_value", 1, 0)
    test_attribute(c, "boolean_value", 1, -1, 1) # Set a bool to anything, you should always get back 0 or 1
    test_attribute(c, "boolean_value", 1, 4, 1) # Set a bool to anything, you should always get back 0 or 1
    test_attribute(c, "boolean_value", 1, "1", 1) # This works by virtual of PyNumber_Int - not sure I agree, but...
    test_attribute_failure(c, "boolean_value", "boo", ValueError)
    test_attribute_failure(c, "boolean_value", test_base_interface, TypeError)

    test_attribute(c, "octet_value", 2, 5)
    test_attribute(c, "octet_value", 2, 0)
    test_attribute(c, "octet_value", 2, 128) # octet is unsigned 8 bit
    test_attribute(c, "octet_value", 2, 255) # octet is unsigned 8 bit
    test_attribute(c, "octet_value", 2, -1, 255) # octet is unsigned 8 bit
    test_attribute_failure(c, "octet_value", "boo", ValueError)

    test_attribute(c, "short_value", 3, 10)
    test_attribute(c, "short_value", 3, -1) # 16 bit signed
    test_attribute(c, "short_value", 3, 0xFFFF, -1) # 16 bit signed
    test_attribute(c, "short_value", 3, 0L)
    test_attribute(c, "short_value", 3, 1L)
    test_attribute(c, "short_value", 3, -1L)
    test_attribute(c, "short_value", 3, 0xFFFFL, -1)
    test_attribute_failure(c, "short_value", "boo", ValueError)

    test_attribute(c, "ushort_value",  4, 5)
    test_attribute(c, "ushort_value",  4, 0)
    test_attribute(c, "ushort_value",  4, -1, 0xFFFF) # 16 bit signed
    test_attribute(c, "ushort_value",  4, 0xFFFF) # 16 bit signed
    test_attribute(c, "ushort_value",  4, 0L)
    test_attribute(c, "ushort_value",  4, 1L)
    test_attribute(c, "ushort_value",  4, -1L, 0xFFFF)
    test_attribute_failure(c, "ushort_value", "boo", ValueError)

    test_attribute(c, "long_value",  5, 7)
    test_attribute(c, "long_value",  5, 0)
    test_attribute(c, "long_value",  5, 0xFFFFFFFF, -1) # 32 bit signed.
    test_attribute(c, "long_value",  5, -1) # 32 bit signed.
    test_attribute(c, "long_value",  5, 0L)
    test_attribute(c, "long_value",  5, 1L)
    test_attribute(c, "long_value",  5, -1L)
    test_attribute_failure(c, "long_value", 0xFFFFL * 0xFFFF, OverflowError) # long int too long to convert
    test_attribute_failure(c, "long_value", "boo", ValueError)

    test_attribute(c, "ulong_value", 6, 7)
    test_attribute(c, "ulong_value", 6, 0)
    test_attribute(c, "ulong_value", 6, 0xFFFFFFFF) # 32 bit signed.
    test_attribute_failure(c, "ulong_value", "boo", ValueError)
    
    test_attribute(c, "long_long_value", 7, 8)
    test_attribute(c, "long_long_value", 7, 0)
    test_attribute(c, "long_long_value", 7, -1)
    test_attribute(c, "long_long_value", 7, 0xFFFF)
    test_attribute(c, "long_long_value", 7, 0xFFFFL * 2)
    test_attribute_failure(c, "long_long_value", 0xFFFFL * 0xFFFF * 0xFFFF * 0xFFFF, OverflowError) # long int too long to convert
    test_attribute_failure(c, "long_long_value", "boo", ValueError)
    
    test_attribute(c, "ulong_long_value", 8, 9)
    test_attribute(c, "ulong_long_value", 8, 0)
    test_attribute_failure(c, "ulong_long_value", "boo", ValueError)
    test_attribute_failure(c, "ulong_long_value", -1, OverflowError) # can't convert negative value to unsigned long)
    
    test_attribute(c, "float_value", 9.0, 10.2)
    test_attribute(c, "float_value", 9.0, 0)
    test_attribute(c, "float_value", 9.0, -1)
    test_attribute(c, "float_value", 9.0, 1L)
    test_attribute_failure(c, "float_value", "boo", ValueError)

    test_attribute(c, "double_value", 10.0, 9.0)
    test_attribute(c, "double_value", 10.0, 0)
    test_attribute(c, "double_value", 10.0, -1)
    test_attribute(c, "double_value", 10.0, 1L)
    test_attribute_failure(c, "double_value", "boo", ValueError)
    
    test_attribute(c, "char_value", "a", "b")
    test_attribute(c, "char_value", "a", "\0")
    test_attribute_failure(c, "char_value", "xy", ValueError)
    test_attribute(c, "char_value", "a", u"c")
    test_attribute(c, "char_value", "a", u"\0")
    test_attribute_failure(c, "char_value", u"xy", ValueError)
    
    test_attribute(c, "wchar_value", "b", "a")
    test_attribute(c, "wchar_value", "b", "\0")
    test_attribute_failure(c, "wchar_value", "hi", ValueError)
    test_attribute(c, "wchar_value", "b", u"a")
    test_attribute(c, "wchar_value", "b", u"\0")
    test_attribute_failure(c, "wchar_value", u"hi", ValueError)
    
    test_attribute(c, "string_value", "cee", "dee")
    test_attribute(c, "string_value", "cee", "a null >\0<", "a null >") # strings are NULL terminated!!
    test_attribute(c, "string_value", "cee", "")
    test_attribute(c, "string_value", "cee", u"dee")
    test_attribute(c, "string_value", "cee", u"a null >\0<", "a null >") # strings are NULL terminated!!
    test_attribute(c, "string_value", "cee", u"")

    test_attribute(c, "wstring_value", "dee", "cee")
    test_attribute(c, "wstring_value", "dee", "a null >\0<", "a null >") # strings are NULL terminated!!
    test_attribute(c, "wstring_value", "dee", "")
    test_attribute(c, "wstring_value", "dee", really_big_string)
    test_attribute(c, "wstring_value", "dee", u"cee")
    test_attribute(c, "wstring_value", "dee", u"a null >\0<", "a null >") # strings are NULL terminated!!
    test_attribute(c, "wstring_value", "dee", u"")
    test_attribute(c, "wstring_value", "dee", really_big_wstring)
    # This will fail internal string representation :(  Test we don't crash
    try:
        c.wstring_value = "a big char >" + chr(129) + "<"
        print_error("strings with chars > 128 appear to have stopped failing?")
    except UnicodeError:
        pass

    test_attribute(c, "iid_value", component_iid, new_iid)
    test_attribute(c, "iid_value", component_iid, str(new_iid), new_iid)
    test_attribute(c, "iid_value", component_iid, xpcom._xpcom.IID(new_iid))

    test_attribute_failure(c, "no_attribute", "boo", AttributeError)

    test_attribute(c, "interface_value", None, c)
    test_attribute_failure(c, "interface_value", 2, TypeError)

    test_attribute(c, "isupports_value", None, c)

    # The methods
    test_method(c.do_boolean, (0,1), (1,0,1))
    test_method(c.do_boolean, (1,0), (1,0,1))
    test_method(c.do_boolean, (1,1), (0,1,0))

    test_int_method(c.do_octet)
    test_int_method(c.do_short)

    test_int_method(c.do_unsigned_short)
    test_int_method(c.do_long)
    test_int_method(c.do_unsigned_long)
    test_int_method(c.do_long_long)
    test_int_method(c.do_unsigned_long)
    test_int_method(c.do_float)
    test_int_method(c.do_double)

    test_method(c.do_char, ("A", " "), (chr(ord("A")+ord(" ")), " ","A") )
    test_method(c.do_char, ("A", "\0"), ("A", "\0","A") )
    test_method(c.do_wchar, ("A", " "), (chr(ord("A")+ord(" ")), " ","A") )
    test_method(c.do_wchar, ("A", "\0"), ("A", "\0","A") )

    test_method(c.do_string, ("Hello from ", "Python"), ("Hello from Python", "Hello from ", "Python") )
    test_method(c.do_string, (u"Hello from ", u"Python"), ("Hello from Python", "Hello from ", "Python") )
    test_method(c.do_string, (None, u"Python"), ("Python", None, "Python") )
    test_method(c.do_string, (None, really_big_string), (really_big_string, None, really_big_string) )
    test_method(c.do_string, (None, really_big_wstring), (really_big_string, None, really_big_string) )
    test_method(c.do_wstring, ("Hello from ", "Python"), ("Hello from Python", "Hello from ", "Python") )
    test_method(c.do_wstring, (u"Hello from ", u"Python"), ("Hello from Python", "Hello from ", "Python") )
    test_method(c.do_string, (None, really_big_wstring), (really_big_wstring, None, really_big_wstring) )
    test_method(c.do_string, (None, really_big_string), (really_big_wstring, None, really_big_wstring) )
    test_method(c.do_nsIIDRef, (component_iid, new_iid), (component_iid, component_iid, new_iid))
    test_method(c.do_nsIIDRef, (new_iid, component_iid), (new_iid, component_iid, component_iid))
    test_method(c.do_nsIPythonTestInterface, (None, None), (None, None, c))
    test_method(c.do_nsIPythonTestInterface, (c, c), (c, c, c))
    test_method(c.do_nsISupports, (None, None), (c, None, None))
    test_method(c.do_nsISupports, (c,c), (c, c, c))
    test_method(c.do_nsISupportsIs, (xpcom._xpcom.IID_nsISupports,), c)
    test_method(c.do_nsISupportsIs, (xpcom.components.interfaces.nsIPythonTestInterface,), c)
##    test_method(c.do_nsISupportsIs2, (xpcom.components.interfaces.nsIPythonTestInterface,c), (xpcom.components.interfaces.nsIPythonTestInterface,c))
##    test_method(c.do_nsISupportsIs3, (c,), (xpcom.components.interfaces.nsIPythonTestInterface,c))
##    test_method(c.do_nsISupportsIs4, (), (xpcom.components.interfaces.nsIPythonTestInterface,c))
    # Test the constants.
    test_constant(c, "One", 1)
    test_constant(c, "Two", 2)
    test_constant(c, "MinusOne", -1)
    test_constant(c, "BigLong", 0x7FFFFFFF)
    test_constant(c, "BigULong", 0xFFFFFFFF)
    # Test the components.Interfaces semantics
    i = xpcom.components.interfaces.nsIPythonTestInterface
    test_constant(i, "One", 1)
    test_constant(i, "Two", 2)
    test_constant(i, "MinusOne", -1)
    test_constant(i, "BigLong", 0x7FFFFFFF)
    test_constant(i, "BigULong", 0xFFFFFFFF)

def test_derived_interface(c):
    val = "Hello\0there"
    expected = val * 2

    test_method(c.DoubleString, (val,), expected)
    test_method(c.DoubleString2, (val,), expected)
    test_method(c.DoubleString3, (val,), expected)
    test_method(c.DoubleString4, (val,), expected)
    test_method(c.UpString, (val,), val.upper())
    test_method(c.UpString2, (val,), val.upper())
    test_method(c.GetFixedString, (20,), "A"*20)
    val = u"Hello\0there"
    expected = val * 2
    test_method(c.DoubleWideString, (val,), expected)
    test_method(c.DoubleWideString2, (val,), expected)
    test_method(c.DoubleWideString3, (val,), expected)
    test_method(c.DoubleWideString4, (val,), expected)
    test_method(c.UpWideString, (val,), val.upper())
    test_method(c.UpWideString2, (val,), val.upper())
    test_method(c.GetFixedWideString, (20,), u"A"*20)
    items = [1,2,3,4,5]
    test_method(c.MultiplyEachItemInIntegerArray, (3, items,), map(lambda i:i*3, items))

    test_method(c.MultiplyEachItemInIntegerArrayAndAppend, (3, items), items + map(lambda i:i*3, items))
    items = "Hello from Python".split()
    expected = map( lambda x: x*2, items)
    test_method(c.DoubleStringArray, (items,), expected)

    test_method(c.CompareStringArrays, (items, items), cmp(items, items))
    # Can we pass lists and tuples correctly?
    test_method(c.CompareStringArrays, (items, tuple(items)), cmp(items, items))
    items2 = ["Not", "the", "same"]
    test_method(c.CompareStringArrays, (items, items2), cmp(items, items2))

    expected = items[:]
    expected.reverse()
    test_method(c.ReverseStringArray, (items,), expected)

    expected = "Hello from the Python test component".split()
    test_method(c.GetStrings, (), expected)

    val = "Hello\0there"
    test_method(c.UpOctetArray, (val,), val.upper())
    test_method(c.UpOctetArray2, (val,), val.upper())

    test_method(c.CheckInterfaceArray, ((c, c),), 1)
    test_method(c.CheckInterfaceArray, ((c, None),), 0)
    test_method(c.CheckInterfaceArray, ((),), 1)

    test_method(c.GetInterfaceArray, (), [c,c,c, None])
    test_method(c.ExtendInterfaceArray, ((c,c,c, None),), [c,c,c,None,c,c,c,None] )

    expected = [xpcom.components.interfaces.nsIPythonTestInterfaceDOMStrings, xpcom.components.classes[contractid].clsid]
    test_method(c.GetIIDArray, (), expected)

    val = [xpcom.components.interfaces.nsIPythonTestInterfaceExtra, xpcom.components.classes[contractid].clsid]
    expected = val * 2
    test_method(c.ExtendIIDArray, (val,), expected)

    test_method(c.GetArrays, (), ( [1,2,3], [4,5,6] ) )
    test_method(c.CopyArray, ([1,2,3],), [1,2,3] )
    test_method(c.CopyAndDoubleArray, ([1,2,3],), [1,2,3,1,2,3] )
    test_method(c.AppendArray, ([1,2,3],), [1,2,3])
    test_method(c.AppendArray, ([1,2,3],[4,5,6]), [1,2,3,4,5,6])

    c = c.queryInterface(xpcom.components.interfaces.nsIPythonTestInterfaceDOMStrings)
# NULL DOM strings don't work yet.
#    test_method(c.GetDOMStringResult, (-1,), None)
    test_method(c.GetDOMStringResult, (3,), "PPP")
#    test_method(c.GetDOMStringOut, (-1,), None)
    test_method(c.GetDOMStringOut, (4,), "yyyy")
    val = "Hello there"
    test_method(c.GetDOMStringLength, (val,), len(val))
    test_method(c.GetDOMStringRefLength, (val,), len(val))
    test_method(c.GetDOMStringPtrLength, (val,), len(val))
    test_method(c.ConcatDOMStrings, (val,val), val+val)
    test_attribute(c, "domstring_value", "dom", "new dom")
    if c.domstring_value_ro != "dom":
        print "Read-only DOMString not currect - got", c.domstring_ro
    try:
        c.dom_string_ro = "new dom"
        print "Managed to set a readonly attribute - eek!"
    except AttributeError:
        pass
    except:
        print "Unexpected exception when setting readonly attribute: %s: %s" % (sys.exc_info()[0], sys.exc_info()[1])
    if c.domstring_value_ro != "dom":
        print "Read-only DOMString not correct after failed set attempt - got", c.domstring_ro

def do_test_failures():
    c = xpcom.client.Component(contractid, xpcom.components.interfaces.nsIPythonTestInterfaceExtra)
    try:
        ret = c.do_nsISupportsIs( xpcom._xpcom.IID_nsIInterfaceInfoManager )
        print "*** got", ret, "***"
        raise RuntimeError, "We worked when using an IID we dont support!?!"
    except xpcom.Exception, details:
        if details.errno != xpcom.nsError.NS_ERROR_NO_INTERFACE:
            raise RuntimeError, "Wrong COM exception type: %r" % (details,)

def test_failures():
    # This extra stack-frame ensures Python cleans up sys.last_traceback etc
    do_test_failures()

def test_all():
    c = xpcom.client.Component(contractid, xpcom.components.interfaces.nsIPythonTestInterface)
    test_base_interface(c)
    # Now create an instance using the derived IID, and test that.
    c = xpcom.client.Component(contractid, xpcom.components.interfaces.nsIPythonTestInterfaceExtra)
    test_base_interface(c)
    test_derived_interface(c)
    test_failures()

try:
    from sys import gettotalrefcount
except ImportError:
    # Not a Debug build - assume no references (can't be leaks then :-)
    def gettotalrefcount():
        return 0

def test_from_js():
    # Ensure we can find the js test script - same dir as this!
    # Assume the path of sys.argv[0] is where we can find the js test code.
    # (Running under the regression test is a little painful)
    script_dir = os.path.split(sys.argv[0])[0]
    fname = os.path.join( script_dir, "test_test_component.js")
    if not os.path.isfile(fname):
        raise RuntimeError, "Can not find '%s'" % (fname,)
    # Note we _dont_ pump the test output out, as debug "xpcshell" spews
    # extra debug info that will cause our output comparison to fail.
    try:
        data = os.popen('xpcshell "' + fname + '"').readlines()
        good = 0
        for line in data:
            if line.strip() == "javascript successfully tested the Python test component.":
                good = 1
        if good:
            print "Javascript could successfully use the Python test component."
        else:
            print "** The javascript test appeared to fail!  Test output follows **"
            print "".join(data)
            print "** End of javascript test output **"

    except os.error, why:
        print "Error executing the javascript test program:", why
        

def doit(num_loops = -1):
    if "-v" in sys.argv: # Hack the verbose flag for the server
        xpcom.verbose = 1
    # Do the test lots of times - can help shake-out ref-count bugs.
    print "Testing the Python.TestComponent component"
    if num_loops == -1: num_loops = 10
    for i in xrange(num_loops):
        test_all()

        if i==0:
            # First loop is likely to "leak" as we cache things.
            # Leaking after that is a problem.
            num_refs = gettotalrefcount()

        if num_errors:
            break

    lost = gettotalrefcount() - num_refs
    # Sometimes we get spurious counts off by 1 or 2.
    # This can't indicate a real leak, as we have looped
    # more than twice!
    if abs(lost)>2:
        print "*** Lost %d references" % (lost,)

    if num_errors:
        print "There were", num_errors, "errors testing the Python component :-("
    else:
        print "The Python test component worked!"

# regrtest doesnt like if __name__=='__main__' blocks - it fails when running as a test!
num_iters = -1
if __name__=='__main__' and len(sys.argv) > 1:
    num_iters = int(sys.argv[1])

doit(num_iters)
test_from_js()

if __name__=='__main__':
    # But we can only do this if _not_ testing - otherwise we
    # screw up any tests that want to run later.
    xpcom._xpcom.NS_ShutdownXPCOM()
    ni = xpcom._xpcom._GetInterfaceCount()
    ng = xpcom._xpcom._GetGatewayCount()
    if ni or ng:
        print "********* WARNING - Leaving with %d/%d objects alive" % (ni,ng)
