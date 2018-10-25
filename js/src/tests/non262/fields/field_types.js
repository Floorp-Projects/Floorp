// * * * THIS TEST IS DISABLED - Fields are not fully implemented yet

class C {
    [Math.sqrt(4)];
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

if (typeof reportCompare === "function")
  reportCompare(true, true);
