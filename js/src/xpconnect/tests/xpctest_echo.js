/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* xpconnect testing using the nsIEcho interface. */

function nsNativeEcho()
{
    var obj = Components.classes.nsEcho.createInstance();
    obj = obj.QueryInterface(Components.interfaces.nsIEcho);
    return obj;
}    

function nsID(str)
{
    var id = Components.classes.nsIID.createInstance();
    id = id.QueryInterface(Components.interfaces.nsIJSIID);
    id.init(str);
    return id;
}

var NS_ISUPPORTS_IID    = new nsID("{00000000-0000-0000-c000-000000000046}");
var NS_ITESTXPC_FOO_IID = new nsID("{159E36D0-991E-11d2-AC3F-00C09300144B}");

print();
print("Components = "+Components);
print("Components properties:");
for(i in Components)
    print("  Components."+i+" = "+Components[i]);

print();
/***************************************************************************/
print(".......................................");
print("echo tests...");

var echo = new nsNativeEcho;

var receiver = new Object();
receiver.SetReceiver = function() {};
receiver.SendOneString = function(str) {receiver_results[0] = str;};
receiver.SendManyTypes = function()
    {
        for(var i = 0; i < arguments.length; i++)
            receiver_results[i] = arguments[i];
    };

echo.SetReceiver(receiver);

////////////////////
// SendOneString

var test_string = "some string";
var receiver_results = new Object();
echo.SendOneString(test_string);
print("SendOneString - "+(
       receiver_results[0] == test_string
       ? "passed" : "failed"));

////////////////////
// In2OutOneInt

print("In2OutOneInt - "+(
       echo.In2OutOneInt(102) == 102
       ? "passed" : "failed"));

////////////////////
// In2OutAddTwoInts

var in_out_results1 = new Object();
var in_out_results2 = new Object();
var in_out_results =
        echo.In2OutAddTwoInts(123, 55, in_out_results1, in_out_results2);
print("In2OutAddTwoInts - "+(
       in_out_results1.value == 123 &&
       in_out_results2.value ==  55 &&
       in_out_results      == 178
       ? "passed" : "failed"));

var test_string2 = "some other string";
print("In2OutOneString - "+(
       echo.In2OutOneString(test_string2) == test_string2 &&
       echo.In2OutOneString(echo.In2OutOneString(test_string2)) == test_string2
       ? "passed" : "failed"));


////////////////////
// SendManyTypes

var receiver_results = new Object();
var send_params = [1,-2,-3,-102020,2,4,6,1023,1.5,2.000008,true,'a','b',NS_ITESTXPC_FOO_IID,"a string","another string"];
echo.SendManyTypes(send_params[0],
                   send_params[1],
                   send_params[2],
                   send_params[3],
                   send_params[4],
                   send_params[5],
                   send_params[6],
                   send_params[7],
                   send_params[8],
                   send_params[9],
                   send_params[10],
                   send_params[11],
                   send_params[12],
                   send_params[13],
                   send_params[14],
                   send_params[15]);

var all_ok = true;
for(i = 0; i < 16; i++) {
    if(((""+receiver_results[i]).toLowerCase()) !=
        ((""+send_params[i]).toLowerCase())) {
        if(all_ok)
            print("SendManyTypes - failed...");
        all_ok = false;
        print("    param number "+i+" diff: "+send_params[i]+" -> "+receiver_results[i])
    }
}
if(all_ok)
    print("SendManyTypes - passed");

////////////////////
// SendInOutManyTypes

var receiver_results = new Object();
var send_params   = [1,-2,-3,-102020,2,4,6,1023,1.5,2.000008,true,'a','b',NS_ITESTXPC_FOO_IID,"a string","another string"];
var resend_params = [2,-3,-7,-10220,18,14,16,123,2.5,8.000008,false,'z','l',NS_ISUPPORTS_IID,"foo string","yet another string"];

receiver.SendInOutManyTypes = function()
    {
        for(var i = 0; i < arguments.length; i++) {
            receiver_results[i] = arguments[i].value;
            arguments[i].value = resend_params[i];
        }
    };

