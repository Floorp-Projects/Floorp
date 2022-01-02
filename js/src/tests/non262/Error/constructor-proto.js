const nativeErrors = [
    InternalError,
    EvalError,
    RangeError,
    ReferenceError,
    SyntaxError,
    TypeError,
    URIError
];

assertEq(Reflect.getPrototypeOf(Error), Function.prototype)

for (const error of nativeErrors)
    assertEq(Reflect.getPrototypeOf(error), Error);

if (typeof reportCompare === "function")
    reportCompare(0, 0);
