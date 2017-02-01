if (helperThreadCount() == 0)
  quit();

function evalWithCacheLoadOffThread(code, ctx) {
  ctx = ctx || {};
  ctx = Object.create(ctx, {
    fileName: { value: "evalWithCacheCode.js" },
    lineNumber: { value: 0 }
  });
  code = code instanceof Object ? code : cacheEntry(code);

  var incremental = ctx.incremental || false;

  // We create a new global ...
  if (!("global" in ctx))
    ctx.global = newGlobal({ cloneSingletons: !incremental });

  var ctx_save;
  if (incremental)
    ctx_save = Object.create(ctx, {saveIncrementalBytecode: { value: true } });
  else
    ctx_save = Object.create(ctx, {saveBytecode: { value: true } });

  ctx.global.generation = 0;
  evaluate(code, ctx_save);

  offThreadDecodeScript(code, ctx);
  ctx.global.eval(`runOffThreadDecodedScript()`);
}

var test;

// Decode a constant.
test = `
  1;
`;
evalWithCacheLoadOffThread(test, {});
evalWithCacheLoadOffThread(test, { incremental: true });

// Decode object literals.
test = `
  var obj = { a: 1, b: 2 };
  obj.a++;
  assertEq(obj.a, 2);
`;
evalWithCacheLoadOffThread(test, {});
evalWithCacheLoadOffThread(test, { incremental: true });

// Decode functions.
test = `
  function g() {
    return function f() { return 1; };
  };
  assertEq(g()(), 1);
`;
evalWithCacheLoadOffThread(test, {});
evalWithCacheLoadOffThread(test, { incremental: true });


