let module1 = registerModule('module1', parseModule(
  `import {} from "module2";
   import {} from "module3";`));

let module2 = registerModule('module2', parseModule(
  `await 1;`));

let module3 = registerModule('module3', parseModule(
  `throw 1;`));

moduleLink(module1);
moduleEvaluate(module1).catch(() => 0);
drainJobQueue();
