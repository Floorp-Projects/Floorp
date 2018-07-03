/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test can be really slow on debug platforms and should be split.
requestLongerTimeout(6);

// Tests loading sourcemapped sources for Babel's compile output.

async function breakpointScopes(dbg, fixture, { line, column }, scopes) {
  const filename = `fixtures/${fixture}/input.`;
  const fnName = fixture.replace(/-([a-z])/g, (s, c) => c.toUpperCase());

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

  await breakpointScopes(dbg, "typescript-classes", { line: 50, column: 2 }, [
    "Module",
    "AnotherThing()",
    "AppComponent()",
    "decoratorFactory()",
    "def()",
    "ExportedOther()",
    "ExpressionClass:Foo()",
    "fn()",
    ["ns", "{\u2026}"],
    "SubDecl()",
    "SubVar:SubExpr()"
  ]);

  await breakpointScopes(dbg, "babel-eval-maps", { line: 14, column: 4 }, [
    "Block",
    ["three", "5"],
    ["two", "4"],
    "Function Body",
    ["three", "3"],
    ["two", "2"],
    "root",
    ["one", "1"],
    "Module",
    "root()"
  ]);

  await breakpointScopes(dbg, "babel-for-of", { line: 5, column: 4 }, [
    "For",
    ["x", "1"],
    "forOf",
    "doThing()",
    "Module",
    "forOf",
    "mod"
  ]);

  await breakpointScopes(dbg, "babel-shadowed-vars", { line: 18, column: 6 }, [
    "Block",
    ["aConst", '"const3"'],
    ["aLet", '"let3"'],
    "Block",
    ["aConst", '"const2"'],
    ["aLet", '"let2"'],
    "Outer:_Outer()",
    "Function Body",
    ["aConst", '"const1"'],
    ["aLet", '"let1"'],
    "Outer()",
    "default",
    ["aVar", '"var3"']
  ]);

  await breakpointScopes(
    dbg,
    "babel-line-start-bindings-es6",
    { line: 19, column: 4 },
    [
      "Function Body",
      ["<this>", "{\u2026}"],
      ["one", "1"],
      ["two", "2"],
      "root",
      ["aFunc", "(optimized away)"],
      "Module",
      "root()"
    ]
  );

  await breakpointScopes(
    dbg,
    "babel-this-arguments-bindings",
    { line: 4, column: 4 },
    [
      "Function Body",
      ["<this>", '"this-value"'],
      ["arrow", "undefined"],
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
    "babel-this-arguments-bindings",
    { line: 8, column: 6 },
    [
      "arrow",
      ["<this>", '"this-value"'],
      ["argArrow", '"arrow-arg"'],
      "Function Body",
      "arrow()",
      "fn",
      ["arg", '"arg-value"'],
      ["arguments", "Arguments"],
      "root",
      "fn()",
      "Module",
      "root()"
    ]
  );

  await breakpointScopes(dbg, "babel-modules-cjs", { line: 20, column: 2 }, [
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
    ["aNamespace2", "{\u2026}"],
    ["aNamespace3", "{\u2026}"],
    ["anotherNamed", '"a-named"'],
    ["anotherNamed2", '"a-named2"'],
    ["anotherNamed3", '"a-named3"'],
    ["example", "(optimized away)"],
    ["optimizedOut", "(optimized away)"],
    "root()"
  ]);

  await breakpointScopes(dbg, "babel-classes", { line: 6, column: 6 }, [
    "Class",
    "Thing()",
    "Function Body",
    "Another()",
    "one",
    "Thing()",
    "Module",
    "root()"
  ]);

  await breakpointScopes(dbg, "babel-classes", { line: 16, column: 6 }, [
    "Function Body",
    ["three", "3"],
    ["two", "2"],
    "Class",
    "Another()",
    "Function Body",
    "Another()",
    ["one", "1"],
    "Thing()",
    "Module",
    "root()"
  ]);

  await breakpointScopes(dbg, "babel-for-loops", { line: 5, column: 4 }, [
    "For",
    ["i", "1"],
    "Function Body",
    ["i", "0"],
    "Module",
    "root()"
  ]);

  await breakpointScopes(dbg, "babel-for-loops", { line: 9, column: 4 }, [
    "For",
    ["i", '"2"'],
    "Function Body",
    ["i", "0"],
    "Module",
    "root()"
  ]);

  await breakpointScopes(dbg, "babel-for-loops", { line: 13, column: 4 }, [
    "For",
    ["i", "3"],
    "Function Body",
    ["i", "0"],
    "Module",
    "root()"
  ]);

  await breakpointScopes(dbg, "babel-functions", { line: 6, column: 8 }, [
    "arrow",
    ["p3", "undefined"],
    "Function Body",
    "arrow()",
    "inner",
    ["p2", "undefined"],
    "Function Expression",
    "inner()",
    "Function Body",
    "inner()",
    "decl",
    ["p1", "undefined"],
    "root",
    "decl()",
    "Module",
    "root()"
  ]);

  await breakpointScopes(dbg, "babel-type-module", { line: 7, column: 2 }, [
    "Module",
    ["alsoModuleScoped", "2"],
    ["moduleScoped", "1"],
    "thirdModuleScoped()"
  ]);

  await breakpointScopes(dbg, "babel-type-script", { line: 7, column: 2 }, []);

  await breakpointScopes(dbg, "babel-commonjs", { line: 7, column: 2 }, [
    "Module",
    ["alsoModuleScoped", "2"],
    ["moduleScoped", "1"],
    "thirdModuleScoped()"
  ]);

  await breakpointScopes(
    dbg,
    "babel-out-of-order-declarations-cjs",
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
  await breakpointScopes(
    dbg,
    "babel-flowtype-bindings",
    { line: 8, column: 2 },
    ["Module", ["aConst", '"a-const"'], "root()"]
  );

  await breakpointScopes(dbg, "babel-switches", { line: 7, column: 6 }, [
    "Switch",
    ["val", "2"],
    "Function Body",
    ["val", "1"],
    "Module",
    "root()"
  ]);

  await breakpointScopes(dbg, "babel-switches", { line: 10, column: 6 }, [
    "Block",
    ["val", "3"],
    "Switch",
    ["val", "2"],
    "Function Body",
    ["val", "1"],
    "Module",
    "root()"
  ]);

  await breakpointScopes(dbg, "babel-try-catches", { line: 8, column: 4 }, [
    "Block",
    ["two", "2"],
    "Catch",
    ["err", '"AnError"'],
    "Function Body",
    ["one", "1"],
    "Module",
    "root()"
  ]);

  await breakpointScopes(dbg, "babel-lex-and-nonlex", { line: 3, column: 4 }, [
    "Function Body",
    "Thing()",
    "root",
    "someHelper()",
    "Module",
    "root()"
  ]);

  await breakpointScopes(
    dbg,
    "babel-modules-webpack",
    { line: 20, column: 2 },
    [
      "Module",
      ["aDefault", '"a-default"'],
      ["aDefault2", '"a-default2"'],
      ["aDefault3", '"a-default3"'],
      ["anAliased", "Getter"],
      ["anAliased2", "Getter"],
      ["anAliased3", "Getter"],
      ["aNamed", "Getter"],
      ["aNamed2", "Getter"],
      ["aNamed3", "Getter"],
      ["aNamespace", "{\u2026}"],
      ["aNamespace2", "{\u2026}"],
      ["aNamespace3", "{\u2026}"],
      ["anotherNamed", "Getter"],
      ["anotherNamed2", "Getter"],
      ["anotherNamed3", "Getter"],
      ["example", "(optimized away)"],
      ["optimizedOut", "(optimized away)"],
      "root()"
    ]
  );

  await breakpointScopes(
    dbg,
    "babel-modules-webpack-es6",
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
      ["aNamespace2", "{\u2026}"],
      ["aNamespace3", "{\u2026}"],
      ["anotherNamed", '"a-named"'],
      ["anotherNamed2", '"a-named2"'],
      ["anotherNamed3", '"a-named3"'],
      ["example", "(optimized away)"],
      ["optimizedOut", "(optimized away)"],
      "root()"
    ]
  );

  await breakpointScopes(
    dbg,
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
      "webpackLineMappings",
      ["__webpack_exports__", "(optimized away)"],
      ["__WEBPACK_IMPORTED_MODULE_0__src_mod1__", "{\u2026}"],
      ["__webpack_require__", "(optimized away)"],
      ["arguments", "(unavailable)"],
      ["module", "(optimized away)"],
      "root()"
    ]
  );

  await breakpointScopes(dbg, "webpack-functions", { line: 4, column: 0 }, [
    "Block",
    ["<this>", "{\u2026}"],
    ["arguments", "Arguments"],
    ["x", "4"],
    "webpackFunctions",
    ["__webpack_exports__", "(optimized away)"],
    ["__webpack_require__", "(optimized away)"],
    ["arguments", "(unavailable)"],
    ["module", "{\u2026}"],
    ["root", "(optimized away)"]
  ]);
});
