let state = "init";

let m = registerModule('m', parseModule(
  `import "l"; import("l").then(() => { state = "complete" });`));

let l = registerModule('l', parseModule(
  `await(0);`));

moduleLink(m);
moduleEvaluate(m);
drainJobQueue();
assertEq(state, "complete");
