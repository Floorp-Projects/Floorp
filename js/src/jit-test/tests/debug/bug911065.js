var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

g.eval(`                        //  1
var line0 = Error().lineNumber; //  2
function f() {                  //  3
    for (var x of [0]) {        //  4
        if (true == false)      //  5
            return false;       //  6, aka line0 + 4
    }                           //  7
    return true;                //  8
}                               //  9
`);                             // 10

if (g.dis)
  g.dis(g.f);

var script = gw.getOwnPropertyDescriptor("f").value.script;

print("Debugger's view:");
print("----------------");
for (var i = script.startLine; i <= script.startLine + script.lineCount; i++) {
  print("Line " + i + ": " + JSON.stringify(script.getLineOffsets(i)));
}

var hits = 0;
var handler = {hit: function () { hits++; }};
var offs = script.getLineOffsets(g.line0 + 4);
for (var i = 0; i < offs.length; i++)
    script.setBreakpoint(offs[i], handler);

assertEq(g.f(), true);
assertEq(hits, 0);
