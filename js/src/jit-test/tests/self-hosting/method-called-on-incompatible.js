load(libdir + "asserts.js");

assertTypeErrorMessage(() => Set.prototype.forEach.call({}), "forEach method called on incompatible Object");
assertTypeErrorMessage(() => newGlobal({newCompartment: true}).Set.prototype.forEach.call({}), "forEach method called on incompatible Object");
assertTypeErrorMessage(() => Set.prototype.forEach.call(15), "forEach method called on incompatible number");

assertTypeErrorMessage(() => Int8Array.prototype.find.call({}), "find method called on incompatible Object");
assertTypeErrorMessage(() => newGlobal({newCompartment: true}).Int8Array.prototype.find.call({}), "find method called on incompatible Object");
assertTypeErrorMessage(() => Int8Array.prototype.find.call(15), "find method called on incompatible number");
