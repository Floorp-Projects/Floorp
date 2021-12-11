// Test using an empty string as a module specifier fails.
let result = null;
let error = null;
let promise = import("");
promise.then((ns) => {
    result = ns;
}).catch((e) => {
    error = e;
});

drainJobQueue();
assertEq(result, null);
assertEq(error instanceof Error, true);

// Test reading a directory as a file fails.
result = null;
error = null;
try {
    result = os.file.readFile(".");
} catch (e) {
    error = e;
}

assertEq(result, null);
assertEq(error instanceof Error, true);
