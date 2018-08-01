/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test can be really slow on debug platforms and should be split.
requestLongerTimeout(20);

// Tests loading sourcemapped sources for Babel's compile output.

async function breakpointScopes(dbg, target, fixture, { line, column }, scopes) {
  const filename = `${target}://./${fixture}/input.`;
  const fnName = pairToFnName(target, fixture);

  await invokeWithBreakpoint(
    dbg,
    fnName,
    filename,
    { line, column },
    async () => {
      await assertScopes(dbg, scopes);
    }
  );

  ok(true, `Ran tests for ${fixture} at line ${line} column ${column}`);
}

add_task(async function() {
  await pushPref("devtools.debugger.features.map-scopes", true);

  const dbg = await initDebugger("doc-sourcemapped.html");

  await testBabelBindingsWithFlow(dbg);
  await testBabelFlowtypeBindings(dbg);
  await testEvalMaps(dbg);
  await testForOf(dbg);
  await testShadowedVars(dbg);
  await testLineStartBindingsES6(dbg);
  await testThisArgumentsBindings(dbg);
  await testClasses(dbg);
  await testForLoops(dbg);
  await testFunctions(dbg);
  await testSwitches(dbg);
  await testTryCatches(dbg);
  await testLexAndNonlex(dbg);
  await testTypescriptClasses(dbg);
  await testTypeModule(dbg);
  await testTypeScriptCJS(dbg);
  await testOutOfOrderDeclarationsCJS(dbg);
  await testCommonJS(dbg);
  await testWebpackLineMappings(dbg);
  await testWebpackFunctions(dbg);
  await testESModules(dbg);
  await testESModulesCJS(dbg);
  await testESModulesES6(dbg);
});

function targetToFlags(target) {
  const isRollup = target.startsWith("rollup");
  const isWebpack = target.startsWith("webpack");
  const hasBabel = target.includes("-babel");

  // Rollup removes lots of things as dead code, so they are marked as optimized out.
  const rollupOptimized = isRollup ? "(optimized away)" : null;
  const webpackImportGetter = isWebpack ? "Getter" : null;
  const maybeLineStart = hasBabel ? col => col : col => 0;

  return { isRollup, isWebpack, rollupOptimized, webpackImportGetter, maybeLineStart };
}
function pairToFnName(target, fixture) {
  return (target + "-" + fixture).replace(/-([a-z])/g, (s, c) => c.toUpperCase());
}

function webpackModule(target, fixture, optimizedOut) {
  return [
    pairToFnName(target, fixture),
    ["__webpack_exports__", optimizedOut ? "(optimized away)" : "{\u2026}"],
    optimizedOut ? ["__webpack_require__", "(optimized away)"] : "__webpack_require__()",
    ["arguments", optimizedOut ? "(unavailable)" : "Arguments"],
  ];
}

async function testBabelBindingsWithFlow(dbg) {
  // Flow is not available on the non-babel builds.
  for (const target of [
    "rollup-babel6",
    "webpack3-babel6",
  ]) {
    const { webpackImportGetter } = targetToFlags(target);

    await breakpointScopes(dbg, target, "babel-bindings-with-flow", { line: 9, column: 2 }, [
      "root",
      ["value", '"a-named"'],
      "Module",
      ["aNamed", webpackImportGetter || '"a-named"'],
      "root()",
    ]);
  }
}

async function testBabelFlowtypeBindings(dbg) {
  // Flow is not available on the non-babel builds.
  for (const target of [
    "rollup-babel6",
    "webpack3-babel6",
  ]) {
    const { webpackImportGetter } = targetToFlags(target);

    await breakpointScopes(
      dbg,
      target,
      "babel-flowtype-bindings",
      { line: 9, column: 2 },
      [
        "Module",
        ["aConst", '"a-const"'],
        ["Four", webpackImportGetter || '"one"'],
        "root()"
      ]
    );
  }
}

async function testEvalMaps(dbg) {
  // At times, this test has been a bit flakey due to the inlined source map
  // never loading. I'm not sure what causes that. If we observe flakiness in CI,
  // we should consider disabling this test for now.

  await breakpointScopes(dbg, "webpack3", "eval-maps", { line: 14, column: 0 }, [
    "Block",
    ["<this>", "Window"],
    ["three", "5"],
    ["two", "4"],
    "Block",
    ["three", "3"],
    ["two", "2"],
    "Block",
    ["arguments", "Arguments"],
    ["one", "1"],
    ...webpackModule("webpack3", "eval-maps", true /* optimized out */),
    ["module", "(optimized away)"],
    "root",
  ]);

  for (const target of [
    "rollup",
    "rollup-babel6",
    "webpack3-babel6",
  ]) {
    const { rollupOptimized, webpackImportGetter, maybeLineStart } = targetToFlags(target);

    await breakpointScopes(dbg, target, "eval-maps", { line: 14, column: maybeLineStart(4) }, [
      "Block",
      ["three", "5"],
      ["two", "4"],
      "Function Body",
      ["three", rollupOptimized || "3"],
      ["two", rollupOptimized || "2"],
      "root",
      ["one", "1"],
      "Module",
      "root",
    ]);
  }
}

