if (!wasmGcEnabled()) {
    quit();
}

wasmEvalText(`
    (module
        (import "global" "func" (result i32))
        (func (export "func_0") (result i32)
         call 0 ;; calls the import, which is func #0
        )
    )
`, {
    global: {
        func() {
          var g = newGlobal();
          var dbg = new Debugger(g);
          var caught = false;
          try {
            dbg.findScripts().filter(isWasm(g, isValidWasmURL, 2));
          } catch(e) {
              caught = true;
              assertEq(/temporarily unavailable/.test(e.toString()), true);
          }
          // When this assertion fails, it's time to remove the restriction in
          // Debugger.findScripts.
          assertEq(caught, true);
        }
    }
}).exports.func_0();;
