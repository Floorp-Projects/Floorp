var iface = Components.interfaces.nsIXPCTestArray;
var clazz = Components.classes["@mozilla.org/js/xpc/test/ArrayTest;1"];

var obj = clazz.createInstance(iface);

var a = [-1,0,1,2,3,4,5,4,3,2,1,0,-1]
obj.PrintIntegerArray(a.length, a);

var b = ["this","is","an","array"];
obj.PrintStringArray(b.length, b);

var o = {
    value : [1,2,3,4]
};
obj.MultiplyEachItemInIntegerArray(2, o.value.length, o);
print(o.value);

var len = {
    value : o.value.length
};

obj.MultiplyEachItemInIntegerArrayAndAppend(2, len, o);
print(o.value+" : "+len.value);


var strs = {
    value : ["this","is","to","be", "reversed"]
};

obj.ReverseStringArray(strs.value.length, strs);
print(strs.value);

var strs2 = {
    value : ["double","me","please"]
};

var strslen2 = {
    value : strs2.value.length
};

obj.DoubleStringArray(strslen2, strs2);
print(strs2.value);


print("-------------------------------------------");

var ech =  {
    SendOneString : function(s) {
        print(s);
    }
};

var iid =  {
    value : Components.interfaces.nsIEcho
};

var count =  {
    value : 3
};

var ifaces =  {
    value : [ech, ech, ech]
};

print("calling from JS...");

obj.CallEchoMethodOnEachInArray(iid, count, ifaces);

print(iid.value.equals(iface));
print(count.value);
for(i = 0; i < count.value; i++)
    ifaces.value[i].PrintIntegerArray(a.length, a);

print("-------------------------------------------");
print("-------------------------------------------");
print("-------------------------------------------");
print("-------------------------------------------");

var jsobj =  {
    PrintIntegerArray : function(len, a) {
        print("["+a+"]"+" : "+len);
    },
    PrintStringArray : function(len, a) {
        print("["+a+"]"+" : "+len);
    },
    MultiplyEachItemInIntegerArray : function(val, len, a) {
        for(var i = 0; i < len; i++)
            a.value[i] *= val;
    },
    MultiplyEachItemInIntegerArrayAndAppend : function(val, len, a) {
        var out = new Array(len.value*2);
        for(var i = 0; i < len.value; i++)
        {
            out[i*2]   = a.value[i];
            out[i*2+1] = a.value[i] * val;
        }
        len.value *= 2;
        a.value = out;
    },
    ReverseStringArray : function(len, a) {
        for(var i = 0; i < len/2; i++) {
            var temp = a.value[i];
            a.value[i] = a.value[len-1-i];
            a.value[len-1-i] = temp;
        }
    },
    DoubleStringArray : function(len, a) {
        var outArray = new Array();
        for(var i = 0; i < len.value; i++) {
            var inStr = a.value[i];
            var outStr = new String();
            for(var k = 0; k < inStr.length; k++) {
                outStr += inStr[k];
                outStr += inStr[k];
            }
            outArray.push(outStr);
            outArray.push(outStr);
        }
        len.value *= 2;
        a.value = outArray;
    },
    CallEchoMethodOnEachInArray : function(uuid, count, result) {
        if(!Components.interfaces.nsIEcho.equals(uuid.value))
            throw Components.results.NS_ERROR_FAILURE;

        for(var i = 0; i < count.value; i++)
            result.value[i].SendOneString("print this from JS");

        uuid.value = Components.interfaces.nsIXPCTestArray;

        result.value = [this, this];
        count.value = 2;
    },
    CallEchoMethodOnEachInArray2 : function(count, result) {
        for(var i = 0; i < count.value; i++)
            result.value[i].SendOneString("print this from JS");

        result.value = [this, this];
        count.value = 2;
    },
    PrintStringWithSize : function(len, a) {
        print("\""+a+"\""+" ; "+len);
    },
    DoubleString : function(len, s) {
        var out = "";
        for(var k = 0; k < s.value.length; k++) {
            out += s.value.charAt(k);
            out += s.value.charAt(k);
        }
        len.value *= 2;
        s.value = out;
    },

    QueryInterface : function(iid)  {
//        print("+++++QI called for "+iid);
        if(iid.equals(Components.interfaces.nsIEcho)) {
//            print("+++++QI matched nsIEcho");
            return this;
        }
        if(iid.equals(Components.interfaces.nsISupports)) {
//              print("+++++QI matched nsISupports");
            return this;
        }
        throw Components.results.NS_ERROR_NO_INTERFACE;  
    }
};