async function testForOf(dbg) {
  await breakpointScopes(dbg, "webpack3", "for-of", { line: 5, column: 0 }, [
    "Block",
    ["<this>", "Window"],
    ["x", "1"],
    "Block",
    ["arguments", "Arguments"],
    "doThing()",
    "Block",
    "mod",
    ...webpackModule("webpack3", "for-of", true /* optimizedOut */),
    "forOf",
    "module"
  ]);

  for (const target of [
    "rollup",
    "rollup-babel6",
    "webpack3-babel6",
  ]) {
    const { rollupOptimized, webpackImportGetter, maybeLineStart } = targetToFlags(target);

    await breakpointScopes(dbg, target, "for-of", { line: 5, column: maybeLineStart(4) }, [
      "For",
      ["x", "1"],
      "forOf",
      "doThing()",
      "Module",
      "forOf",
      "mod"
    ]);
  }
}

async function testShadowedVars(dbg) {
  await breakpointScopes(dbg, "webpack3", "shadowed-vars", { line: 18, column: 0 }, [
    "Block",
    ["<this>", "Window"],
    ["aConst", '"const3"'],
    ["aLet", '"let3"'],

    "Block",
    ["aConst", '"const2"'],
    ["aLet", '"let2"'],
    "Outer()",

    "Block",
    ["aConst", '"const1"'],
    ["aLet", '"let1"'],
    "Outer()",

    "Block",
    ["arguments", "Arguments"],
    ["aVar", '"var3"'],

    ...webpackModule("webpack3", "shadowed-vars", true /* optimizedOut */),
    ["module", "(optimized away)"],
  ]);

  for (const target of [
    "rollup",
    "rollup-babel6",
    "webpack3-babel6",
  ]) {
    const { rollupOptimized, webpackImportGetter, maybeLineStart } = targetToFlags(target);

    await breakpointScopes(dbg, target, "shadowed-vars", { line: 18, column: maybeLineStart(6) }, [
      "Block",
      ["aConst", rollupOptimized || '"const3"'],
      ["aLet", rollupOptimized || '"let3"'],
      "Block",
      ["aConst", rollupOptimized || '"const2"'],
      ["aLet", rollupOptimized || '"let2"'],
      rollupOptimized ? ["Outer", rollupOptimized ] : "Outer:_Outer()",
      "Function Body",
      ["aConst", rollupOptimized || '"const1"'],
      ["aLet", rollupOptimized || '"let1"'],
      rollupOptimized ? ["Outer", rollupOptimized ] : "Outer()",
      "default",
      ["aVar", rollupOptimized || '"var3"']
    ]);
  }
}

async function testLineStartBindingsES6(dbg) {
  await breakpointScopes(
    dbg,
    "webpack3",
    "line-start-bindings-es6",
    { line: 19, column: 0 },
    [
      "Block",
      ["<this>", "{\u2026}"],
      ["one", "1"],
      ["two", "2"],
      "Block",
      ["arguments", "Arguments"],

      "Block",
      ["aFunc", "(optimized away)"],
      ["arguments", "(unavailable)"],

      ...webpackModule("webpack3", "line-start-bindings-es6", true /* optimizedOut */),
      ["module", "(optimized away)"],
      "root()"
    ]
  );

  for (const target of [
    "rollup",
    "rollup-babel6",
    "webpack3-babel6",
  ]) {
    const { rollupOptimized, webpackImportGetter, maybeLineStart } = targetToFlags(target);

    await breakpointScopes(
      dbg,
      target,
      "line-start-bindings-es6",
      { line: 19, column: maybeLineStart(4) },
      [
        "Function Body",
        ["<this>", "{\u2026}"],
        ["one", rollupOptimized || "1"],
        ["two", rollupOptimized || "2"],
        "root",
        ["aFunc", "(optimized away)"],
        "Module",
        "root()"
      ]
    );
  }
}

