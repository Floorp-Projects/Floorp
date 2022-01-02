// |jit-test| skip-if: typeof withSourceHook !== 'function'
// withSourceHook isn't defined if you pass the shell the --fuzzing-safe
// option. Skip this test silently, to avoid spurious failures.

/*
 * Debugger.Source.prototype.text should correctly retrieve the source for
 * code compiled with CompileOptions::LAZY_SOURCE.
 */

let g = newGlobal({newCompartment: true});
let dbg = new Debugger(g);

function test(source) {
  // To ensure that we're getting the value the source hook returns, make
  // it differ from the actual source.
  let frobbed = source.replace(/debugger/, 'reggubed');
  let log = '';

  withSourceHook(function (url) {
    log += 's';
    assertEq(url, "BanalBivalve.jsm");
    return frobbed;
  }, () => {
    dbg.onDebuggerStatement = function (frame) {
      log += 'd';
      assertEq(frame.script.source.text, frobbed);
    }

    g.evaluate(source, { fileName: "BanalBivalve.jsm",
                         sourceIsLazy: true });
  });

  assertEq(log, 'ds');
}

test("debugger; // Ignominious Iguana");
test("(function () { debugger; /* Meretricious Marmoset */})();");
test("(() => { debugger; })(); // Gaunt Gibbon");
