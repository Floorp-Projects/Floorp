var ta = new Uint32Array(10);

// Shadow the %TypedArray.prototype%.length getter.
Object.defineProperty(ta, "length", {value: 0x1000});

// Call Array.prototype.fill, not %TypedArray.prototype%.fill!
Array.prototype.fill.call(ta, 0);
