// |jit-test| allow-oom; allow-unhandlable-oom

if (!('oomAfterAllocations' in this))
    quit();

var buffer = new ArrayBuffer(100);
view = new DataView(buffer, undefined, undefined);
oomAfterAllocations(10);
view = new DataView(buffer, 20, undefined);
