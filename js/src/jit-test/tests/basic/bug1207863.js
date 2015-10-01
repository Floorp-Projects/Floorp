// |jit-test| allow-oom; allow-unhandlable-oom

if (!("oomAtAllocation" in this && "resetOOMFailure" in this))
    quit();

function oomTest(f) {
    var i = 1;
    do {
        try {
            oomAtAllocation(i);
            f();
        } catch (e) {
            more = resetOOMFailure();
        }
        i++;
    } while(more);
}
oomTest(
	() => 3
	| (function () {
	  "use strict";
	  return eval("f();");
	})()
);
