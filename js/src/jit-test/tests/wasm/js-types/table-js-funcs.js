// |jit-test| skip-if: !('Function' in WebAssembly)

function testfunc(n) {}

const table = new WebAssembly.Table({element: "anyfunc", initial: 3});
const func1 = new WebAssembly.Function({parameters: ["i32"], results: []}, testfunc);
table.set(0, func1);
const func2 = new WebAssembly.Function({parameters: ["f32"], results: []}, testfunc);
table.set(1, func2);
const func3 = new WebAssembly.Function({parameters: ["i64"], results: []}, testfunc);
table.set(2, func3);

const first = table.get(0);
assertEq(first instanceof WebAssembly.Function, true);
assertEq(first, func1);
assertEq(first.type().parameters[0], func1.type().parameters[0]);

const second = table.get(1);
assertEq(second instanceof WebAssembly.Function, true);
assertEq(second, func2);
assertEq(second.type().parameters[0], func2.type().parameters[0]);

const third = table.get(2);
assertEq(third instanceof WebAssembly.Function, true);
assertEq(third, func3);
assertEq(third.type().parameters[0], func3.type().parameters[0]);
