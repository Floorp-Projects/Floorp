// |jit-test| --no-source-pragmas

const stack = evaluate(`
  new Error('test').stack;
  //# sourceURL=file.js
`, { fileName: "test.js"});

assertEq(stack.split("\n")[0], "@test.js:2:3");
