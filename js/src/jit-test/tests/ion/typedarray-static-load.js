var buffer = new ArrayBuffer(512 * 1024);
var ta = new Uint8Array(buffer);

function load() { return ta[0x1234]; }

load();
load();
load();

detachArrayBuffer(buffer);

load();
