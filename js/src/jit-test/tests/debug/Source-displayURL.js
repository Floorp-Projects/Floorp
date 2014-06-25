/* -*- js-indent-level: 4; indent-tabs-mode: nil -*- */
// Source.prototype.displayURL can be a string or null.

let g = newGlobal('new-compartment');
let dbg = new Debugger;
let gw = dbg.addDebuggee(g);

function getDisplayURL() {
    let fw = gw.makeDebuggeeValue(g.f);
    return fw.script.source.displayURL;
}

// Without a source url
g.evaluate("function f(x) { return 2*x; }");
assertEq(getDisplayURL(), null);

// With a source url
g.evaluate("function f(x) { return 2*x; }", {displayURL: 'file:///var/foo.js'});
assertEq(getDisplayURL(), 'file:///var/foo.js');

// Nested functions
let fired = false;
dbg.onDebuggerStatement = function (frame) {
    fired = true;
    assertEq(frame.script.source.displayURL, 'file:///var/bar.js');
};
g.evaluate('(function () { (function () { debugger; })(); })();',
           {displayURL: 'file:///var/bar.js'});
assertEq(fired, true);

// Comment pragmas
g.evaluate('function f() {}\n' +
           '//# sourceURL=file:///var/quux.js');
assertEq(getDisplayURL(), 'file:///var/quux.js');

g.evaluate('function f() {}\n' +
           '/*//# sourceURL=file:///var/quux.js*/');
assertEq(getDisplayURL(), 'file:///var/quux.js');

g.evaluate('function f() {}\n' +
           '/*\n' +
           '//# sourceURL=file:///var/quux.js\n' +
           '*/');
assertEq(getDisplayURL(), 'file:///var/quux.js');

// Spaces are disallowed by the URL spec (they should have been
// percent-encoded).
g.evaluate('function f() {}\n' +
           '//# sourceURL=http://example.com/has illegal spaces');
assertEq(getDisplayURL(), 'http://example.com/has');

// When the URL is missing, we don't set the sourceMapURL and we don't skip the
// next line of input.
g.evaluate('function f() {}\n' +
           '//# sourceURL=\n' +
           'function z() {}');
assertEq(getDisplayURL(), null);
assertEq('z' in g, true);

// The last comment pragma we see should be the one which sets the displayURL.
g.evaluate('function f() {}\n' +
           '//# sourceURL=http://example.com/foo.js\n' +
           '//# sourceURL=http://example.com/bar.js');
assertEq(getDisplayURL(), 'http://example.com/bar.js');

// With both a comment and the evaluate option.
g.evaluate('function f() {}\n' +
           '//# sourceURL=http://example.com/foo.js',
           {displayURL: 'http://example.com/bar.js'});
assertEq(getDisplayURL(), 'http://example.com/foo.js');

