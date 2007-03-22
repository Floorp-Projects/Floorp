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

/*
var start_time = new Date().getTime()/1000;
echo.SetReceiver(receiver2);
for(i = 0; i < iterations; i++)
    echo.SimpleCallNoEcho();
var end_time = new Date().getTime()/1000;
var interval = parseInt(100*(end_time - start_time),10)/100;
print("XPConnect  did "+iterations+" iterations in "+interval+ " seconds.");
*/

/*********************************************/

print("\nEcho.SendOneString (calls a callback that does a call)");
var start_time = new Date().getTime()/1000;
echoJS.SetReceiver(receiver2);
for(i = 0; i < iterations; i++)
    echoJS.SendOneString("foo");
var end_time = new Date().getTime()/1000;
var interval = parseInt(100*(end_time - start_time),10)/100;
print("JS control did "+iterations+" iterations in "+interval+ " seconds.");

/*
var start_time = new Date().getTime()/1000;
echo.SetReceiver(receiver2);
for(i = 0; i < iterations; i++)
    echo.SendOneString("foo");
var end_time = new Date().getTime()/1000;
var interval = parseInt(100*(end_time - start_time),10)/100;
print("XPConnect  did "+iterations+" iterations in "+interval+ " seconds.");
*/

print(".......................................");

echoJS.SetReceiver(null);
