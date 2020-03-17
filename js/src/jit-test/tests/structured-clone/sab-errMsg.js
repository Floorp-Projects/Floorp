// |jit-test| skip-if: !sharedMemoryEnabled()

// Check the error mssage when the prefs for COOP/COEP are both enable or not.
var g = newGlobal();
var ex;
const sab = new SharedArrayBuffer();
try {
  g.serialize(sab);
} catch (e) {
  ex = e;
}
assertEq(ex.toString(),
         `TypeError: The SharedArrayBuffer object cannot be serialized. The ` +
         `Cross-Origin-Opener-Policy and Cross-Origin-Embedder-Policy HTTP ` +
         `headers will enable this in the future.`);

var h = newGlobal({enableCoopAndCoep: true});
try {
  h.serialize(sab);
} catch (e) {
  ex = e;
}
assertEq(ex.toString(),
         `TypeError: The SharedArrayBuffer object cannot be serialized. The ` +
         `Cross-Origin-Opener-Policy and Cross-Origin-Embedder-Policy HTTP ` +
         `headers can be used to enable this.`);
