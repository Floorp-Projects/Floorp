function TestCase(n, d) {}
function reportCompare() {
    new TestCase;
}
Object.defineProperty(Object.prototype, "name", {});
reportCompare();
try {
    function TestCase( n, d ) {
	this.name = n;
	this.description = d;
    }
    reportCompare();
    reportCompare();
} catch(exc3) { assertEq(0, 1); }
