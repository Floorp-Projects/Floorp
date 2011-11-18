// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

if (typeof Debugger === 'function') {
    var g = newGlobal('new-compartment');
    var dbg = new Debugger(g);
    dbg.onDebuggerStatement = function (frame) { frame.eval(''); };
    var s = '{ let ';
    for (var i = 0; i < 128; i++)
        s += 'x' + i + ', ';
    s += 'X = 0; debugger; }';
    g.eval(s);
}

reportCompare(0, 0, 'ok');
