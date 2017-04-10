// Failure to serialize an object containing a SAB should not leave the SAB's
// rawbuffer's reference count incremented.

if (!this.SharedArrayBuffer || !this.sharedArrayRawBufferRefcount) {
    reportCompare(true,true);
    quit(0);
}

let x = new SharedArrayBuffer(1);

// Initially the reference count is 1.
assertEq(sharedArrayRawBufferRefcount(x), 1);

let y = serialize(x);

// Serializing it successfully increments the reference count.
assertEq(sharedArrayRawBufferRefcount(x), 2);

// Serializing something containing a function should throw.
var failed = false;
try {
    serialize([x, function () {}]);
}
catch (e) {
    failed = true;
}
assertEq(failed, true);

// Serializing the SAB unsuccessfully does not increment the reference count.
assertEq(sharedArrayRawBufferRefcount(x), 2);

reportCompare(true, true);
