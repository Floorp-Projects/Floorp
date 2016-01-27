var buffer = new ArrayBuffer(512 * 1024);
var ta = new Uint8Array(buffer);

function store() { ta[0x1234] = 42; }

store();
store();
store();

detachArrayBuffer(buffer, "change-data");

store();
