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
#       Mark Hammond <MarkH@ActiveState.com> (original author)  
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

# NOTE: This is a TEST interface, not a DEMO interface :-)
# We try to get as many data-types etc exposed, meaning this
# doesnt really make a good demo of a "simple component"


from xpcom import components, verbose

class PythonTestComponent:
    # Note we only list the "child" interface, not our intermediate interfaces
    # (which we must, by definition, also support)
    _com_interfaces_ = components.interfaces.nsIPythonTestInterfaceDOMStrings
    _reg_clsid_ = "{7EE4BDC6-CB53-42c1-A9E4-616B8E012ABA}"
    _reg_contractid_ = "Python.TestComponent"
    def __init__(self):
        self.boolean_value = 1
        self.octet_value = 2
        self.short_value = 3
        self.ushort_value = 4
        self.long_value = 5
        self.ulong_value = 6
        self.long_long_value = 7
        self.ulong_long_value = 8
        self.float_value = 9.0
        self.double_value = 10.0
        self.char_value = "a"
        self.wchar_value = "b"
        self.string_value = "cee"
        self.wstring_value = "dee"
        self.astring_value = "astring"
        self.acstring_value = "acstring"
        self.utf8string_value = "utf8string"
        self.iid_value = self._reg_clsid_
        self.interface_value = None
        self.isupports_value = None
        self.domstring_value = "dom"

    def __del__(self):
        if verbose:
            print "Python.TestComponent: __del__ method called - object is destructing"

    def do_boolean(self, p1, p2):
        # boolean                  do_boolean(in boolean p1, inout boolean p2, out boolean p3);
        ret = p1 ^ p2
        return ret, not ret, ret

    def do_octet(self, p1, p2):
        # octet                    do_octet(in octet p1, inout octet p2, out octet p3);
        return p1+p2, p1-p2, p1*p2

    def do_short(self, p1, p2):
        # short                    do_short(in short p1, inout short p2, out short p3);
        return p1+p2, p1-p2, p1*p2

    def do_unsigned_short(self, p1, p2):
        # unsigned short           do_unsigned_short(in unsigned short p1, inout unsigned short p2, out unsigned short p3);
        return p1+p2, p1-p2, p1*p2
    def do_long(self, p1, p2):
        # long                     do_long(in long p1, inout long p2, out long p3);
        return p1+p2, p1-p2, p1*p2

    def do_unsigned_long(self, p1, p2):
        # unsigned long            do_unsigned_long(in unsigned long p1, inout unsigned long p2, out unsigned long p3);
        return p1+p2, p1-p2, p1*p2
    def do_long_long(self, p1, p2):
        #  long long                do_long_long(in long long p1, inout long long p2, out long long p3);
        return p1+p2, p1-p2, p1*p2
    def do_unsigned_long_long(self, p1, p2):
        # unsigned long long       do_unsigned_long_long(in unsigned long long p1, inout unsigned long long p2, out unsigned long long p3);
        return p1+p2, p1-p2, p1*p2
    def do_float(self, p1, p2):
        # float                    do_float(in float p1, inout float p2, out float p3);
        return p1+p2, p1-p2, p1*p2
    def do_double(self, p1, p2):
        # double                   do_double(in double p1, inout double p2, out double p3);
        return p1+p2, p1-p2, p1*p2
    def do_char(self, p1, p2):
        # char                     do_char(in char p1, inout char p2, out char p3);
        return chr(ord(p1)+ord(p2)), p2, p1
    def do_wchar(self, p1, p2):
        # wchar                    do_wchar(in wchar p1, inout wchar p2, out wchar p3);
        return chr(ord(p1)+ord(p2)), p2, p1
    def do_string(self, p1, p2):
        # string                   do_string(in string p1, inout string p2, out string p3);
        ret = ""
        if p1 is not None: ret = ret + p1
        if p2 is not None: ret = ret + p2
        return ret, p1, p2
    def do_wstring(self, p1, p2):
        # wstring                  do_wstring(in wstring p1, inout wstring p2, out wstring p3);
        ret = u""
        if p1 is not None: ret = ret + p1
        if p2 is not None: ret = ret + p2
        return ret, p1, p2
    def do_nsIIDRef(self, p1, p2):
        # nsIIDRef                 do_nsIIDRef(in nsIIDRef p1, inout nsIIDRef p2, out nsIIDRef p3);
        return p1, self._reg_clsid_, p2
    def do_nsIPythonTestInterface(self, p1, p2):
        # nsIPythonTestInterface   do_nsIPythonTestInterface(in nsIPythonTestInterface p1, inout nsIPythonTestInterface p2, out nsIPythonTestInterface p3);
        return p2, p1, self
    def do_nsISupports(self, p1, p2):
        # nsISupports              do_nsISupports(in nsISupports p1, inout nsISupports p2, out nsISupports p3);
        return self, p1, p2
    def do_nsISupportsIs(self, iid):
        # void                     do_nsISupportsIs(in nsIIDRef iid, [iid_is(iid),retval] out nsQIResult result)
        # Note the framework does the QI etc on us, so there is no real point me doing it.
        # (However, user code _should_ do the QI - otherwise any errors are deemed "internal" (as they
        # are raised by the C++ framework), and therefore logged to the console, etc.
        # A user QI allows the user to fail gracefully, whatever gracefully means for them!
        return self
