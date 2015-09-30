if (!(this.resetOOMFailure && this.oomAtAllocation && this.gczeal && this.uneval))
    quit(0);

oomTest(((function() {
  try {
    gczeal(2)(uneval(this))
  } catch(e) {}
})));
function oomTest(f) {
    var i = 1;
    // Outer catch in case the inner catch fails due to OOM.
    try {
    do {
        try {
            oomAtAllocation(i);
            f();
        } catch (e) {}
        more = resetOOMFailure();
        i++;
	if (i > 1560)
	    print(i);
    } while(more);
    } catch (e) {}
}
