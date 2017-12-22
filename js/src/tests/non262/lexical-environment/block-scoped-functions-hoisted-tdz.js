var log = "";
try {
  (function() {
    {
      let y = f();
      function f() { y; }
    }
  })()
} catch (e) {
  log += e instanceof ReferenceError;
}

try {
  function f() {
    switch (1) {
      case 0:
        let x;
      case 1:
        (function() { x; })();
    }
  }
  f();
} catch (e) {
  log += e instanceof ReferenceError;
}

assertEq(log, "truetrue");

if ("reportCompare" in this)
  reportCompare(true, true);
