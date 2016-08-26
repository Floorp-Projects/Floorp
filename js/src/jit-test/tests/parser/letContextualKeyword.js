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
eval(`for (const x in [1]) { assertEq(x, "0"); }`);

eval(`for (let x of [1]) { assertEq(x, 1); }`);
eval(`for (const x of [1]) { assertEq(x, 1); }`);

eval(`for (let i = 0; i < 1; i++) { assertEq(i, 0); }`);
eval(`var done = false; for (const i = 0; !done; done = true) { assertEq(i, 0); }`);

eval(`for (let of of [1]) { assertEq(of, 1); }`);
eval(`for (const of of [1]) { assertEq(of, 1); }`);

eval(`try { throw 17; } catch (let) { assertEq(let, 17); }`);
eval(`try { throw [17]; } catch ([let]) { assertEq(let, 17); }`);
eval(`try { throw { x: 17 }; } catch ({ x: let }) { assertEq(let, 17); }`);
eval(`try { throw {}; } catch ({ x: let = 17 }) { assertEq(let, 17); }`);

expectError(`try { throw [17, 42]; } catch ([let, let]) {}`);

eval(`for (let in [1]) { assertEq(let, "0"); }`);
eval(`for (let / 1; ; ) { break; }`);
expectError(`let = {}; for (let.x of;;);`);
expectError(`for (let of [1]) { }`);

expectError(`for (let let in [1]) { }`);
expectError(`for (const let in [1]) { }`);

expectError(`for (let let of [1]) { }`);
expectError(`for (const let of [1]) { }`);

expectError(`for (let let = 17; false; ) { }`);
expectError(`for (const let = 17; false; ) { }`);

expectError(`for (let [let] = 17; false; ) { }`);
expectError(`for (const [let] = 17; false; ) { }`);

expectError(`for (let [let = 42] = 17; false; ) { }`);
expectError(`for (const [let = 42] = 17; false; ) { }`);

expectError(`for (let { x: let } = 17; false; ) { }`);
expectError(`for (const { x: let } = 17; false; ) { }`);

expectError(`for (let { x: let = 42 } = 17; false; ) { }`);
expectError(`for (const { x: let = 42 } = 17; false; ) { }`);

expectError("let\nlet;");
expectError("const\nlet;");

expectError(`let let = 17;`);
expectError(`const let = 17;`);

expectError(`let [let] = 17;`);
expectError(`const [let] = 17;`);

expectError(`let [let = 42] = 17;`);
expectError(`const [let = 42] = 17;`);

expectError(`let {let} = 17;`);
expectError(`const {let} = 17;`);

expectError(`let { let = 42 } = 17;`);
expectError(`const { let = 42 } = 17;`);

expectError(`let { x: let } = 17;`);
expectError(`const { x: let } = 17;`);

expectError(`let { x: let = 42 } = 17;`);
expectError(`const { x: let = 42 } = 17;`);

expectError(`let { ['y']: let } = 17;`);
expectError(`const { ['y']: let } = 17;`);

expectError(`let { ['y']: let = 42 } = 17;`);
expectError(`const { ['y']: let = 42 } = 17;`);

expectError(`let { x: [let] } = { x: 17 };`);
expectError(`const { x: [let] } = { x: 17 };`);

expectError(`let { x: [let = 42] } = { x: 17 };`);
expectError(`const { x: [let = 42] } = { x: 17 };`);

expectError(`let [foo, let] = 42;`);
expectError(`const [foo, let] = 42;`);

expectError(`let [foo, { let }] = [17, {}];`);
expectError(`const [foo, { let }] = [17, {}];`);

expectError(`"use strict"; var let = 42;`);
expectError(`"use strict"; function let() {}`);
expectError(`"use strict"; for (let of [1]) {}`);
expectError(`"use strict"; try {} catch (let) {}`);
