function test() { 
// Builder tests

Pattern("program").match(Reflect.parse("42", {builder:{program:function()"program"}}));

assertGlobalStmt("throw 42", 1, { throwStatement: function() 1 });
assertGlobalStmt("for (;;);", 2, { forStatement: function() 2 });
assertGlobalStmt("for (x in y);", 3, { forInStatement: function() 3 });
assertGlobalStmt("{ }", 4, { blockStatement: function() 4 });
assertGlobalStmt("foo: { }", 5, { labeledStatement: function() 5 });
assertGlobalStmt("with (o) { }", 6, { withStatement: function() 6 });
assertGlobalStmt("while (x) { }", 7, { whileStatement: function() 7 });
assertGlobalStmt("do { } while(false);", 8, { doWhileStatement: function() 8 });
assertGlobalStmt("switch (x) { }", 9, { switchStatement: function() 9 });
assertGlobalStmt("try { } catch(e) { }", 10, { tryStatement: function() 10 });
assertGlobalStmt(";", 11, { emptyStatement: function() 11 });
assertGlobalStmt("debugger;", 12, { debuggerStatement: function() 12 });
assertGlobalStmt("42;", 13, { expressionStatement: function() 13 });
assertGlobalStmt("for (;;) break", forStmt(null, null, null, 14), { breakStatement: function() 14 });
assertGlobalStmt("for (;;) continue", forStmt(null, null, null, 15), { continueStatement: function() 15 });

assertBlockDecl("var x", "var", { variableDeclaration: function(kind) kind });
assertBlockDecl("let x", "let", { variableDeclaration: function(kind) kind });
assertBlockDecl("const x = undefined", "const", { variableDeclaration: function(kind) kind });
assertBlockDecl("function f() { }", "function", { functionDeclaration: function() "function" });

assertGlobalExpr("(x,y,z)", 1, { sequenceExpression: function() 1 });
assertGlobalExpr("(x ? y : z)", 2, { conditionalExpression: function() 2 });
assertGlobalExpr("x + y", 3, { binaryExpression: function() 3 });
assertGlobalExpr("delete x", 4, { unaryExpression: function() 4 });
assertGlobalExpr("x = y", 5, { assignmentExpression: function() 5 });
assertGlobalExpr("x || y", 6, { logicalExpression: function() 6 });
assertGlobalExpr("x++", 7, { updateExpression: function() 7 });
assertGlobalExpr("new x", 8, { newExpression: function() 8 });
assertGlobalExpr("x()", 9, { callExpression: function() 9 });
assertGlobalExpr("x.y", 10, { memberExpression: function() 10 });
assertGlobalExpr("(function() { })", 11, { functionExpression: function() 11 });
assertGlobalExpr("[1,2,3]", 12, { arrayExpression: function() 12 });
assertGlobalExpr("({ x: y })", 13, { objectExpression: function() 13 });
assertGlobalExpr("this", 14, { thisExpression: function() 14 });
assertGlobalExpr("[x for (x in y)]", 17, { comprehensionExpression: function() 17 });
assertGlobalExpr("(x for (x in y))", 18, { generatorExpression: function() 18 });
assertGlobalExpr("(function() { yield 42 })", genFunExpr(null, [], blockStmt([exprStmt(19)])), { yieldExpression: function() 19 });
assertGlobalExpr("(let (x) x)", 20, { letExpression: function() 20 });

assertGlobalStmt("switch (x) { case y: }", switchStmt(ident("x"), [1]), { switchCase: function() 1 });
assertGlobalStmt("try { } catch (e) { }", 2, { tryStatement: (function(b, g, u, f) u), catchClause: function() 2 });
assertGlobalStmt("try { } catch (e if e instanceof A) { } catch (e if e instanceof B) { }", [2, 2], { tryStatement: (function(b, g, u, f) g), catchClause: function() 2 });
assertGlobalStmt("try { } catch (e) { }", tryStmt(blockStmt([]), [], 2, null), { catchClause: function() 2 });
assertGlobalStmt("try { } catch (e if e instanceof A) { } catch (e if e instanceof B) { }",
                 tryStmt(blockStmt([]), [2, 2], null, null),
                 { catchClause: function() 2 });
assertGlobalExpr("[x for (y in z) for (x in y)]",
                 compExpr(ident("x"), [3, 3], null, "legacy"),
                 { comprehensionBlock: function() 3 });

assertGlobalExpr("({ x: y } = z)", aExpr("=", 1, ident("z")), { objectPattern: function() 1 });
assertGlobalExpr("({ x: y } = z)", aExpr("=", objPatt([2]), ident("z")), { propertyPattern: function() 2 });
assertGlobalExpr("[ x ] = y", aExpr("=", 3, ident("y")), { arrayPattern: function() 3 });

}

runtest(test);
