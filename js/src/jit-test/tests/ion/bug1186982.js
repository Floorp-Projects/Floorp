if (!(this.resetOOMFailure && this.oomAtAllocation && this.gczeal && this.uneval))
    quit(0);

oomTest(((function() {
  try {
    gczeal(2)(uneval(this))
  } catch(e) {}
})));
function oomTest(f) {
    var i = 1;
    do {
        try {
            oomAtAllocation(i);
            f();
        } catch (e) {}
        more = resetOOMFailure();
        i++;
    } while(more);
}
