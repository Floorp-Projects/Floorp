// |jit-test| allow-oom; allow-unhandlable-oom
if (!("oomAfterAllocations" in this && "gczeal" in this))
    quit();
gczeal(0);
"".search(function() {});
r = /x/;
x = [/y/];
oomAfterAllocations(5);
x.v = function() {};
a = /z/;
