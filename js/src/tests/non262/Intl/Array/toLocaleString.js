if (typeof Intl === "object") {
    const localeSep = [,,].toLocaleString();

    // Missing arguments are passed as |undefined|.
    const objNoArgs = {
        toLocaleString() {
            assertEq(arguments.length, 2);
            assertEq(arguments[0], undefined);
            assertEq(arguments[1], undefined);
            return "pass";
        }
    };
    // - Single element case.
    assertEq([objNoArgs].toLocaleString(), "pass");
    // - More than one element.
    assertEq([objNoArgs, objNoArgs].toLocaleString(), "pass" + localeSep + "pass");

    // Ensure "locales" and "options" arguments are passed to the array elements.
    const locales = {}, options = {};
    const objWithArgs = {
        toLocaleString() {
            assertEq(arguments.length, 2);
            assertEq(arguments[0], locales);
            assertEq(arguments[1], options);
            return "pass";
        }
    };
    // - Single element case.
    assertEq([objWithArgs].toLocaleString(locales, options), "pass");
    // - More than one element.
    assertEq([objWithArgs, objWithArgs].toLocaleString(locales, options), "pass" + localeSep + "pass");
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
