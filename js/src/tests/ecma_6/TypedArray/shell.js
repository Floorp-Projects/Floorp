// Synthesize a constructor for a shared memory array from the
// constructor for unshared memory.  This has "good enough" fidelity
// for many uses.  In cases where it's not good enough, use the
// __isShared__ flags or call isSharedConstructor for local workarounds.

function sharedConstructor(constructor) {
    var c = function(...args) {
	if (!new.target)
	    throw new TypeError("Not callable");
	var array = new constructor(...args);
	var buffer = array.buffer;
	var offset = array.byteOffset;
	var length = array.length;
	var sharedBuffer = new SharedArrayBuffer(buffer.byteLength);
	var sharedArray = new constructor(sharedBuffer, offset, length);
	for ( var i=0 ; i < length ; i++ )
	    sharedArray[i] = array[i];
	assertEq(sharedArray.buffer, sharedBuffer);
	return sharedArray;
    };
    c.prototype = Object.create(constructor.prototype);
    c.__isShared__ = true;
    c.__baseConstructor__ = constructor;
    c.from = constructor.from;
    c.of = constructor.of;
    return c;
}

function isSharedConstructor(x) {
    return typeof x == "function" && x.__isShared__;
}

function isFloatingConstructor(c) {
    return c == Float32Array ||
	c == Float64Array ||
	(c.hasOwnProperty("__baseConstructor__") &&
	 isFloatingConstructor(c.__baseConstructor__));
}