var inout_params = [{value:send_params[0] },
                    {value:send_params[1] },
                    {value:send_params[2] },
                    {value:send_params[3] },
                    {value:send_params[4] },
                    {value:send_params[5] },
                    {value:send_params[6] },
                    {value:send_params[7] },
                    {value:send_params[8] },
                    {value:send_params[9] },
                    {value:send_params[10]},
                    {value:send_params[11]},
                    {value:send_params[12]},
                    {value:send_params[13]},
                    {value:send_params[14]},
                    {value:send_params[15]}];

echo.SendInOutManyTypes(inout_params[0] ,
                        inout_params[1] ,
                        inout_params[2] ,
                        inout_params[3] ,
                        inout_params[4] ,
                        inout_params[5] ,
                        inout_params[6] ,
                        inout_params[7] ,
                        inout_params[8] ,
                        inout_params[9] ,
                        inout_params[10],
                        inout_params[11],
                        inout_params[12],
                        inout_params[13],
                        inout_params[14],
                        inout_params[15]);

var all_ok = true;
for(i = 0; i < 16; i++) {
    if(((""+receiver_results[i]).toLowerCase()) !=
        ((""+send_params[i]).toLowerCase())) {
        if(all_ok)
            print("SendInOutManyTypes - failed...");
        all_ok = false;
        print("    sent param number "+i+" diff: "+send_params[i]+" -> "+receiver_results[i]);
    }
}

for(i = 0; i < 16; i++) {
    if(((""+resend_params[i]).toLowerCase()) !=
        ((""+inout_params[i].value).toLowerCase())) {
        if(all_ok)
            print("SendInOutManyTypes - failed...");
        all_ok = false;
        print("    resent param number "+i+" diff: "+resend_params[i]+" -> "+inout_params[i].value);
    }
}

if(all_ok)
    print("SendInOutManyTypes - passed");

////////////////////
// check exceptions on xpcom error code

try {
    echo.ReturnCode(0);
    print("ReturnCode(0) - passed");
}
catch(e) {
    print("ReturnCode(0) exception text: "+e+" - failed");
}

try {
    echo.ReturnCode(-1);
    print("ReturnCode(-1) - failed");
}
catch(e) {
//    print("ReturnCode(-1) exception text: "+e+" - passed");
    print("ReturnCode(-1) - passed");
}

var all_ok = true;

echo.ReturnCode_NS_OK()
if(Components.RESULT_NS_OK != Components.lastResult) {
    all_ok = false;
    print("expected: RESULT_NS_OK = "+Components.RESULT_NS_OK+"  got: "+Components.lastResult);
    print(Components.lastResult);
}

echo.ReturnCode_NS_COMFALSE()
if(Components.RESULT_NS_COMFALSE != Components.lastResult) {
    all_ok = false;
    print("expected: RESULT_NS_COMFALSE = "+Components.RESULT_NS_COMFALSE+"  got: "+Components.lastResult);
}

try {
    echo.ReturnCode_NS_ERROR_NULL_POINTER()
    all_ok = false;
} catch(e) {
    if(Components.RESULT_NS_ERROR_NULL_POINTER != Components.lastResult) {
        all_ok = false;
        print("expected: RESULT_NS_ERROR_NULL_POINTER = "+Components.RESULT_NS_ERROR_NULL_POINTER+"  got: "+Components.lastResult);
    }
}

try {
    echo.ReturnCode_NS_ERROR_UNEXPECTED()
    all_ok = false;
} catch(e) {
    if(Components.RESULT_NS_ERROR_UNEXPECTED != Components.lastResult) {
        all_ok = false;
        print("expected: RESULT_NS_ERROR_UNEXPECTED = "+Components.RESULT_NS_ERROR_UNEXPECTED+"  got: "+Components.lastResult);
    }
}

try {
    echo.ReturnCode_NS_ERROR_OUT_OF_MEMORY()
    all_ok = false;
} catch(e) {
    if(Components.RESULT_NS_ERROR_OUT_OF_MEMORY != Components.lastResult) {
        all_ok = false;
        print("expected: RESULT_NS_ERROR_OUT_OF_MEMORY = "+Components.RESULT_NS_ERROR_OUT_OF_MEMORY+"  got: "+Components.lastResult);
    }
}

