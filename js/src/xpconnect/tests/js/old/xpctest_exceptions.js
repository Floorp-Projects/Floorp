
function printExceptionDoubleData(e) {
    if(!e || !e.data)
        return;
    if(e instanceof Components.interfaces.nsIXPCException) {
        try {
            var d = e.data.QueryInterface(Components.interfaces.nsISupportsDouble)
            try {
                print("e.data = "+d.data);
            } catch(i) {
                print("could not get double from e.data");

            }
        } catch(i) {
            print("e.data was not an nsISupportsDouble");
        }
    }
    else {
        print("object was not an nsIXPCException");
    }
}    

var clazz = Components.classes["@mozilla.org/js/xpc/test/Echo;1"];
var iface = Components.interfaces.nsIEcho;
var nativeEcho = clazz.createInstance(iface);


var localEcho =  {
    SendOneString : function() {throw this.result;},
    result : 0
};

nativeEcho.SetReceiver(localEcho);

localEcho.result = 0;

print("\n");
try {
    nativeEcho.SendOneString("foo");
    print("no error");
} catch(e)  {
    print("caught "+e);
    printExceptionDoubleData(e);
}

localEcho.result = 1;

print("\n");
try {
    nativeEcho.SendOneString("foo");
    print("no error");
} catch(e)  {
    print("caught "+e);
    printExceptionDoubleData(e);
}

localEcho.result = -1;

print("\n");
try {
    nativeEcho.SendOneString("foo");
    print("no error");
} catch(e)  {
    print("caught "+e);
    printExceptionDoubleData(e);
}

localEcho.result = Components.results.NS_ERROR_NO_AGGREGATION;

print("\n");
try {
    nativeEcho.SendOneString("foo");
    print("no error");
} catch(e)  {
    print("caught "+e);
    printExceptionDoubleData(e);
}

localEcho.result = 0x80040154;

print("\n");
try {
    nativeEcho.SendOneString("foo");
    print("no error");
} catch(e)  {
    print("caught "+e);
    printExceptionDoubleData(e);
}

print("\n");
localEcho.result = new Components.Exception("hi", Components.results.NS_ERROR_ABORT);
try {
    nativeEcho.SendOneString("foo");
    
} catch(e)  {
    print("caught "+e);
}

print("\n");
localEcho.result = {message : "pretending to be NS_OK does not work", result : 0};
try {
    nativeEcho.SendOneString("foo");
    print("pretending 0 == NS_OK");
} catch(e)  {
    print("caught "+e);
}

print("\n");
localEcho.result = {message : "pretending to be NS_ERROR_NO_INTERFACE", 
                    result : Components.results.NS_ERROR_NO_INTERFACE};
try {
    nativeEcho.SendOneString("foo");
} catch(e)  {
    print("caught "+e.message);
}

print("\n");
localEcho.result = "just a string";
try {
    nativeEcho.SendOneString("foo");
} catch(e)  {
    print("caught "+e);
}

print("\n");
localEcho.result = Components.results.NS_ERROR_NO_INTERFACE;
try {
    nativeEcho.SendOneString("foo");
} catch(e)  {
    print("caught "+e);
}

print("\n");
localEcho.result = "NS_OK";
try {
    nativeEcho.SendOneString("foo");
} catch(e)  {
    print("caught "+e);
}

print("\n");
localEcho.result = Components;
try {
    nativeEcho.SendOneString("foo");
} catch(e)  {
    print("caught "+e);
}

print("\n");
/* make this function have a JS runtime error */
localEcho.SendOneString = function(){return Components.foo.bar;};
try {
    nativeEcho.SendOneString("foo");
} catch(e)  {
    print("caught "+e);
}

print("\n");
/* make this function have a JS compiletime error */
localEcho.SendOneString = function(){new Function("","foo ===== 1")};
try {
    nativeEcho.SendOneString("foo");
} catch(e)  {
    print("caught "+e);
}


