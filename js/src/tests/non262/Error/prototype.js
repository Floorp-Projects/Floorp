const nativeErrors = [
    InternalError,
    EvalError,
    RangeError,
    ReferenceError,
    SyntaxError,
    TypeError,
    URIError
];

assertEq(Reflect.getPrototypeOf(Error.prototype), Object.prototype)

for (const error of nativeErrors) {
    assertEq(Reflect.getPrototypeOf(error.prototype), Error.prototype);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
