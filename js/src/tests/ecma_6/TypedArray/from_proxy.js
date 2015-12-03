const constructors = [
    Int8Array,
    Uint8Array,
    Uint8ClampedArray,
    Int16Array,
    Uint16Array,
    Int32Array,
    Uint32Array,
    Float32Array,
    Float64Array ];

if (typeof SharedArrayBuffer != "undefined")
    constructors.push(sharedConstructor(Int8Array),
		      sharedConstructor(Uint8Array),
		      sharedConstructor(Int16Array),
		      sharedConstructor(Uint16Array),
		      sharedConstructor(Int32Array),
		      sharedConstructor(Uint32Array),
		      sharedConstructor(Float32Array),
		      sharedConstructor(Float64Array));

for (var constructor of constructors) {
    // Two tests involving %TypedArray%.from and a Proxy.
    var log = [];
    function LoggingProxy(target) {
        log.push("target", target);
        var h = {
            defineProperty: function (t, id) {
                log.push("define", id);
                return true;
            },
            has: function (t, id) {
                log.push("has", id);
                return id in t;
            },
            get: function (t, id) {
                log.push("get", id);
                return t[id];
            },
            set: function (t, id, v) {
                log.push("set", id);
                t[id] = v;
                return true;
            }
        };
        return new Proxy(Object(target), h);
    }

    // Unlike Array.from, %TypedArray%.from uses [[Put]] instead of [[DefineOwnProperty]].
    // Hence, it calls handler.set to create new elements rather than handler.defineProperty.
    // Additionally, it doesn't set the length property at the end.
    LoggingProxy.from = constructor.from;
    LoggingProxy.from([3, 4, 5]);
    assertDeepEq(log, ["target", 3, "set", "0", "set", "1", "set", "2"]);

    // When the argument passed to %TypedArray%.from is a Proxy, %TypedArray%.from
    // calls handler.get on it.
    log = [];
    assertDeepEq(constructor.from(new LoggingProxy([3, 4, 5])), new constructor([3, 4, 5]));
    assertDeepEq(log, ["target", [3, 4, 5],
                       "get", Symbol.iterator,
                       "get", "length", "get", "0",
                       "get", "length", "get", "1",
                       "get", "length", "get", "2",
                       "get", "length"]);

    // Array-like iteration only gets the length once.
    log = [];
    var arr = [5, 6, 7];
    arr[Symbol.iterator] = undefined;
    assertDeepEq(constructor.from(new LoggingProxy(arr)), new constructor([5, 6, 7]));
    assertDeepEq(log, ["target", [5, 6, 7],
                       "get", Symbol.iterator,
                       "get", "length",
                       "get", "0", "get", "1", "get", "2"]);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
