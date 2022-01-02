// |jit-test| error: ReferenceError
function reportCompare (expected, actual, description) {
    if (expected != actual) {}
}
reportCompare(1);
addThis();
function addThis() {
    for (var i=0; i<UBound; i++)
	reportCompare( true | this && this );
}
