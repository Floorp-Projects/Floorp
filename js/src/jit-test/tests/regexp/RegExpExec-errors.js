load(libdir + "asserts.js");

assertErrorMessage(() => RegExp.prototype.test.call({}), TypeError,
                   /test method called on incompatible Object/);
assertErrorMessage(() => RegExp.prototype[Symbol.match].call([]), TypeError,
                   /\[Symbol\.match\] method called on incompatible Array/);
