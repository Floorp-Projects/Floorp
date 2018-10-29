// Smoke test for the PC count profiler. 

function f(x) {
    if (x < 10)
        return 0;
    if (x < 50)
        return 1;
    return 2;
}

L: try {
    // Profile the test function "f".
    pccount.start();
    for (var i = 0; i < 100; ++i) f(i);
    pccount.stop();

    // Count the profiled scripts, stop if no scripts were profiled.
    var n = pccount.count();
    if (n === 0)
        break L;

    // Find the script index for "f".
    var scriptIndex = -1;
    for (var i = 0; i < n; ++i) {
        var summary = JSON.parse(pccount.summary(i));
        if (summary.name === "f")
            scriptIndex = i;
    }

    // Stop if the script index for "f" wasn't found.
    if (scriptIndex < 0)
        break L;

    // Retrieve the profiler data for "f".
    var contents = pccount.contents(scriptIndex);
    assertEq(typeof contents, "string");

    // The profiler data should be parsable as JSON text.
    var contents = JSON.parse(contents, (name, value) => {
        // Split the Ion code segments into multiple entries.
        if (name === "code")
            return value.split("\n");

        return value;
    });

    // Pretty print the profiler data.
    var pretty = JSON.stringify(contents, null, 1);
    print(pretty);
} finally {
    // Clean-up.
    pccount.purge();
}
