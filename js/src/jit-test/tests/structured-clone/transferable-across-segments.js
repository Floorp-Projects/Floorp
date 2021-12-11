// Default capacity is 4096 bytes and each entry requires 24 bytes, so when
// the transferables list contains >170 entries, more than one segment is
// used, because 171 * 24 = 4104 and 4104 > 4096.

const transferables = [];
for (let i = 0; i < 170 + 1; ++i) {
  transferables.push(new ArrayBuffer(1));
}

// Just don't crash.
serialize([], transferables, {
  scope: "DifferentProcess",
});
