// |jit-test| slow;

if (!('oomTest' in this))
  quit();

loadFile(`
  var o = {}
  var global = this;
  var p = new Proxy(o, {
    "deleteProperty": function (await , key) {
      var g = newGlobal({sameZoneAs: this});
      g.parent = global;
      g.eval("var dbg = new Debugger(parent); dbg.onEnterFrame = function(frame) {};");
    }
  })
  for (var i=0; i<100; i++);
  assertEq(delete p.foo, true);
`);
function loadFile(lfVarx) {
    var k = 0;
    oomTest(function() {
        // In practice a crash occurs before iteration 4000.
        if (k++ > 4000)
          quit();
        eval(lfVarx);
    })
}

