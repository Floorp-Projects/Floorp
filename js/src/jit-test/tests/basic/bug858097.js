// |jit-test| allow-oom
function MyObject( value ) {}
gcparam("maxBytes", gcparam("gcBytes") + 4*(1));
gczeal(4);
function test() {}
var obj = new test();
