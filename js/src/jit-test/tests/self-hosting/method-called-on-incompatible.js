load(libdir + "asserts.js");

assertTypeErrorMessage(() => WeakSet.prototype.add.call({}), "add method called on incompatible Object");
assertTypeErrorMessage(() => newGlobal().WeakSet.prototype.add.call({}), "add method called on incompatible Object");
assertTypeErrorMessage(() => WeakSet.prototype.add.call(15), "add method called on incompatible number");

assertTypeErrorMessage(() => Int8Array.prototype.find.call({}), "find method called on incompatible Object");
assertTypeErrorMessage(() => newGlobal().Int8Array.prototype.find.call({}), "find method called on incompatible Object");
assertTypeErrorMessage(() => Int8Array.prototype.find.call(15), "find method called on incompatible number");
