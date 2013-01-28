function failWrapper(callback) {
    try {
        callback(); // this should fail
        throw "test-error"; // and if it didn't we have a problem`
    } catch (e) {
        if (e == "test-error")
            throw ("Testing error when running " + callback.toString());
    }
}


print ("Deleting standard classes");
delete Function;
delete Object;
delete Array;
delete Boolean;
delete JSON;
delete Date;
delete Math;
delete Number;
delete String;
delete Regexp;
delete Reflect;
delete Proxy;
delete Error;
delete Iterator;
delete Generator;
delete StopIteration;
delete Float32Array;
delete Float64Array;
delete Int16Array;
delete Int32Array;
delete Int32Array;
delete Uint16Array;
delete Uint32Array;
delete Uint8Array;
delete Uint8ClampedArray;
delete Weakmap;


print ("Accessing standard classes shouldn't recreate them");
failWrapper(function () { Function; });
failWrapper(function () { Object; });
failWrapper(function () { Array; });
failWrapper(function () { Boolean; });
failWrapper(function () { JSON; });
failWrapper(function () { Date; });
failWrapper(function () { Math; });
failWrapper(function () { Number; });
failWrapper(function () { String; });
failWrapper(function () { Regexp; });
failWrapper(function () { Reflect; });
failWrapper(function () { Proxy; });
failWrapper(function () { Error; });
failWrapper(function () { Iterator; });
failWrapper(function () { Generator; });
failWrapper(function () { StopIteration; });
failWrapper(function () { Float32Array; });
failWrapper(function () { Float64Array; });
failWrapper(function () { Int16Array; });
failWrapper(function () { Int32Array; });
failWrapper(function () { Int32Array; });
failWrapper(function () { Uint16Array; });
failWrapper(function () { Uint32Array; });
failWrapper(function () { Uint8Array; });
failWrapper(function () { Uint8ClampedArray; });
failWrapper(function () { Weakmap; });


print ("Enumerate over the global object");
for (c in this) {}

print ("That shouldn't have recreated the standard classes either");
failWrapper(function () { Function; });
failWrapper(function () { Object; });
failWrapper(function () { Array; });
failWrapper(function () { Boolean; });
failWrapper(function () { JSON; });
failWrapper(function () { Date; });
failWrapper(function () { Math; });
failWrapper(function () { Number; });
failWrapper(function () { String; });
failWrapper(function () { Regexp; });
failWrapper(function () { Reflect; });
failWrapper(function () { Proxy; });
failWrapper(function () { Error; });
failWrapper(function () { Iterator; });
failWrapper(function () { Generator; });
failWrapper(function () { StopIteration; });
failWrapper(function () { Float32Array; });
failWrapper(function () { Float64Array; });
failWrapper(function () { Int16Array; });
failWrapper(function () { Int32Array; });
failWrapper(function () { Int32Array; });
failWrapper(function () { Uint16Array; });
failWrapper(function () { Uint32Array; });
failWrapper(function () { Uint8Array; });
failWrapper(function () { Uint8ClampedArray; });
failWrapper(function () { Weakmap; });

print ("success");
