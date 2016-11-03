load(libdir + "asserts.js");

assertThrowsInstanceOf(_=>getWaitForAllPromise(42), Error);
assertThrowsInstanceOf(_=>getWaitForAllPromise([42]), Error);
assertThrowsInstanceOf(_=>getWaitForAllPromise([{}]), Error);

// Shouldn't throw.
getWaitForAllPromise([Promise.resolve()]);
