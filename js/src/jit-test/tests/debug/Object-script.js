var g = newGlobal();
var dbg = new Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    var arr = frame.arguments;
    assertEq(arr[0].script instanceof Debugger.Script, true);
    assertEq(arr[1].script instanceof Debugger.Script, true);
    assertEq(arr[2].script instanceof Debugger.Script, true);
    assertEq(arr[3].script instanceof Debugger.Script, true);
    assertEq(arr[4].script, undefined);
    assertEq(arr[5].script, undefined);
    assertEq(arr.length, 6);
    hits++;
};

g.eval(`
    function f() { debugger; }
    f(function g(){},
      function* h() {},
      async function j() {},
      async function* k() {},
      {},
      Math.atan2);
`);
assertEq(hits, 1);
