// Function to test OOM handling by repeatedly calling a function and failing
// successive allocations.

if (!("oomAtAllocation" in this && "resetOOMFailure" in this && "oomThreadTypes" in this))
    quit();

if ("gczeal" in this)
    gczeal(0);

const verbose = ("os" in this) && os.getenv("OOM_VERBOSE");

// Test out of memory handing by calling a function f() while causing successive
// memory allocations to fail.  Repeat until f() finishes without reaching the
// failing allocation.
function oomTest(f) {
    for (let thread = 1; thread < oomThreadTypes(); thread++) {
        if (verbose)
            print("testing thread " + thread);

        var i = 1;
        var more;
        do {
            if (verbose)
                print("fail at " + i);
            try {
                oomAtAllocation(i, thread);
                f();
                more = resetOOMFailure();
            } catch (e) {
                // Ignore exceptions.
                more = resetOOMFailure();
            }
            i++;
        } while(more);

        if (verbose)
            print("finished after " + (i - 2) + " failures");
    }
}
