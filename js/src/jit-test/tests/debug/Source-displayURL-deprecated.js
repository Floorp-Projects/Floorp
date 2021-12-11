/* -*- js-indent-level: 4; indent-tabs-mode: nil -*- */
// Source.prototype.displayURL can be a string or null.

let g = newGlobal({newCompartment: true});
let dbg = new Debugger;
let gw = dbg.addDebuggee(g);

function getDisplayURL() {
    let fw = gw.makeDebuggeeValue(g.f);
    return fw.script.source.displayURL;
}

// Comment pragmas
g.evaluate('function f() {}\n' +
           '//@ sourceURL=file:///var/quux.js');
assertEq(getDisplayURL(), 'file:///var/quux.js');

g.evaluate('function f() {}\n' +
           '/*//@ sourceURL=file:///var/quux.js*/');
assertEq(getDisplayURL(), 'file:///var/quux.js');

g.evaluate('function f() {}\n' +
           '/*\n' +
           '//@ sourceURL=file:///var/quux.js\n' +
           '*/');
assertEq(getDisplayURL(), 'file:///var/quux.js');
