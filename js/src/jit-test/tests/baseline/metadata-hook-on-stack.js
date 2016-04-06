// JSOP_NEWOBJECT should respect the metadata hook, even if
// it's set with scripts on the stack.

function f() {
    for (var i=0; i<100; i++) {
	if (i === 20)
	    enableShellAllocationMetadataBuilder();
	var o = {x: 1};
	if (i >= 20) {
	    var md = getAllocationMetadata(o);
	    assertEq(typeof md === "object" && md !== null, true);
	    assertEq(typeof md.index, "number");
	}
    }
}
f();
