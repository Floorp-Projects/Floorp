// Function to test OOM handling by repeatedly calling a function and failing
// successive allocations.

const verbose = false;

if (!("oomAtAllocation" in this && "resetOOMFailure" in this))
    quit();

if ("gczeal" in this)
    gczeal(0);

function oomTest(f) {
    var i = 1;
    var more;
    do {
        if (verbose)
            print("fail at " + i);
        try {
            oomAtAllocation(i);
            f();
        } catch (e) {
            // Ignore exceptions.
        }
        more = resetOOMFailure();
        i++;
    } while(more);

    if (verbose)
        print("finished after " + i);
}
