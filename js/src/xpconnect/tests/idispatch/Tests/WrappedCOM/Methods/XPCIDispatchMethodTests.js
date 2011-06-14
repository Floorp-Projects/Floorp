/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the IDispatch implementation for XPConnect.
 *
 * The Initial Developer of the Original Code is
 * David Bradley.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * Verify that we can access but not overwrite the values of read-only 
 * attributes.
 */

test();

function TestOutMethod(obj, method, value)
{
    printDebug("TestOutMethod - " + method);
    var x = new Object();
    try
    {
        obj[method](x);
        reportCompare(value, x.value, "Testing output parameter: " + method);
    }
    catch (e)
    {
        reportFailure("Testing output parameter failed: " + method + ": " + e.toString());
    }
}

function TestReturnMethod(obj, method, value)
{
    printDebug("TestReturnMethod - " + method);
    try
    {
        reportCompare(value, obj[method](), "Testing output parameter: " + method);
    }
    catch (e)
    {
        reportFailure("Testing return value failed: " + method + ": " + e.toString());
    }
}

function TestInputMethod(obj, method, value)
{
    printDebug("TestInputMethod - " + method);
    try
    {
        obj[method.toLowerCase()](value);
    }
    catch (e)
    {
        reportFailure("Testing input parameter failed: " + method + ": " + e.toString());
    }
}

function TestInOutMethod(obj, method, inValue, outValue)
{
    printDebug("TestInOutMethod - " + method);
    try
    {
        var param = { value : inValue };
        obj[method](param);
        reportCompare(outValue, param.value, "Testing in/out parameter: " + method);
    }
    catch (e)
    {
        reportFailure("Testing in/out parameter failed: " + method + ": " + e.toString());
    }
}