print("Components.lastResult test -  "+ (all_ok ? "passed" : "failed") );

////////////////////
// check exceptions on too few args

try {
    echo.ReturnCode();  // supposed to have one arg
    print("Too few args test - failed");
}
catch(e) {
//    print("Too few args test -- exception text: "+e+" - passed");
    print("Too few args test - passed");
}

////////////////////
// check exceptions on can't convert

// XXX this is bad test since null is now convertable.
/*
try {
    echo.SetReceiver(null);
//    print("Can't convert arg to Native ("+out+")- failed");
    print("Can't convert arg to Native - failed");
}
catch(e) {
//    print("Can't convert arg to Native ("+e+") - passed");
    print("Can't convert arg to Native - passed");
}
*/
////////////////////
// FailInJSTest

var receiver3 = new Object();
receiver3.SetReceiver = function() {};
receiver3.FailInJSTest = function(fail) {if(fail)throw("");};
echo.SetReceiver(receiver3);

var all_ok = true;

try {
    echo.FailInJSTest(false);
}
catch(e) {
    print("FailInJSTest - failed");
    all_ok = false;
}
try {
    echo.FailInJSTest(true);
    print("FailInJSTest - failed");
    all_ok = false;
}
catch(e) {
}

if(all_ok)
    print("FailInJSTest - passed");

////////////////////
// nsID tests...

function idTest(name, iid, same)
{
    result = true;
    var idFromName = new nsID(name);
    var idFromIID  = new nsID(iid);

    if(!idFromName.valid || !idFromIID.valid) {
        return (same && idFromName.valid == idFromIID.valid) ||
               (!same && idFromName.valid != idFromIID.valid);
    }

    if(same != idFromName.equals(idFromIID) ||
       same != idFromIID.equals(idFromName)) {
        print("iid equals test failed for "+name+" "+iid);
        result = false;
    }

    nameNormalized = name.toLowerCase();
    iidNormalized  = iid.toLowerCase();

    idFromName_NameNormalized = idFromName.name ?
                                    idFromName.name.toLowerCase() :
                                    idFromName.name;

    idFromIID_NameNormalized = idFromIID.name ?
                                    idFromIID.name.toLowerCase() :
                                    idFromIID.name;

    idFromName_StringNormalized = idFromName.number ?
                                    idFromName.number.toLowerCase() :
                                    idFromName.number;

    idFromIID_StringNormalized = idFromIID.number ?
                                    idFromIID.number.toLowerCase() :
                                    idFromIID.number;

    if(idFromName_NameNormalized != nameNormalized ||
       same != (idFromIID_NameNormalized == nameNormalized)) {
        print("iid toName test failed for "+name+" "+iid);
        result = false;
    }

    if(idFromIID_StringNormalized != iidNormalized ||
       same != (idFromName_StringNormalized == iidNormalized)) {
        print("iid toString test failed for "+name+" "+iid);
        result = false;
    }

    if(!idFromName.equals(new nsID(idFromName)) ||
       !idFromIID.equals(new nsID(idFromIID))) {
        print("new id from id test failed for "+name+" "+iid);
        result = false;
    }

    return result;
}

var all_ok = true;
// these 4 should be valid and the same
all_ok = idTest("nsISupports",   "{00000000-0000-0000-c000-000000000046}", true)  && all_ok;
all_ok = idTest("nsITestXPCFoo", "{159E36D0-991E-11d2-AC3F-00C09300144B}", true)  && all_ok;
all_ok = idTest("nsITestXPCFoo2","{5F9D20C0-9B6B-11d2-9FFE-000064657374}", true)  && all_ok;
all_ok = idTest("nsIEcho",       "{CD2F2F40-C5D9-11d2-9838-006008962422}", true)  && all_ok;
// intentional mismatches
all_ok = idTest("nsISupports",   "{CD2F2F40-C5D9-11d2-9838-006008962422}", false) && all_ok;
all_ok = idTest("nsITestXPCFoo", "{00000000-0000-0000-c000-000000000046}", false) && all_ok;
// intentional bad name
all_ok = idTest("bogus",         "{CD2F2F40-C5D9-11d2-9838-006008962422}", false) && all_ok;
// intentional bad iid
all_ok = idTest("nsISupports",   "{XXXXXXXX-C5D9-11d2-9838-006008962422}", false) && all_ok;
// intentional bad name AND iid
all_ok = idTest("bogus",         "{XXXXXXXX-C5D9-11d2-9838-006008962422}", true)  && all_ok;

