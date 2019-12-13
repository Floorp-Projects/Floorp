const nativeErrors = [
    InternalError,
    EvalError,
    RangeError,
    ReferenceError,
    SyntaxError,
    TypeError,
    URIError
];

const expectedOwnKeys = "toSource" in Object.prototype
                        ? "toSource,toString,message,name,stack,constructor"
                        : "toString,message,name,stack,constructor";
assertEq(Reflect.ownKeys(Error.prototype).toString(), expectedOwnKeys);
assertEq(Error.prototype.name, "Error");
assertEq(Error.prototype.message, "");

for (const error of nativeErrors) {
    assertEq(Reflect.ownKeys(error.prototype).toString(), "message,name,constructor");
    assertEq(error.prototype.name, error.name);
    assertEq(error.prototype.message, "");
    assertEq(error.prototype.constructor, error);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
