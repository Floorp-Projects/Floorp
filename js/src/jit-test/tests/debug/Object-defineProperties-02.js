// Exceptions thrown by obj.defineProperties are copied into the debugger compartment.

var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

function test(objexpr, descs) {
    var exca, excb;

    g.eval("obj = (" + objexpr + ");");
    var gobjw = gw.getOwnPropertyDescriptor("obj").value;
    try {
        gobjw.defineProperties(descs);
    } catch (exc) {
        exca = exc;
    }

    var indirectEval = eval;
    var obj = indirectEval("(" + objexpr + ");");
    try {
        Object.defineProperties(obj, descs);
    } catch (exc) {
        excb = exc;
    }

    assertEq(Object.getPrototypeOf(exca), Object.getPrototypeOf(excb));
    assertEq(exca.message, excb.message);
    assertEq(typeof exca.fileName, "string");
    assertEq(typeof exca.stack, "string");
}

test("Object.create(null, {p: {value: 1}})", {p: {value: 2}});
test("({})", {x: {get: 'bad'}});