print("nsID tests - "+(all_ok ? "passed" : "failed"));

/***************************************************************************/

all_ok = echo.SharedString() == "a static string";
print("[shared] test - "+(all_ok ? "passed" : "failed"));

/***************************************************************************/
// Components object test...
// print(".......................................");

// print("Components = "+Components);
// print("Components.interfaces = "+Components.interfaces);
// print("Components.interfaces.nsISupports = "+Components.interfaces.nsISupports);
// print("Components.interfaces.nsISupports.name = "+Components.interfaces.nsISupports.name);
// print("Components.interfaces.nsISupports.number = "+Components.interfaces.nsISupports.number);
//
// print("Components.interfaces.nsIEcho.number = "+Components.interfaces.nsIEcho.number);
// print("Components.interfaces['{CD2F2F40-C5D9-11d2-9838-006008962422}'] = "+Components.interfaces['{CD2F2F40-C5D9-11d2-9838-006008962422}']);
// print("Components.interfaces['{CD2F2F40-C5D9-11d2-9838-006008962422}'].name = "+Components.interfaces['{CD2F2F40-C5D9-11d2-9838-006008962422}'].name);
//
// print("Components.classes = "+Components.classes);
// print("Components.classes.nsIID = "+Components.classes.nsIID);
// print("Components.classes.nsCID = "+Components.classes.nsCID);
// print("Components.classes.nsCID.name = "+Components.classes.nsCID.name);

/***************************************************************************/

print(".......................................");
print("simple speed tests...");

var iterations = 1000;

var receiver2 = new Object();
receiver2.SetReceiver = function() {};
receiver2.SendOneString = function(str) {/*print(str);*/};

var echoJS = new Object();
echoJS.SetReceiver = function(r) {this.r = r;};
echoJS.SendOneString = function(str) {if(this.r)this.r.SendOneString(str)};
echoJS.SimpleCallNoEcho = function(){}

/*********************************************/
/*********************************************/

print("\nEcho.SimpleCallNoEcho (just makes call with no params and no callback)");
var start_time = new Date().getTime()/1000;
echoJS.SetReceiver(receiver2);
for(i = 0; i < iterations; i++)
    echoJS.SimpleCallNoEcho();
var end_time = new Date().getTime()/1000;
var interval = parseInt(100*(end_time - start_time),10)/100;
print("JS control did "+iterations+" iterations in "+interval+ " seconds.");

var start_time = new Date().getTime()/1000;
echo.SetReceiver(receiver2);
for(i = 0; i < iterations; i++)
    echo.SimpleCallNoEcho();
var end_time = new Date().getTime()/1000;
var interval = parseInt(100*(end_time - start_time),10)/100;
print("XPConnect  did "+iterations+" iterations in "+interval+ " seconds.");

/*********************************************/

print("\nEcho.SendOneString (calls a callback that does a call)");
var start_time = new Date().getTime()/1000;
echoJS.SetReceiver(receiver2);
for(i = 0; i < iterations; i++)
    echoJS.SendOneString("foo");
var end_time = new Date().getTime()/1000;
var interval = parseInt(100*(end_time - start_time),10)/100;
print("JS control did "+iterations+" iterations in "+interval+ " seconds.");

var start_time = new Date().getTime()/1000;
echo.SetReceiver(receiver2);
for(i = 0; i < iterations; i++)
    echo.SendOneString("foo");
var end_time = new Date().getTime()/1000;
var interval = parseInt(100*(end_time - start_time),10)/100;
print("XPConnect  did "+iterations+" iterations in "+interval+ " seconds.");

print(".......................................");

if(0){
print();
print("this = "+this);
print("global object properties:");
for(i in this)
    print("  this."+i+" = "+ (typeof(this[i])!="function"? this[i] : "[function]"));

print();
}

echoJS.SetReceiver(null);
echo.SetReceiver(null);