async function testThisArgumentsBindings(dbg) {
  await breakpointScopes(
    dbg,
    "webpack3",
    "this-arguments-bindings",
    { line: 4, column: 0 },
    [
      "Block",
      ["<this>", '"this-value"'],
      ["arrow", "(uninitialized)"],
      "fn",
      ["arg", '"arg-value"'],
      ["arguments", "Arguments"],
      "root",
      ["arguments", "Arguments"],
      "fn()",
      ...webpackModule("webpack3", "this-arguments-bindings", true /* optimizedOut */),
      ["module", "(optimized away)"],
      "root()"
    ]
  );

  await breakpointScopes(
    dbg,
    "webpack3",
    "this-arguments-bindings",
    { line: 8, column: 0 },
    [
      "Block",
      ["<this>", '"this-value"'],
      ["argArrow", '"arrow-arg"'],
      ["arguments", "Arguments"],
      "Block",
      ["arrow", "(optimized away)"],
      "fn",
      ["arg", '"arg-value"'],
      ["arguments", "Arguments"],
      "root",
      ["arguments", "Arguments"],
      "fn()",
      ...webpackModule("webpack3", "this-arguments-bindings", true /* optimizedOut */),
      ["module", "(optimized away)"],
      "root()"
    ]
  );

  for (const target of [
    "rollup",
    "rollup-babel6",
    "webpack3-babel6",
  ]) {
    const { rollupOptimized, webpackImportGetter, maybeLineStart } = targetToFlags(target);

    await breakpointScopes(
      dbg,
      target,
      "this-arguments-bindings",
      { line: 4, column: maybeLineStart(4) },
      [
        "Function Body",
        ["<this>", '"this-value"'],
        ["arrow", target === "rollup" ? "(uninitialized)" : "undefined"],
        "fn",
        ["arg", '"arg-value"'],
        ["arguments", "Arguments"],
        "root",
        "fn()",
        "Module",
        "root()"
      ]
    );

    await breakpointScopes(
      dbg,
      target,
      "this-arguments-bindings",
      { line: 8, column: maybeLineStart(6) },
      [
        "arrow",
        ["<this>", '"this-value"'],
        ["argArrow", '"arrow-arg"'],
        "Function Body",
        target === "rollup" ? ["arrow", "(optimized away)"] : "arrow()",
        "fn",
        ["arg", '"arg-value"'],
        ["arguments", "Arguments"],
        "root",
        "fn()",
        "Module",
        "root()"
      ]
    );
  }
}

async function testClasses(dbg) {
  await breakpointScopes(dbg, "webpack3", "classes", { line: 6, column: 0 }, [
    "Block",
    ["<this>", "{}"],
    ["arguments", "Arguments"],
    "Block",
    ["Thing", "(optimized away)"],
    "Block",
    "Another()",
    ["one", "1"],
    "Thing()",
    "Block",
    ["arguments", "(unavailable)"],
    ...webpackModule("webpack3", "classes", true /* optimizedOut */),
    ["module", "(optimized away)"],
    "root()"
  ]);

  await breakpointScopes(dbg, "webpack3", "classes", { line: 16, column: 0 }, [
    "Block",
    ["<this>", "{}"],
    ["three", "3"],
    ["two", "2"],
    "Block",
    ["arguments", "Arguments"],
    "Block",
    "Another()",
    "Block",
    "Another()",
    ["one", "1"],
    "Thing()",
    "Block",
    ["arguments", "(unavailable)"],
    ...webpackModule("webpack3", "classes", true /* optimizedOut */),
    ["module", "(optimized away)"],
    "root()"
  ]);

  for (const target of [
    "rollup",
    "rollup-babel6",
    "webpack3-babel6",
  ]) {
    const { rollupOptimized, webpackImportGetter, maybeLineStart } = targetToFlags(target);

    await breakpointScopes(dbg, target, "classes", { line: 6, column: maybeLineStart(6) }, [
      "Class",
      target === "rollup" ? ["Thing", "(optimized away)"] : "Thing()",
      "Function Body",
      "Another()",
      "one",
      target === "rollup" ? ["Thing", "(optimized away)"] : "Thing()",
      "Module",
      "root()"
    ]);

    await breakpointScopes(dbg, target, "classes", { line: 16, column: maybeLineStart(6) }, [
      "Function Body",
      ["three", rollupOptimized || "3"],
      ["two", rollupOptimized || "2"],
      "Class",
      "Another()",
      "Function Body",
      "Another()",
      ["one", "1"],
      "Thing()",
      "Module",
      "root()"
    ]);
  }
}

