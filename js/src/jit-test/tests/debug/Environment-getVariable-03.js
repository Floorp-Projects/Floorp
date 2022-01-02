// getVariable sees bindings in let-block scopes.

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
var log = '';
dbg.onDebuggerStatement = function (frame) {
    log += frame.environment.getVariable("x");
};
g.eval("function f() {\n" +
       "    let x = 'a';\n" +
       "    debugger;\n" +
       "    for (let x = 0; x < 2; x++)\n" + 
       "        if (x === 0)\n" +
       "            debugger;\n" +
       "        else {\n" +
       "            let x = 'b'; debugger;\n" +
       "        }\n" +
       "}\n");
g.f();
g.eval("{ let x = 'd'; debugger; }");
assertEq(log, 'a0bd');
