var g = newGlobal();
var dbg = new Debugger(g);

g.eval("" + function f() {
  throw 42;
});

g.eval("" + function g() {
  throw new Error("42");
});

// Call the functions once. This will compile them in Ion under --ion-eager.
g.eval("try { f(); } catch (e) { }");
g.eval("try { g(); } catch (e) { }");

// Now set an onExceptionUnwind hook so that the Ion-compiled functions will
// try to bail out. The tail of the bytecode for f and g looks like 'throw;
// retrval', with 'retrval' being unreachable. Since 'throw' is resumeAfter,
// bailing out for debug mode will attempt to resume at 'retrval'. Test that
// this case is handled.
dbg.onExceptionUnwind = function f() { };
g.eval("try { f(); } catch (e) { }");
g.eval("try { g(); } catch (e) { }");
