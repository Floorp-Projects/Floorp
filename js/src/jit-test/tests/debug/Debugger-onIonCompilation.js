
function test() {
    // Force Ion compilation with OSR.
    for (var res = false; !res; res = inIon()) {};
    if (typeof res == "string")
        throw "Skipping test: Ion compilation is disabled/prevented.";
};


// Functions used to assert the representation of the graph which is exported in
// the JSON string argument.
function assertInstruction(ins) {
    assertEq(typeof ins.id, "number");
    assertEq(ins.id | 0, ins.id);
    assertEq(typeof ins.opcode, "string");
}

function assertBlock(block) {
    assertEq(typeof block, "object");
    assertEq(typeof block.number, "number");
    assertEq(block.number | 0, block.number);
    assertEq(typeof block.instructions, "object");
    for (var ins of block.instructions)
        assertInstruction(ins);
}

function assertGraph(graph, scripts) {
    assertEq(typeof graph, "object");
    assertEq(typeof graph.blocks, "object");
    assertEq(graph.blocks.length, scripts.length);
    for (var block of graph.blocks)
        assertBlock(block);
}

function assertJSON(str, scripts) {
    assertEq(typeof str, "string");

    var json = JSON.parse(str);
    assertGraph(json.mir, scripts);
    assertGraph(json.lir, scripts);
}

function assertOnIonCompilationArgument(obj) {
    assertEq(typeof obj, "object");
    assertEq(typeof obj.scripts, "object");
    assertJSON(obj.json, obj.scripts);
}

// Attach the current global to a debugger.
var hits = 0;
var g = newGlobal();
g.parent = this;
g.eval(`
  var dbg = new Debugger();
  var parentw = dbg.addDebuggee(parent);
  var testw = parentw.makeDebuggeeValue(parent.test);
  var scriptw = testw.script;
`);

// Wrap the testing function.
function check(assert) {
  // print('reset compilation counter.');
  with ({}) {       // Prevent Ion compilation.
    gc();           // Flush previous compilation.
    hits = 0;       // Synchronized hit counts.

    try {           // Skip this test if we cannot reliably compile with Ion.
        test();         // Wait until the next Ion compilation.
    } catch (msg) {
        if (typeof msg == "string") {
            // print(msg);
            return;
        }
    }

    assert();       // Run the assertions given as arguments.
  }
}

// With the compilation graph inhibited, we should have no output.
g.eval(`
  dbg.onIonCompilation = function (graph) {
    // print('Compiled ' + graph.scripts[0].displayName + ':' + graph.scripts[0].startLine);
    if (graph.scripts[0] !== scriptw)
      return;
    parent.assertOnIonCompilationArgument(graph);
    parent.hits++;
  };
`);

check(function () {
    // '>= 1' is needed because --ion-eager is too eager.
    assertEq(hits >= 1, true);
});


// Try re-entering the same compartment as the compiled script.
g.dbg.onIonCompilation = function (graph) {
  // print('Compiled ' + graph.scripts[0].displayName + ':' + graph.scripts[0].startLine);
  if (graph.scripts[0] !== g.scriptw)
    return;
  assertOnIonCompilationArgument(graph);
  hits++;
};
check(function () { assertEq(hits >= 1, true); });

// Disable the debugger, and redo the last 2 tests.
g.eval(`
  dbg.enabled = false;
  dbg.onIonCompilation = function (graph) {
    parent.hits++;
  };
`);
check(function () { assertEq(hits, 0); });


g.dbg.enabled = false;
g.dbg.onIonCompilation = function (graph) {
  hits++;
};
check(function () { assertEq(hits, 0); });