async function testForLoops(dbg) {
  await breakpointScopes(dbg, "webpack3", "for-loops", { line: 5, column: 0 }, [
    "Block",
    ["<this>", "Window"],
    ["i", "1"],
    "Block",
    ["i", "0"],
    "Block",
    ["arguments", "Arguments"],
    ...webpackModule("webpack3", "for-loops", true /* optimizedOut */),
    ["module", "(optimized away)"],
    "root()"
  ]);
  await breakpointScopes(dbg, "webpack3", "for-loops", { line: 9, column: 0 }, [
    "Block",
    ["<this>", "Window"],
    ["i", '"2"'],
    "Block",
    ["i", "0"],
    "Block",
    ["arguments", "Arguments"],
    ...webpackModule("webpack3", "for-loops", true /* optimizedOut */),
    ["module", "(optimized away)"],
    "root()"
  ]);
  await breakpointScopes(dbg, "webpack3", "for-loops", { line: 13, column: 0 }, [
    "Block",
    ["<this>", "Window"],
    ["i", "3"],
    "Block",
    ["i", "0"],
    "Block",
    ["arguments", "Arguments"],
    ...webpackModule("webpack3", "for-loops", true /* optimizedOut */),
    ["module", "(optimized away)"],
    "root()"
  ]);

  for (const target of [
    "rollup",
    "rollup-babel6",
    "webpack3-babel6",
  ]) {
    const { rollupOptimized, webpackImportGetter, maybeLineStart } = targetToFlags(target);

    await breakpointScopes(dbg, target, "for-loops", { line: 5, column: maybeLineStart(4) }, [
      "For",
      ["i", "1"],
      "Function Body",
      ["i", rollupOptimized || "0"],
      "Module",
      "root()"
    ]);
    await breakpointScopes(dbg, target, "for-loops", { line: 9, column: maybeLineStart(4) }, [
      "For",
      ["i", '"2"'],
      "Function Body",
      ["i", rollupOptimized || "0"],
      "Module",
      "root()"
    ]);
    await breakpointScopes(dbg, target, "for-loops", { line: 13, column: maybeLineStart(4) }, [
      "For",
      ["i", target === "rollup" ? "3" : rollupOptimized || "3"],
      "Function Body",
      ["i", rollupOptimized || "0"],
      "Module",
      "root()"
    ]);
  }
}

async function testFunctions(dbg) {
  await breakpointScopes(dbg, "webpack3", "functions", { line: 6, column: 0 }, [
    "Block",
    ["<this>", "(optimized away)"],
    ["arguments", "Arguments"],
    ["p3", "undefined"],
    "Block",
    "arrow()",
    "inner",
    ["arguments", "Arguments"],
    ["p2", "undefined"],
    "Block",
    "inner()",
    "Block",
    ["inner", "(optimized away)"],
    "decl",
    ["arguments", "Arguments"],
    ["p1", "undefined"],
    "root",
    ["arguments", "Arguments"],
    "decl()",
    ...webpackModule("webpack3", "functions", true /* optimizedOut */),
    ["module", "(optimized away)"],
    "root()"
  ]);

  for (const target of [
    "rollup",
    "rollup-babel6",
    "webpack3-babel6",
  ]) {
    const { rollupOptimized, webpackImportGetter, maybeLineStart } = targetToFlags(target);

    await breakpointScopes(dbg, target, "functions", { line: 6, column: maybeLineStart(8) }, [
      "arrow",
      ["p3", "undefined"],
      "Function Body",
      "arrow()",
      "inner",
      ["p2", "undefined"],
      "Function Expression",
      "inner()",
      "Function Body",
      target === "rollup" ? ["inner", "(optimized away)"] : "inner()",
      "decl",
      ["p1", "undefined"],
      "root",
      "decl()",
      "Module",
      "root()"
    ]);
  }
}

async function testSwitches(dbg) {
  await breakpointScopes(dbg, "webpack3", "switches", { line: 7, column: 0 }, [
    "Block",
    ["<this>", "Window"],
    ["val", "2"],
    "Block",
    ["val", "1"],
    "Block",
    ["arguments", "Arguments"],
    ...webpackModule("webpack3", "switches", true /* optimizedOut */),
    ["module", "(optimized away)"],
    "root()"
  ]);

  await breakpointScopes(dbg, "webpack3", "switches", { line: 10, column: 0 }, [
    "Block",
    ["<this>", "Window"],
    ["val", "3"],
    "Block",
    ["val", "2"],
    "Block",
    ["val", "1"],
    "Block",
    ["arguments", "Arguments"],
    ...webpackModule("webpack3", "switches", true /* optimizedOut */),
    ["module", "(optimized away)"],
    "root()"
  ]);

  for (const target of [
    "rollup",
    "rollup-babel6",
    "webpack3-babel6",
  ]) {
    const { rollupOptimized, webpackImportGetter, maybeLineStart } = targetToFlags(target);

    await breakpointScopes(dbg, target, "switches", { line: 7, column: maybeLineStart(6) }, [
      "Switch",
      ["val", rollupOptimized || "2"],
      "Function Body",
      ["val", rollupOptimized || "1"],
      "Module",
      "root()"
    ]);

    await breakpointScopes(dbg, target, "switches", { line: 10, column: maybeLineStart(6) }, [
      "Block",
      ["val", rollupOptimized || "3"],
      "Switch",
      ["val", rollupOptimized || "2"],
      "Function Body",
      ["val", rollupOptimized || "1"],
      "Module",
      "root()"
    ]);
  }
}

