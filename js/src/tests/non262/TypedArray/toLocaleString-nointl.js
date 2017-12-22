if (typeof Intl !== "object") {
    const localeSep = [,,].toLocaleString();

    const originalNumberToLocaleString = Number.prototype.toLocaleString;

    // Ensure no arguments are passed to the array elements.
    for (let constructor of anyTypedArrayConstructors) {
        Number.prototype.toLocaleString = function() {
            assertEq(arguments.length, 0);
            return "pass";
        };

        // Single element case.
        assertEq(new constructor(1).toLocaleString(), "pass");

        // More than one element.
        assertEq(new constructor(2).toLocaleString(), "pass" + localeSep + "pass");
    }
    Number.prototype.toLocaleString = originalNumberToLocaleString;

    // Ensure no arguments are passed to the array elements even if supplied.
    for (let constructor of anyTypedArrayConstructors) {
        Number.prototype.toLocaleString = function() {
            assertEq(arguments.length, 0);
            return "pass";
        };
        let locales = {};
        let options = {};

        // Single element case.
        assertEq(new constructor(1).toLocaleString(locales, options), "pass");

        // More than one element.
        assertEq(new constructor(2).toLocaleString(locales, options), "pass" + localeSep + "pass");
    }
    Number.prototype.toLocaleString = originalNumberToLocaleString;
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
