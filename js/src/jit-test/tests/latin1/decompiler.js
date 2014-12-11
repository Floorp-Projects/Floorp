// Latin1
function f(someName) {
    someName();
}
try {
    f(3);
} catch(e) {
    assertEq(e.message.contains("someName"), true);
}

// TwoByte
function g(someName\u1200) {
    someName\u1200();
}
try {
    g(3);
} catch(e) {
    // Note: string is deflated; don't check for the \u1200.
    assertEq(e.message.contains("someName"), true);
}