async function testTryCatches(dbg) {
  await breakpointScopes(dbg, "webpack3", "try-catches", { line: 8, column: 0 }, [
    "Block",
    ["<this>", "Window"],
    ["two", "2"],
    "Block",
    ["err", '"AnError"'],
    "Block",
    ["one", "1"],
    "Block",
    ["arguments", "Arguments"],
    ...webpackModule("webpack3", "try-catches", true /* optimizedOut */),
    ["module", "(optimized away)"],
    "root()"
  ]);

  for (const target of [
    "rollup",
    "rollup-babel6",
    "webpack3-babel6",
  ]) {
    const { rollupOptimized, webpackImportGetter, maybeLineStart } = targetToFlags(target);

    await breakpointScopes(dbg, target, "try-catches", { line: 8, column: maybeLineStart(4) }, [
      "Block",
      ["two", rollupOptimized || "2"],
      "Catch",
      ["err", '"AnError"'],
      "Function Body",
      ["one", rollupOptimized || "1"],
      "Module",
      "root()"
    ]);
  }
}

async function testLexAndNonlex(dbg) {
  await breakpointScopes(dbg, "webpack3", "lex-and-nonlex", { line: 3, column: 0 }, [
    "Block",
    ["<this>", "undefined"],
    ["arguments", "Arguments"],
    "Block",
    "Thing()",
    "Block",
    ["arguments", "(unavailable)"],
    ["someHelper", "(optimized away)"],
    ...webpackModule("webpack3", "lex-and-nonlex", true /* optimizedOut */),
    ["module", "(optimized away)"],
    "root()"
  ]);

  for (const target of [
    "rollup",
    "rollup-babel6",
    "webpack3-babel6",
  ]) {
    const { rollupOptimized, webpackImportGetter, maybeLineStart } = targetToFlags(target);

    await breakpointScopes(dbg, target, "lex-and-nonlex", { line: 3, column: maybeLineStart(4) }, [
      "Function Body",
      "Thing()",
      "root",
      target === "rollup" ? ["someHelper", "(optimized away)"] : "someHelper()",
      "Module",
      "root()"
    ]);
  }
}

async function testTypescriptClasses(dbg) {
  // Typescript is not available on the Babel builds.
  for (const target of [
    "webpack3",
    "rollup",
  ]) {
    const { isRollup, rollupOptimized } = targetToFlags(target);

    await breakpointScopes(dbg, target, "typescript-classes", { line: 50, column: 2 }, [
      "Module",
      "AnotherThing()",
      "AppComponent()",
      "decoratorFactory()",
      rollupOptimized ? ["def", rollupOptimized] : "def()",
      rollupOptimized ? ["ExportedOther", rollupOptimized] : "ExportedOther()",
      rollupOptimized ? ["ExpressionClass", rollupOptimized] : "ExpressionClass:Foo()",
      "fn()",
      // Rollup optimizes out the 'ns' reference here, but when it does, it leave a mapping
      // pointed at a location that is super weird, so it ends up being unmapped instead
      // be "(optimized out)".
      ["ns", isRollup ? "(unmapped)" : "{\u2026}"],
      "SubDecl()",
      "SubVar:SubExpr()"
    ]);
  }
}

async function testTypeModule(dbg) {
  await breakpointScopes(dbg, "webpack3", "type-module", { line: 7, column: 0 }, [
    "Block",
    ["<this>", "Window"],
    ["arguments", "Arguments"],
    "Block",
    ["alsoModuleScoped", "2"],
    ...webpackModule("webpack3", "type-module", true /* optimizedOut */),
    ["module", "(optimized away)"],
    ["moduleScoped", "1"],
    "thirdModuleScoped()"
  ]);

  for (const target of [
    "rollup",
    "rollup-babel6",
    "webpack3-babel6",
  ]) {
    const { rollupOptimized, webpackImportGetter, maybeLineStart } = targetToFlags(target);

    await breakpointScopes(dbg, target, "type-module", { line: 7, column: maybeLineStart(2) }, [
      "Module",
      ["alsoModuleScoped", "2"],
      ["moduleScoped", "1"],
      "thirdModuleScoped()"
    ]);
  }
}

