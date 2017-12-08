for (var constructor of anyTypedArrayConstructors) {
    // Note %TypedArray%.from(iterable) calls 'this' with an argument whose value is
    // `[...iterable].length`, while Array.from(iterable) doesn't pass any argument.
    constructor.from.call(function(len){
        assertEq(len, 3);
        return new constructor(len);
    }, Array(3));

    // If the 'this' value passed to %TypedArray.from is not a constructor,
    // then an exception is thrown, while Array.from will use Array as it's constructor.
    var arr = [3, 4, 5];
    var nonconstructors = [
        {}, Math, Object.getPrototypeOf, undefined, 17,
        () => ({})  // arrow functions are not constructors
    ];
    for (var v of nonconstructors) {
        assertThrowsInstanceOf(() => {
            constructor.from.call(v, arr);
        }, TypeError);
    }

    // %TypedArray%.from does not get confused if global constructors for typed arrays
    // are replaced with another constructor.
    function NotArray(...rest) {
        return new constructor(...rest);
    }
    var RealArray = constructor;
    NotArray.from = constructor.from;
    this[constructor.name] = NotArray;
    assertEq(RealArray.from([1]) instanceof RealArray, true);
    assertEq(NotArray.from([1]) instanceof RealArray, true);
    this[constructor.name] = RealArray;
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
