// |jit-test| --enable-experimental-fields

class C {
    [Math.sqrt(16)];
    [Math.sqrt(8)] = 5 + 2;
    "hi";
    "bye" = {};
    2 = 2;
    0x101 = 2;
    0o101 = 2;
    0b101 = 2;
    NaN = 0; // actually the field called "NaN", not the number
    Infinity = 50; // actually the field called "Infinity", not the number
    // all the keywords below are proper fields (?!?)
    with = 0;
    //static = 0; // doesn't work yet
    async = 0;
    get = 0;
    set = 0;
    export = 0;
    function = 0;
}

let obj = new C();
assertEq(Math.sqrt(16) in obj, true);
assertEq(obj[Math.sqrt(16)], undefined);
assertEq(obj[Math.sqrt(8)], 7);
assertEq("hi" in obj, true);
assertEq(obj["hi"], undefined);
assertEq(typeof obj["bye"], "object");
assertEq(obj[2], 2);
assertEq(obj[0x101], 2);
assertEq(obj[0o101], 2);
assertEq(obj[0b101], 2);
assertEq(obj.NaN, 0);
assertEq(obj.Infinity, 50);
assertEq(obj.with, 0);
assertEq(obj.async, 0);
assertEq(obj.get, 0);
assertEq(obj.set, 0);
assertEq(obj.export, 0);
assertEq(obj.function, 0);

if (typeof reportCompare === "function")
  reportCompare(true, true);
