// |jit-test| allow-oom; allow-unhandlable-oom
gczeal(0);
function MyObject( value ) {}
gcparam("maxBytes", gcparam("gcBytes") + 4*(1));
gczeal(4);
function test() {}
var obj = new test();
