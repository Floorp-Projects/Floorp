// Function to test OOM handling by repeatedly calling a function and failing
// successive allocations.

const verbose = false;

if (!("oomAtAllocation" in this && "resetOOMFailure" in this))
    quit();

if ("gczeal" in this)
    gczeal(0);

// Test out of memory handing by calling a function f() while causing successive
// memory allocations to fail.  Repeat until f() finishes without reaching the
// failing allocation.
function oomTest(f) {
    var i = 1;
    var more;
    do {
        if (verbose)
            print("fail at " + i);
        try {
            oomAtAllocation(i);
            f();
            more = resetOOMFailure();
        } catch (e) {
            // Ignore exceptions.
            more = resetOOMFailure();
        }
        i++;
    } while(more);

    if (verbose)
        print("finished after " + i);
}
