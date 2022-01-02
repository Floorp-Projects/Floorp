function toPrinted(value) {}
function reportCompare (expected, actual, description) {
    if (expected != actual)
        + toPrinted(actual)
}
test();
function test() {
    reportCompare();
    try {
        test();
    } catch (e) {
	try {
            new test();
	} catch(e) {}
    }
    reportCompare();
}
