// test case...

var NS_ISUPPORTS_IID    = new nsID("{00000000-0000-0000-c000-000000000046}");
var NS_ITESTXPC_FOO_IID = new nsID("{159E36D0-991E-11d2-AC3F-00C09300144B}");

var baz = foo.QueryInterface(NS_ITESTXPC_FOO_IID);
print("baz = "+baz);
print("distinct wrapper test "+ (foo != baz ? "passed" : "failed"));
var baz2 = foo.QueryInterface(NS_ITESTXPC_FOO_IID);
print("shared wrapper test "+ (baz == baz2 ? "passed" : "failed"));
print("root wrapper identity test "+ 
    (foo.QueryInterface(NS_ISUPPORTS_IID) == 
     baz.QueryInterface(NS_ISUPPORTS_IID) ? 
        "passed" : "failed"));

print("foo = "+foo);
foo.toString = new Function("return 'foo toString called';")
foo.toStr = new Function("return 'foo toStr called';")
print("foo = "+foo);
print("foo.toString() = "+foo.toString());
print("foo.toStr() = "+foo.toStr());
print("foo.five = "+ foo.five);
print("foo.six = "+ foo.six);
print("foo.bogus = "+ foo.bogus);
print("setting bogus explicitly to '5'...");
foo.bogus = 5;
print("foo.bogus = "+ foo.bogus);
print("foo.Test(10,20) returned: "+foo.Test(10,20));

function _Test(p1, p2)
{
    print("test called in JS with p1 = "+p1+" and p2 = "+p2);
    return p1+p2;
}

function _QI(iid)
{
    print("QueryInterface called in JS with iid = "+iid); 
    return  this;
}

print("creating bar");
bar = new Object();
bar.Test = _Test;
bar.QueryInterface = _QI;
// this 'bar' object is accessed from native code after this script is run

print("foo properties:");
for(i in foo)
    print("  foo."+i+" = "+foo[i]);

/***************************************************************************/
print(".......................................");
print("echo tests...");

var reciever = new Object();
reciever.SetReciever = function() {};
reciever.SendOneString = function(str) {reciever_results[0] = str;};
reciever.SendManyTypes = function() 
    {
        for(var i = 0; i < arguments.length; i++)
            reciever_results[i] = arguments[i];
    };

echo.SetReciever(reciever);

////////////////////
// SendOneString

var test_string = "some string";
var reciever_results = new Object();
echo.SendOneString(test_string);
print("SendOneString - "+(
       reciever_results[0] == test_string
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
       in_out_results1.val == 123 &&
       in_out_results2.val ==  55 &&
       in_out_results      == 178 
       ? "passed" : "failed"));

var test_string2 = "some other string";
print("In2OutOneString - "+(
       echo.In2OutOneString(test_string2) == test_string2 &&
       echo.In2OutOneString(echo.In2OutOneString(test_string2)) == test_string2 
       ? "passed" : "failed"));


////////////////////
// SendManyTypes

var reciever_results = new Object();
var send_params = [-1,-2,-3,-102020,2,4,6,1023,1.5,2.000008,true,'a','b',NS_ITESTXPC_FOO_IID,"a string","another string"];
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
                   send_params[15], 
                   send_params[16], 
                   send_params[17]);

var all_ok = true;
for(i = 0; i < 16; i++) {
    if((""+reciever_results[i]) != (""+send_params[i])) {
        if(all_ok)
            print("SendManyTypes - failed...");
        all_ok = false;
        print("    param number "+i+" diff: "+send_params[i]+" -> "+reciever_results[i])
    }
}
if(all_ok)
    print("SendManyTypes - passed");


print(".......................................");
print("simple speed tests...");

var iterations = 1000;

var reciever2 = new Object();
reciever2.SetReciever = function() {};
reciever2.SendOneString = function(str) {/*print(str);*/};

var echoJS = new Object();
echoJS.SetReciever = function(r) {this.r = r;};
echoJS.SendOneString = function(str) {if(this.r)this.r.SendOneString(str)};
echoJS.SimpleCallNoEcho = function(){}

/*********************************************/

print("\nEcho.SimpleCallNoEcho (just makes call with no params and no callback)");
var start_time = new Date().getTime()/1000;
echoJS.SetReciever(reciever2);
for(i = 0; i < iterations; i++)
    echoJS.SimpleCallNoEcho();
var end_time = new Date().getTime()/1000;
var interval = parseInt(100*(end_time - start_time),10)/100;
print("JS control did "+iterations+" iterations in "+interval+ " seconds.");

var start_time = new Date().getTime()/1000;
echo.SetReciever(reciever2);
for(i = 0; i < iterations; i++)
    echo.SimpleCallNoEcho();
var end_time = new Date().getTime()/1000;
var interval = parseInt(100*(end_time - start_time),10)/100;
print("XPConnect  did "+iterations+" iterations in "+interval+ " seconds.");

/*********************************************/

print("\nEcho.SendOneString (calls a callback that does a call)");
var start_time = new Date().getTime()/1000;
echoJS.SetReciever(reciever2);
for(i = 0; i < iterations; i++)
    echoJS.SendOneString("foo");
var end_time = new Date().getTime()/1000;
var interval = parseInt(100*(end_time - start_time),10)/100;
print("JS control did "+iterations+" iterations in "+interval+ " seconds.");

var start_time = new Date().getTime()/1000;
echo.SetReciever(reciever2);
for(i = 0; i < iterations; i++)
    echo.SendOneString("foo");
var end_time = new Date().getTime()/1000;
var interval = parseInt(100*(end_time - start_time),10)/100;
print("XPConnect  did "+iterations+" iterations in "+interval+ " seconds.");


print(".......................................");
