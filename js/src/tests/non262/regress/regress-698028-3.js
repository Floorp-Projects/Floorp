// |reftest| skip-if(!xulRuntime.shell) -- needs Debugger

// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);
dbg.onDebuggerStatement = function (frame) { frame.eval(''); };
var s = '{ let ';
for (var i = 0; i < 128; i++)
    s += 'x' + i + ', ';
s += 'X = 0; debugger; }';
g.eval(s);

reportCompare(0, 0, 'ok');
