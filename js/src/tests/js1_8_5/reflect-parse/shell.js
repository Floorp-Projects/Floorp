loadRelativeToScript('PatternAsserts.js');

function runtest(main) {
    try {
        main();
        if (typeof reportCompare === 'function')
            reportCompare(true, true);
    } catch (exc) {
        print(exc.stack);
        throw exc;
    }
}