# Do I really need these??
##    def do_nsISupportsIs2(self, iid, interface):
##        # void                     do_nsISupportsIs2(inout nsIIDRef iid, [iid_is(iid),retval] inout nsQIResult result);
##        return iid, interface
##    def do_nsISupportsIs3(self, interface):
##        # void                     do_nsISupportsIs3(out nsIIDRef iid, [iid_is(iid)] inout nsQIResult result);
##        return self._com_interfaces_, interface
##    def do_nsISupportsIs4(self):
##        # void                     do_nsISupportsIs4(out nsIIDRef iid, [iid_is(iid)] out nsQIResult result);
##        return self._com_interfaces_, self

    # Methods from the nsIPythonTestInterfaceExtra interface
    #
    def MultiplyEachItemInIntegerArray(self, val, valueArray):
        # void MultiplyEachItemInIntegerArray(
        #                       in PRInt32 val, 
        #                       in PRUint32 count, 
        #                       [array, size_is(count)] inout PRInt32 valueArray);
        # NOTE - the "sizeis" params are never passed to or returned from Python!
        results = []
        for item in valueArray:
            results.append(item * val)
        return results
    def MultiplyEachItemInIntegerArrayAndAppend(self, val, valueArray):
        #void MultiplyEachItemInIntegerArrayAndAppend(
        #                       in PRInt32 val, 
        #                       inout PRUint32 count, 
        #                       [array, size_is(count)] inout PRInt32 valueArray);
        results = valueArray[:]
        for item in valueArray:
            results.append(item * val)
        return results
    def DoubleStringArray(self, valueArray):
        # void DoubleStringArray(inout PRUint32 count, 
        #                       [array, size_is(count)] inout string valueArray);
        results = []
        for item in valueArray:
            results.append(item * 2)
        return results

    def ReverseStringArray(self, valueArray):
        # void ReverseStringArray(in PRUint32 count, 
        #                    [array, size_is(count)] inout string valueArray);
        valueArray.reverse()
        return valueArray

    # Note that this method shares a single "size_is" between 2 params!
    def CompareStringArrays(self, ar1, ar2):
        # void CompareStringArrays([array, size_is(count)] in string arr1,
        #              [array, size_is(count)] in string arr2,
        #              in unsigned long count,
        #             [retval] out short result);
        return cmp(ar1, ar2)

    def DoubleString(self, val):
        # void DoubleString(inout PRUint32 count, 
        #               [size_is(count)] inout string str);
        return val * 2
    def DoubleString2(self, val):
        # void DoubleString2(in PRUint32 in_count, [size_is(in_count)] in string in_str,
        #                out PRUint32 out_count, [size_is(out_count)] out string out_str);
        return val * 2
    def DoubleString3(self, val):
        # void DoubleString3(in PRUint32 in_count, [size_is(in_count)] in string in_str,
        #                    out PRUint32 out_count, [size_is(out_count), retval] string out_str);
        return val * 2
    def DoubleString4(self, val):
        # void DoubleString4([size_is(count)] in string in_str, inout PRUint32 count, [size_is(count)] out string out_str);
        return val * 2
    def UpString(self, val):
        # // UpString defines the count as only "in" - meaning the result must be the same size
        # void UpString(in PRUint32 count, 
        #               [size_is(count)] inout string str);
        return val.upper()
    UpString2 = UpString
        # // UpString2 defines count as only "in", and a string as only "out"
        #     void UpString2(in PRUint32 count, 
        #               [size_is(count)] inout string in_str,
        #               [size_is(count)]out string out_str);

    def GetFixedString(self, count):
        # void GetFixedString(in PRUint32 count, [size_is(count)out string out_str);
        return "A" * count

    # DoubleWideString functions are identical to DoubleString, except use wide chars!
    def DoubleWideString(self, val):
        return val * 2
    def DoubleWideString2(self, val):
        return val * 2
    def DoubleWideString3(self, val):
        return val * 2
    def DoubleWideString4(self, val):
        return val * 2
    def UpWideString(self, val):
        return val.upper()
    UpWideString2 = UpWideString
    def CopyUTF8String(self, v):
        return v
    def CopyUTF8String2(self, v):
        return v.encode("utf8")
    # Test we can get an "out" array with an "in" size (and the size is not used anywhere as a size for an in!)
    def GetFixedWideString(self, count):
        # void GetFixedWideString(in PRUint32 count, [size_is(count)out string out_str);
        return u"A" * count

    def GetStrings(self):
        # void GetStrings(out PRUint32 count,
        #            [retval, array, size_is(count)] out string str);
        return "Hello from the Python test component".split()
    # Some tests for our special "PRUint8" support.
    def UpOctetArray( self, data ):
        # void UpOctetArray(inout PRUint32 count,
        #                [array, size_is(count)] inout PRUint8 data);
        return data.upper()

    def UpOctetArray2( self, data ):
        # void UpOctetArray2(inout PRUint32 count,
        #                [array, size_is(count)] inout PRUint8 data);
        data = data.upper()
        # This time we return a list of integers.
        return map( ord, data )

    # Arrays of interfaces
    def CheckInterfaceArray(self, interfaces):
        # void CheckInterfaceArray(in PRUint32 count,
        #                          [array, size_is(count)] in nsISupports data,
        #                          [retval] out PRBool all_non_null);
        ret = 1
        for i in interfaces:
            if i is None:
                ret = 0
                break
        return ret
    def CopyInterfaceArray(self, a):
        return a
    def GetInterfaceArray(self):
        # void GetInterfaceArray(out PRUint32 count,
        #                          [array, size_is(count)] out nsISupports data);
        return self, self, self, None
    def ExtendInterfaceArray(self, data):
        # void ExtendInterfaceArray(inout PRUint32 count,
        #                          [array, size_is(count)] inout nsISupports data);
        return data * 2

    # Arrays of IIDs
    def CheckIIDArray(self, data):
        # void CheckIIDArray(in PRUint32 count,
        #                          [array, size_is(count)] in nsIIDRef data,
        #                          [retval] out PRBool all_mine);
        ret = 1
        for i in data:
            if i!= self._com_interfaces_ and i != self._reg_clsid_:
                ret = 0
                break
        return ret
    def GetIIDArray(self):
        # void GetIIDArray(out PRUint32 count,
        #                         [array, size_is(count)] out nsIIDRef data);
        return self._com_interfaces_, self._reg_clsid_
    def ExtendIIDArray(self, data):
        # void ExtendIIDArray(inout PRUint32 count,
        #                      [array, size_is(count)] inout nsIIDRef data);
        return data * 2

    # Test our count param can be shared as an "in" param.
    def SumArrays(self, array1, array2):
        # void SumArrays(in PRUint32 count, [array, size_is(count)]in array1, [array, size_is(count)]in array2, [retval]result);
        if len(array1)!=len(array2):
            print "SumArrays - not expecting different lengths!"
        result = 0
        for i in array1:
            result = result + i
        for i in array2:
            result = result+i
        return result

    # Test our count param can be shared as an "out" param.
    def GetArrays(self):
        # void GetArrays(out PRUint32 count, [array, size_is(count)]out array1, [array, size_is(count)]out array2);
        return (1,2,3), (4,5,6)
    # Test we can get an "out" array with an "in" size
    def GetFixedArray(self, size):
        # void GetFixedArray(in PRUint32 count, [array, size_is(count)]out PRInt32 array1]);
        return 0 * size
    
    # Test our "in" count param can be shared as one "in", plus one  "out" param.
    def CopyArray(self, array1):
        # void CopyArray(in PRUint32 count, [array, size_is(count)]in array1, [array, size_is(count)]out array2);
        return array1
    # Test our "in-out" count param can be shared as one "in", plus one  "out" param.
    def CopyAndDoubleArray(self, array):
        # void CopyAndDoubleArray(inout PRUint32 count, [array, size_is(count)]in array1, [array, size_is(count)]out array2);
        return array + array
    # Test our "in-out" count param can be shared as one "in", plus one  "in-out" param.
    def AppendArray(self, array1, array2):
        # void AppendArray(inout PRUint32 count, [array, size_is(count)]in array1, [array, size_is(count)]inout array2);
        rc = array1
        if array2 is not None:
            rc.extend(array2)
        return rc
    # Test nsIVariant support
    def AppendVariant(self, invar, inresult):
        if type(invar)==type([]):
            invar_use = invar[0]
            for v in invar[1:]:
                invar_use += v
        else:
            invar_use = invar
        if type(inresult)==type([]):
            inresult_use = inresult[0]
            for v in inresult[1:]:
                inresult_use += v
        else:
            inresult_use = inresult
        if inresult_use is None and invar_use is None:
            return None
        return inresult_use + invar_use

    def CopyVariant(self, invar):
        return invar

    # Some tests for the "new" (Feb-2001) DOMString type.
    def GetDOMStringResult( self, length ):
        # Result: DOMString &
        if length == -1:
            return None
        return "P" * length
    def GetDOMStringOut( self, length ):
        # Result: DOMString &
        if length == -1:
            return None
        return "y" * length
    def GetDOMStringLength( self, param0 ):
        # Result: uint32
        # In: param0: DOMString &
        if param0 is None: return -1
        return len(param0)

    def GetDOMStringRefLength( self, param0 ):
        # Result: uint32
        # In: param0: DOMString &
        if param0 is None: return -1
        return len(param0)

    def GetDOMStringPtrLength( self, param0 ):
        # Result: uint32
        # In: param0: DOMString *
        if param0 is None: return -1
        return len(param0)

    def ConcatDOMStrings( self, param0, param1 ):
        # Result: void - None
        # In: param0: DOMString &
        # In: param1: DOMString &
        # Out: DOMString &
        return param0 + param1
    def get_domstring_value( self ):
        # Result: DOMString &
        return self.domstring_value
    def set_domstring_value( self, param0 ):
        # Result: void - None
        # In: param0: DOMString &
        self.domstring_value = param0

    def get_domstring_value_ro( self ):
        # Result: DOMString &
        return self.domstring_value