async function testTypeScriptCJS(dbg) {
  await breakpointScopes(dbg, "webpack3", "type-script-cjs", { line: 7, column: 0 }, [
    "Block",
    ["<this>", "Window"],
    ["arguments", "Arguments"],
    "Block",
    "alsoModuleScopes",

    "webpack3TypeScriptCjs",
    ["arguments", "(unavailable)"],
    ["exports", "(optimized away)"],
    ["module", "(optimized away)"],
    "moduleScoped",
    "nonModules",
    "thirdModuleScoped",
  ]);

  // CJS does not work on Rollup.
  for (const target of [
    "webpack3-babel6",
  ]) {
    await breakpointScopes(dbg, target, "type-script-cjs", { line: 7, column: 2 }, [
      "Module",
      "alsoModuleScopes",
      "moduleScoped",
      "nonModules",
      "thirdModuleScoped",
    ]);
  }
}

async function testOutOfOrderDeclarationsCJS(dbg) {
  // CJS does not work on Rollup.
  for (const target of [
    "webpack3-babel6",
  ]) {
    await breakpointScopes(
      dbg,
      target,
      "out-of-order-declarations-cjs",
      { line: 8, column: 4 },
      [
        "callback",
        "fn()",
        ["val", "undefined"],
        "root",
        ["callback", "(optimized away)"],
        ["fn", "(optimized away)"],
        ["val", "(optimized away)"],
        "Module",

        // This value is currently optimized away, which isn't 100% accurate.
        // Because import declarations is the last thing in the file, our current
        // logic doesn't cover _both_ 'var' statements that it generates,
        // making us use the first, optimized-out binding. Given that imports
        // are almost never the last thing in a file though, this is probably not
        // a huge deal for now.
        ["aDefault", "(optimized away)"],
        ["root", "(optimized away)"],
        ["val", "(optimized away)"]
      ]
    );
  }
}

async function testCommonJS(dbg) {
  await breakpointScopes(dbg, "webpack3", "modules-cjs", { line: 7, column: 0 }, [
    "Block",
    ["<this>", "Window"],
    ["arguments", "Arguments"],
    "Block",
    ["alsoModuleScoped", "2"],
    "webpack3ModulesCjs",
    ["arguments", "(unavailable)"],
    ["exports", "(optimized away)"],
    ["module", "(optimized away)"],
    ["moduleScoped", "1"],
    "thirdModuleScoped()"
  ]);

  // CJS does not work on Rollup.
  for (const target of [
    "webpack3-babel6",
  ]) {
    await breakpointScopes(dbg, target, "modules-cjs", { line: 7, column: 2 }, [
      "Module",
      ["alsoModuleScoped", "2"],
      ["moduleScoped", "1"],
      "thirdModuleScoped()"
    ]);
  }
}

async function testWebpackLineMappings(dbg) {
  await breakpointScopes(
    dbg,
    "webpack3",
    "webpack-line-mappings",
    { line: 11, column: 0 },
    [
      "Block",
      ["<this>", '"this-value"'],
      ["arg", '"arg-value"'],
      ["arguments", "Arguments"],
      ["inner", "undefined"],
      "Block",
      ["someName", "(optimized away)"],
      "Block",
      ["two", "2"],
      "Block",
      ["one", "1"],
      "root",
      ["arguments", "Arguments"],
      "fn:someName()",
      "webpack3WebpackLineMappings",
      ["__webpack_exports__", "(optimized away)"],
      ["__WEBPACK_IMPORTED_MODULE_0__src_mod1__", "{\u2026}"],
      ["__webpack_require__", "(optimized away)"],
      ["arguments", "(unavailable)"],
      ["module", "(optimized away)"],
      "root()"
    ]
  );
}

async function testWebpackFunctions(dbg) {
  await breakpointScopes(dbg, "webpack3", "webpack-functions", { line: 4, column: 0 }, [
    "Block",
    ["<this>", "{\u2026}"],
    ["arguments", "Arguments"],
    ["x", "4"],
    "webpack3WebpackFunctions",
    ["__webpack_exports__", "(optimized away)"],
    ["__webpack_require__", "(optimized away)"],
    ["arguments", "(unavailable)"],
    ["module", "{\u2026}"],
    ["root", "(optimized away)"]
  ]);
}

