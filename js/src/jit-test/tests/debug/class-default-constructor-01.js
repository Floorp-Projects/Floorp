// We should be able to retrieve the script of a class's default constructor.

var g = newGlobal({newCompartment: true});
var dbg = new Debugger;
var gDO = dbg.addDebuggee(g);

// Class definitions go in the global's lexical environment, so we can't use
// getOwnPropertyDescriptor or g.X to retrieve their constructor.
//
// Derived clasess use a different script, so check those too.
gDO.executeInGlobal(`   // 1729
  class X {};           // 1730
                        // 1731
                        // 1732
  class Y extends X {}; // 1733
`, { lineNumber: 1729 });

function check(name, text, startLine) {
  print(`checking ${name}`);
  var desc = gDO.executeInGlobal(name).return;
  assertEq(desc.class, 'Function');
  assertEq(desc.name, name);
  var script = desc.script;
  assertEq(script instanceof Debugger.Script, true,
           "default constructor's script should be available");
  assertEq(script.startLine, startLine,
           "default constructor's starting line should be set");
  var source = script.source;
  assertEq(source.text.substr(script.sourceStart, script.sourceLength), text);
}

check('X', 'class X {}', 1730);
check('Y', 'class Y extends X {}', 1733);