obj.SetReceiver(jsobj);

var a = [-1,0,1,2,3,4,5,4,3,2,1,0,-1]
obj.PrintIntegerArray(a.length, a);

var b = ["this","is","an","array"];
obj.PrintStringArray(b.length, b);

var o = {
    value : [1,2,3,4]
};
obj.MultiplyEachItemInIntegerArray(2, o.value.length, o);
print(o.value);

var len = {
    value : o.value.length
};
obj.MultiplyEachItemInIntegerArrayAndAppend(2, len, o);
print(o.value+" : "+len.value);

var strs = {
    value : ["this","is","to","be", "reversed"]
};

obj.ReverseStringArray(strs.value.length, strs);
print(strs.value);

var strs2 = {
    value : ["double","me","please", "sir"]
};

var strslen2 = {
    value : strs2.value.length
};

obj.DoubleStringArray(strslen2, strs2);
print(strs2.value);

print("-------------------------------------------");

var ech =  {
    SendOneString : function(s) {
        print(s);
    }
};

var iid =  {
    value : Components.interfaces.nsIEcho
};

var count =  {
    value : 3
};

var ifaces =  {
    value : [ech, ech, ech]
};

print("calling from JS to native to JS...");

obj.CallEchoMethodOnEachInArray(iid, count, ifaces);

print(iid.value.equals(iface));
print(count.value);
for(i = 0; i < count.value; i++)
    ifaces.value[i].PrintIntegerArray(a.length, a);

print("-------------------------------------------");
print("-------------------------------------------");


count =  {
    value : 3
};

ifaces =  {
    value : [ech, ech, ech]
};

print("calling from JS to native to JS 2...");

obj.SetReceiver(null);
obj.CallEchoMethodOnEachInArray2(count, ifaces);
count =  {
    value : 3
};

ifaces =  {
    value : [ech, ech, ech]
};
obj.SetReceiver(jsobj);
obj.CallEchoMethodOnEachInArray2(count, ifaces);


print("-------------------------------------------");

obj.SetReceiver(null);
str = "This is a string";
obj.PrintStringWithSize(str.length, str);
obj.SetReceiver(jsobj);
obj.PrintStringWithSize(str.length, str);

var str2 = {value : "double me baby"};
var len2 = {value : str2.value.length};
obj.SetReceiver(null);
print("\""+str2.value+"\""+" : "+len2.value);
obj.DoubleString(len2, str2);
print("\""+str2.value+"\""+" : "+len2.value);

var str2 = {value : "double me baby"};
var len2 = {value : str2.value.length};
obj.SetReceiver(jsobj);
print("\""+str2.value+"\""+" ; "+len2.value);
obj.DoubleString(len2, str2);
print("\""+str2.value+"\""+" ; "+len2.value);

print("-------------------------------------------");
print("GetStrings test...");
obj.SetReceiver(null);
var count = {};
var strings = obj.GetStrings(count);
print("count = "+ count.value);
print("strings = "+ strings);

print("-----------------------");

var gs = {
    GetStrings : function(count) {
        var s = ['red', 'green', 'blue'];
        count.value = s.length;
        return s;
    }
};

obj.SetReceiver(gs);
var count = {};
var strings = obj.GetStrings(count);
print("count = "+ count.value);
print("strings = "+ strings);

print("\n");
