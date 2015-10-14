if (!('oomAtAllocation' in this && 'resetOOMFailure' in this))
    quit();

function f() {
    var i = 1;
    do {
        try {
            oomAtAllocation(i);
            (function() y)();
        } catch (e) {
            x = resetOOMFailure();
        }
        i++;
    } while (x);
}
f();
fullcompartmentchecks(true);
