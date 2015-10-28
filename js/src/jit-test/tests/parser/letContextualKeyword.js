function expectError(str) {
  var log = "";
  try {
    eval(str);
  } catch (e) {
    log += "e";
    assertEq(e instanceof SyntaxError, true);
  }
  assertEq(log, "e");
}

eval(`let x = 42; assertEq(x, 42);`);
eval(`var let = 42; assertEq(let, 42);`);
eval(`let;`);
eval(`[...let] = [];`);
eval(`function let() { return 42; } assertEq(let(), 42);`)
eval(`let {x:x} = {x:42}; assertEq(x, 42);`);
eval(`let [x] = [42]; assertEq(x, 42);`);
eval(`for (let x in [1]) { assertEq(x, "0"); }`);
eval(`for (let x of [1]) { assertEq(x, 1); }`);
eval(`for (let i = 0; i < 1; i++) { assertEq(i, 0); }`);
eval(`for (let in [1]) { assertEq(let, "0"); }`);
eval(`for (let of of [1]) { assertEq(of, 1); }`);
eval(`for (let/1;;) { break; }`);
expectError(`for (let of [1]) { }`);
expectError(`let let = 42;`);
expectError(`"use strict"; var let = 42;`);
expectError(`"use strict"; function let() {}`);
expectError(`"use strict"; for (let of [1]) {}`);
