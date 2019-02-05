Object.prototype[Symbol.iterator] = Array.prototype[Symbol.iterator];

// Just ensure it doesn't crash.
Uint8Array.from(0);