async function testESModules(dbg) {
  await breakpointScopes(
    dbg,
    "webpack3",
    "esmodules",
    { line: 20, column: 0 },
    [
      "Block",
      ["<this>", "Window"],
      ["arguments", "Arguments"],
      pairToFnName("webpack3", "esmodules"),
      "__webpack_exports__",
      "__WEBPACK_IMPORTED_MODULE_0__src_mod1__",
      "__WEBPACK_IMPORTED_MODULE_1__src_mod2__",
      "__WEBPACK_IMPORTED_MODULE_10__src_optimized_out__",
      "__WEBPACK_IMPORTED_MODULE_2__src_mod3__",
      "__WEBPACK_IMPORTED_MODULE_3__src_mod4__",
      "__WEBPACK_IMPORTED_MODULE_4__src_mod5__",
      "__WEBPACK_IMPORTED_MODULE_5__src_mod6__",
      "__WEBPACK_IMPORTED_MODULE_6__src_mod7__",
      "__WEBPACK_IMPORTED_MODULE_7__src_mod9__",
      "__WEBPACK_IMPORTED_MODULE_8__src_mod10__",
      "__WEBPACK_IMPORTED_MODULE_9__src_mod11__",
      "__webpack_require__",
      "arguments",
      "example",
      "module",
      "root()",
    ]
  );

  for (const target of [
    "rollup",
    "webpack3-babel6",
  ]) {
    const { webpackImportGetter, maybeLineStart } = targetToFlags(target);

    await breakpointScopes(
      dbg,
      target,
      "esmodules",
      { line: 20, column: maybeLineStart(2) },
      [
        "Module",
        ["aDefault", '"a-default"'],
        ["aDefault2", '"a-default2"'],
        ["aDefault3", '"a-default3"'],
        ["anAliased", webpackImportGetter || '"an-original"'],
        ["anAliased2", webpackImportGetter || '"an-original2"'],
        ["anAliased3", webpackImportGetter || '"an-original3"'],
        ["aNamed", webpackImportGetter || '"a-named"'],
        ["aNamed2", webpackImportGetter || '"a-named2"'],
        ["aNamed3", webpackImportGetter || '"a-named3"'],
        ["aNamespace", "{\u2026}"],
        ["anotherNamed", webpackImportGetter || '"a-named"'],
        ["anotherNamed2", webpackImportGetter || '"a-named2"'],
        ["anotherNamed3", webpackImportGetter || '"a-named3"'],
        ["example", "(optimized away)"],
        ["optimizedOut", "(optimized away)"],
        "root()"
      ]
    );
  }

  // This test currently bails out because Babel does not map function calls
  // fully and includes the () of the call in the range of the identifier.
  // this means that Rollup, has to map locations for calls to imports,
  // it can fail. This will be addressed in Babel eventually.
  await breakpointScopes(
    dbg,
    "rollup-babel6",
    "esmodules",
    { line: 20, column: 2 },
    [
      "root",
      ["<this>", "Window"],
      ["arguments", "Arguments"],
      "rollupBabel6Esmodules",
      ["aDefault", '"a-default"'],
      ["aDefault2", '"a-default2"'],
      ["aDefault3", '"a-default3"'],
      ["aNamed", '"a-named"'],
      ["aNamed$1", '(optimized away)'],
      ["aNamed2", '"a-named2"'],
      ["aNamed3", '"a-named3"'],
      ["aNamespace", "{\u2026}"],
      ["arguments", "(unavailable)"],
      ["mod4", "(optimized away)"],
      ["original", '"an-original"'],
      ["original$1", '"an-original2"'],
      ["original$2", '"an-original3"'],
      "root()"
    ]
  );
}


async function testESModulesCJS(dbg) {
  await breakpointScopes(
    dbg,
    "webpack3",
    "esmodules-cjs",
    { line: 20, column: 0 },
    [
      "Block",
      ["<this>", "Window"],
      ["arguments", "Arguments"],
      pairToFnName("webpack3", "esmodules-cjs"),
      "__webpack_exports__",
      "__WEBPACK_IMPORTED_MODULE_0__src_mod1__",
      "__WEBPACK_IMPORTED_MODULE_1__src_mod2__",
      "__WEBPACK_IMPORTED_MODULE_10__src_optimized_out__",
      "__WEBPACK_IMPORTED_MODULE_2__src_mod3__",
      "__WEBPACK_IMPORTED_MODULE_3__src_mod4__",
      "__WEBPACK_IMPORTED_MODULE_4__src_mod5__",
      "__WEBPACK_IMPORTED_MODULE_5__src_mod6__",
      "__WEBPACK_IMPORTED_MODULE_6__src_mod7__",
      "__WEBPACK_IMPORTED_MODULE_7__src_mod9__",
      "__WEBPACK_IMPORTED_MODULE_8__src_mod10__",
      "__WEBPACK_IMPORTED_MODULE_9__src_mod11__",
      "__webpack_require__",
      "arguments",
      "example",
      "module",
      "root()",
    ]
  );

  // CJS does not work on Rollup.
  for (const target of [
    "webpack3-babel6",
  ]) {
    await breakpointScopes(
      dbg,
      target,
      "esmodules-cjs",
      { line: 20, column: 2 },
      [
      "Module",
        ["aDefault", '"a-default"'],
        ["aDefault2", '"a-default2"'],
        ["aDefault3", '"a-default3"'],
        ["anAliased", '"an-original"'],
        ["anAliased2", '"an-original2"'],
        ["anAliased3", '"an-original3"'],
        ["aNamed", '"a-named"'],
        ["aNamed2", '"a-named2"'],
        ["aNamed3", '"a-named3"'],
        ["aNamespace", "{\u2026}"],
        ["anotherNamed", '"a-named"'],
        ["anotherNamed2", '"a-named2"'],
        ["anotherNamed3", '"a-named3"'],
        ["example", "(optimized away)"],
        ["optimizedOut", "(optimized away)"],
        "root()"
      ]
    );
  }
}