function test()
{
    printStatus("Testing methods"); 
    var obj = COMObject(CLSID_nsXPCDispTestMethods);    
    try
    {
        obj.NoParameters();
    }
    catch (e)
    {
        reportFailure("NoParameters failed: " + e.toString());
    }
    printStatus("Test method - Return values"); 
    TestReturnMethod(obj, "ReturnBSTR", "Boo");
    TestReturnMethod(obj, "ReturnI4",99999);
    TestReturnMethod(obj, "ReturnUI1", 99);
    TestReturnMethod(obj, "ReturnI2",9999);
    TestReturnMethod(obj, "ReturnR4",99999.1);
    TestReturnMethod(obj, "ReturnR8", 99999999999.99);
    TestReturnMethod(obj, "ReturnBool", true);

    printStatus("TestReturnMethod - ReturnIDispatch");
    compareObject(obj.ReturnIDispatch(), nsXPCDispSimpleDesc, "Test method - Return IDispatch"); 
    var E_FAIL= -2147467259;
    TestReturnMethod(obj, "ReturnError", E_FAIL);
    TestReturnMethod(obj, "ReturnDate", "5/2/2002");
    // TestReturnMethod(obj, ReturnIUnknown(IUnknown * * result)
    TestReturnMethod(obj, "ReturnI1", 120);
    TestReturnMethod(obj, "ReturnUI2", 9999);
    TestReturnMethod(obj, "ReturnUI4", 3000000000);
    TestReturnMethod(obj, "ReturnInt", -999999);
    TestReturnMethod(obj, "ReturnUInt", 3000000000);

    printStatus("Test method - Input params");
    TestInputMethod(obj, "TakesBSTR", "TakesBSTR");
    TestInputMethod(obj, "TakesI4", 999999);
    TestInputMethod(obj, "TakesUI1", 42);
    TestInputMethod(obj, "TakesI2", 32000);
    TestInputMethod(obj, "TakesR4", 99999.99);
    TestInputMethod(obj, "TakesR8", 999999999.99);
    TestInputMethod(obj, "TakesBool", true);
    var x = { Number : 5, ClassName : "nsXPCDispSimple" };
    TestInputMethod(obj, "TakesIDispatch", x);
    // This would always fail, type mismatch (no way to indicate this is going
    // to an error, and there's no automatic conversion from integers to error
    // codes
    // TestInputMethod(obj, "TakesError", E_FAIL);
    TestInputMethod(obj, "TakesDate", "5/2/2002");
    // TestInputMethod(obj, TakesIUnknown, );
    TestInputMethod(obj, "TakesI1", 42);
    TestInputMethod(obj, "TakesUI2", 50000);
    TestInputMethod(obj, "TakesUI4", 4026531840);
    TestInputMethod(obj, "TakesInt", -10000000);
    TestInputMethod(obj, "TakesUInt", 3758096384);

    printStatus("Test method - Output params");
    TestOutMethod(obj, "OutputsBSTR", "Boo");
    TestOutMethod(obj, "OutputsI4",99999);
    TestOutMethod(obj, "OutputsUI1", 99);
    TestOutMethod(obj, "OutputsI2",9999);
    TestOutMethod(obj, "OutputsR4",999999.1);
    TestOutMethod(obj, "OutputsR8", 99999999999.99);
    TestOutMethod(obj, "OutputsBool", true);

    var outParam = new Object();
    obj.OutputsIDispatch(outParam);
    compareObject(outParam.value, nsXPCDispSimpleDesc, "Test method - Output params"); 
    TestOutMethod(obj, "OutputsError", E_FAIL);
    TestOutMethod(obj, "OutputsDate", "5/2/2002");
    // TestOutMethod(obj, OutputsIUnknown(IUnknown * * result)
    TestOutMethod(obj, "OutputsI1", 120);
    TestOutMethod(obj, "OutputsUI2", 9999);
    TestOutMethod(obj, "OutputsUI4", 3000000000);

    printStatus("Test method - In/out params");
    TestInOutMethod(obj, "InOutsBSTR", "String", "StringAppended");
    TestInOutMethod(obj, "InOutsI4", 2000000, -2000000);
    TestInOutMethod(obj, "InOutsUI1", 50, 8);
    TestInOutMethod(obj, "InOutsI2", -1000, 9000);
    TestInOutMethod(obj, "InOutsR4", -2.05, 3.0);
    TestInOutMethod(obj, "InOutsR8", -5.5, 49999994.50000005);
    TestInOutMethod(obj, "InOutsBool", true, false);
    TestInOutMethod(obj, "InOutsBool", false, true);
    var inoutParam = { value : {ClassName : "InOutTest", Number : 10 } };
    try
    {
        obj.InOutsIDispatch(inoutParam);
        if (inoutParam.value.Number != 15)
            reportFailure("Testing in/out parameter failed: IDispatch");
    }
    catch (e)
    {
        reportFailure("Testing in/out parameter failed: IDispatch - " + e.toString());
    }
    // See the TakesError case
    //TestInOutMethod(obj, "InOutsError", E_FAIL, E_FAIL + 1);
    TestInOutMethod(obj, "InOutsDate", "5/23/2001", "5/24/2001");
    //TestInOutMethod(obj, InOutsIUnknown(IUnknown * * result)
    TestInOutMethod(obj, "InOutsI1", -42, -41);
    TestInOutMethod(obj, "InOutsUI2", 43, 85);
    TestInOutMethod(obj, "InOutsUI4", 88, 46);

    try
    {
        reportCompare(obj.OneParameterWithReturn(-20), 22, "Testing OneParameterWithReturn");
    }
    catch (e)
    {
        reportFailure("Testing OneParameterWithReturn: " + e.toString());
    }

    try
    {
        reportCompare(obj.StringInputAndReturn("A String "), "A String Appended", "Testing StringInputAndReturn");
    }
    catch (e)
    {
        reportFailure("Testing StringInputAndReturn: " + e.toString());
    }
    try
    {
        var inoutIDispatch = {  className : "inouttest", Number : 5 };
        var result = obj.IDispatchInputAndReturn(inoutIDispatch);
        reportCompare(81, result.Number, "IDispatchInputAndReturn");
    }
    catch (e)
    {
        reportFailure("Testing IDispatchInputAndReturn: " + e.toString());
    }
    try
    {
        obj.TwoParameters(1, 2);
    }
    catch (e)
    {
        reportFailure("Testing TwoParameters: " + e.toString());
    }

    try
    {
        obj.TwelveInParameters(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
    }
    catch (e)
    {
        reportFailure("Testing TwelveInParameters: " + e.toString());
    }

    try
    {
        var out1 = new Object();
        var out2 = new Object();
        var out3 = new Object();
        var out4 = new Object();
        var out5 = new Object();
        var out6 = new Object();
        var out7 = new Object();
        var out8 = new Object();
        var out9 = new Object();
        var out10 = new Object();
        var out11 = new Object();
        var out12 = new Object();
        obj.TwelveOutParameters(out1, out2, out3, out4, out5, out6, out7, out8, out9, out10, out11, out12);
        reportCompare(78, out1.value + out2.value + out3.value + out4.value + out5.value + out6.value + out7.value + out8.value + out9.value + out10.value + out11.value + out12.value, "Testing TwelveOutParameters");
    }
    catch (e)
    {
        reportFailure("Testing TwelveOutParameters: " + e.toString());
    }

    try
    {
        obj.TwelveStrings("one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "eleven", "twelve");
    }
    catch (e)
    {
        reportFailure("Testing TwelveStrings: " + e.toString());
    }

    try
    {
        var out1 = new Object();
        var out2 = new Object();
        var out3 = new Object();
        var out4 = new Object();
        var out5 = new Object();
        var out6 = new Object();
        var out7 = new Object();
        var out8 = new Object();
        var out9 = new Object();
        var out10 = new Object();
        var out11 = new Object();
        var out12 = new Object();
        obj.TwelveOutStrings(out1, out2, out3, out4, out5, out6, out7, out8, out9, out10, out11, out12);
        reportCompare(true, out1.value == "one" && out2.value == "two" &&
                            out3.value == "three" && out4.value == "four" &&
                            out5.value == "five" && out6.value == "six" &&
                            out7.value == "seven" && out8.value == "eight" &&
                            out9.value == "nine" && out10.value == "ten" &&
                            out11.value == "eleven" && out12.value == "twelve",
                      "Testing TwelveOutString");
    }
    catch (e)
    {
        reportFailure("Testing TwelveOutParameters: " + e.toString());
    }
    try
    {
        var out1 = { className : "out1", Number : 5 };
        var out2 = { className : "out2", Number : 5 };
        var out3 = { className : "out3", Number : 5 };
        var out4 = { className : "out4", Number : 5 };
        var out5 = { className : "out5", Number : 5 };
        var out6 = { className : "out6", Number : 5 };
        var out7 = { className : "out7", Number : 5 };
        var out8 = { className : "out8", Number : 5 };
        var out9 = { className : "out9", Number : 5 };
        var out10 = { className : "out10", Number : 5 };
        var out11 = { className : "out11", Number : 5 };
        var out12 = { className : "out12", Number : 5 };
        obj.TwelveIDispatch(out1, out2, out3, out4, out5, out6, out7, out8, out9, out10, out11, out12);
    }
    catch (e)
    {
        reportFailure("Testing TwelveIDispatch: " + e.toString());
    }
    try
    {
        var out1 = new Object();
        var out2 = new Object();
        var out3 = new Object();
        var out4 = new Object();
        var out5 = new Object();
        var out6 = new Object();
        var out7 = new Object();
        var out8 = new Object();
        var out9 = new Object();
        var out10 = new Object();
        var out11 = new Object();
        var out12 = new Object();
        obj.TwelveOutIDispatch(out1, out2, out3, out4, out5, out6, out7, out8, out9, out10, out11, out12);
        print("out1.value" + out1.value.Number);
        reportCompare(true, out1.value.Number == 5 && out2.value.Number == 5 &&
                            out3.value.Number == 5 && out4.value.Number == 5 &&
                            out5.value.Number == 5 && out6.value.Number == 5 &&
                            out7.value.Number == 5 && out8.value.Number == 5 &&
                            out9.value.Number == 5 && out10.value.Number == 5 &&
                            out11.value.Number == 5 && out12.value.Number == 5,
                      "Testing TwelveOutIDispatch");
        compareObject(out1.value, nsXPCDispSimpleDesc, "Testing TwelveOutParameters - out1");
        compareObject(out2.value, nsXPCDispSimpleDesc, "Testing TwelveOutParameters - out2");
        compareObject(out3.value, nsXPCDispSimpleDesc, "Testing TwelveOutParameters - out3");
        compareObject(out4.value, nsXPCDispSimpleDesc, "Testing TwelveOutParameters - out4");
        compareObject(out5.value, nsXPCDispSimpleDesc, "Testing TwelveOutParameters - out5");
        compareObject(out6.value, nsXPCDispSimpleDesc, "Testing TwelveOutParameters - out6");
        compareObject(out7.value, nsXPCDispSimpleDesc, "Testing TwelveOutParameters - out7");
        compareObject(out8.value, nsXPCDispSimpleDesc, "Testing TwelveOutParameters - out8");
        compareObject(out9.value, nsXPCDispSimpleDesc, "Testing TwelveOutParameters - out9");
        compareObject(out10.value, nsXPCDispSimpleDesc, "Testing TwelveOutParameters - out10");
        compareObject(out11.value, nsXPCDispSimpleDesc, "Testing TwelveOutParameters - out11");
        compareObject(out12.value, nsXPCDispSimpleDesc, "Testing TwelveOutParameters - out12");
    }
    catch (e)
    {
        reportFailure("Testing TwelveOutIDispatch: " + e.toString());
    }

    try
    {
        obj.CreateError();
        reportFailure("Testing CreateError: Didn't generate a catchable exception");
    }
    catch (e)
    {
        reportCompare(true, e.toString().search("CreateError Test") != -1, "Testing CreateError");
    }
}