async function testESModulesES6(dbg) {
  await breakpointScopes(
    dbg,
    "webpack3",
    "esmodules-es6",
    { line: 20, column: 0 },
    [
      "Block",
      ["<this>", "Window"],
      ["arguments", "Arguments"],
      pairToFnName("webpack3", "esmodules-es6"),
      "__webpack_exports__",
      "__WEBPACK_IMPORTED_MODULE_0__src_mod1__",
      "__WEBPACK_IMPORTED_MODULE_1__src_mod2__",
      "__WEBPACK_IMPORTED_MODULE_10__src_optimized_out__",
      "__WEBPACK_IMPORTED_MODULE_2__src_mod3__",
      "__WEBPACK_IMPORTED_MODULE_3__src_mod4__",
      "__WEBPACK_IMPORTED_MODULE_4__src_mod5__",
      "__WEBPACK_IMPORTED_MODULE_5__src_mod6__",
      "__WEBPACK_IMPORTED_MODULE_6__src_mod7__",
      "__WEBPACK_IMPORTED_MODULE_7__src_mod9__",
      "__WEBPACK_IMPORTED_MODULE_8__src_mod10__",
      "__WEBPACK_IMPORTED_MODULE_9__src_mod11__",
      "__webpack_require__",
      "arguments",
      "example",
      "module",
      "root()",
    ]
  );

  for (const target of [
    "rollup",
    "webpack3-babel6",
  ]) {
    const { maybeLineStart } = targetToFlags(target);

    await breakpointScopes(
      dbg,
      target,
      "esmodules-es6",
      { line: 20, column: maybeLineStart(2) },
      [
        "Module",
        ["aDefault", '"a-default"'],
        ["aDefault2", '"a-default2"'],
        ["aDefault3", '"a-default3"'],
        ["anAliased", '"an-original"'],
        ["anAliased2", '"an-original2"'],
        ["anAliased3", '"an-original3"'],
        ["aNamed", '"a-named"'],
        ["aNamed2", '"a-named2"'],
        ["aNamed3", '"a-named3"'],
        ["aNamespace", "{\u2026}"],
        ["anotherNamed", '"a-named"'],
        ["anotherNamed2", '"a-named2"'],
        ["anotherNamed3", '"a-named3"'],
        ["example", "(optimized away)"],
        ["optimizedOut", "(optimized away)"],
        "root()"
      ]
    );
  }

  // This test currently bails out because Babel does not map function calls
  // fully and includes the () of the call in the range of the identifier.
  // this means that Rollup, has to map locations for calls to imports,
  // it can fail. This will be addressed in Babel eventually.
  await breakpointScopes(
    dbg,
    "rollup-babel6",
    "esmodules-es6",
    { line: 20, column: 2 },
    [
      "root",
      ["<this>", "Window"],
      ["arguments", "Arguments"],

      "Block",
      ["aNamed", '"a-named"'],
      ["aNamed$1", "undefined"],
      ["aNamed2", '"a-named2"'],
      ["aNamed3", '"a-named3"'],
      ["original", '"an-original"'],
      ["original$1", '"an-original2"'],
      ["original$2", '"an-original3"'],

      "rollupBabel6EsmodulesEs6",
      ["aDefault", '"a-default"'],
      ["aDefault2", '"a-default2"'],
      ["aDefault3", '"a-default3"'],

      ["aNamespace", "{\u2026}"],
      ["arguments", "(unavailable)"],
      ["mod4", "(optimized away)"],
      "root()"
    ]
  );
}
