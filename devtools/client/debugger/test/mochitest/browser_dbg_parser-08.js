/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that inferring anonymous function information is done correctly.
 */

function test() {
  let { Parser, ParserHelpers, SyntaxTreeVisitor } =
    Cu.import("resource://devtools/shared/Parser.jsm", {});

  function verify(source, predicate, details) {
    let { name, chain } = details;
    let [[sline, scol], [eline, ecol]] = details.loc;
    let ast = Parser.reflectionAPI.parse(source);
    let node = SyntaxTreeVisitor.filter(ast, predicate).pop();
    let info = ParserHelpers.inferFunctionExpressionInfo(node);

    is(info.name, name,
      "The function expression assignment property name is correct.");
    is(chain ? info.chain.toSource() : info.chain, chain ? chain.toSource() : chain,
      "The function expression assignment property chain is correct.");
    is(info.loc.start.toSource(), { line: sline, column: scol }.toSource(),
      "The start location was correct for the identifier in: '" + source + "'.");
    is(info.loc.end.toSource(), { line: eline, column: ecol }.toSource(),
      "The end location was correct for the identifier in: '" + source + "'.");
  }

  // VariableDeclarator

  verify("var foo=function(){}", e => e.type == "FunctionExpression", {
    name: "foo",
    chain: null,
    loc: [[1, 4], [1, 7]]
  });
  verify("\nvar\nfoo\n=\nfunction\n(\n)\n{\n}\n", e => e.type == "FunctionExpression", {
    name: "foo",
    chain: null,
    loc: [[3, 0], [3, 3]]
  });

  // AssignmentExpression

  verify("foo=function(){}", e => e.type == "FunctionExpression",
    { name: "foo", chain: [], loc: [[1, 0], [1, 3]] });

  verify("\nfoo\n=\nfunction\n(\n)\n{\n}\n", e => e.type == "FunctionExpression",
    { name: "foo", chain: [], loc: [[2, 0], [2, 3]] });

  verify("foo.bar=function(){}", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["foo"], loc: [[1, 0], [1, 7]] });

  verify("\nfoo.bar\n=\nfunction\n(\n)\n{\n}\n", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["foo"], loc: [[2, 0], [2, 7]] });

  verify("this.foo=function(){}", e => e.type == "FunctionExpression",
    { name: "foo", chain: ["this"], loc: [[1, 0], [1, 8]] });

  verify("\nthis.foo\n=\nfunction\n(\n)\n{\n}\n", e => e.type == "FunctionExpression",
    { name: "foo", chain: ["this"], loc: [[2, 0], [2, 8]] });

  verify("this.foo.bar=function(){}", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["this", "foo"], loc: [[1, 0], [1, 12]] });

  verify("\nthis.foo.bar\n=\nfunction\n(\n)\n{\n}\n", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["this", "foo"], loc: [[2, 0], [2, 12]] });

  verify("foo.this.bar=function(){}", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["foo", "this"], loc: [[1, 0], [1, 12]] });

  verify("\nfoo.this.bar\n=\nfunction\n(\n)\n{\n}\n", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["foo", "this"], loc: [[2, 0], [2, 12]] });

  // ObjectExpression

  verify("({foo:function(){}})", e => e.type == "FunctionExpression",
    { name: "foo", chain: [], loc: [[1, 2], [1, 5]] });

  verify("(\n{\nfoo\n:\nfunction\n(\n)\n{\n}\n}\n)", e => e.type == "FunctionExpression",
    { name: "foo", chain: [], loc: [[3, 0], [3, 3]] });

  verify("({foo:{bar:function(){}}})", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["foo"], loc: [[1, 7], [1, 10]] });

  verify("(\n{\nfoo\n:\n{\nbar\n:\nfunction\n(\n)\n{\n}\n}\n}\n)", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["foo"], loc: [[6, 0], [6, 3]] });

  // AssignmentExpression + ObjectExpression

  verify("foo={bar:function(){}}", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["foo"], loc: [[1, 5], [1, 8]] });

  verify("\nfoo\n=\n{\nbar\n:\nfunction\n(\n)\n{\n}\n}\n", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["foo"], loc: [[5, 0], [5, 3]] });

  verify("foo={bar:{baz:function(){}}}", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["foo", "bar"], loc: [[1, 10], [1, 13]] });

  verify("\nfoo\n=\n{\nbar\n:\n{\nbaz\n:\nfunction\n(\n)\n{\n}\n}\n}\n", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["foo", "bar"], loc: [[8, 0], [8, 3]] });

  verify("nested.foo={bar:function(){}}", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["nested", "foo"], loc: [[1, 12], [1, 15]] });

  verify("\nnested.foo\n=\n{\nbar\n:\nfunction\n(\n)\n{\n}\n}\n", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["nested", "foo"], loc: [[5, 0], [5, 3]] });

  verify("nested.foo={bar:{baz:function(){}}}", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["nested", "foo", "bar"], loc: [[1, 17], [1, 20]] });

  verify("\nnested.foo\n=\n{\nbar\n:\n{\nbaz\n:\nfunction\n(\n)\n{\n}\n}\n}\n", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["nested", "foo", "bar"], loc: [[8, 0], [8, 3]] });

  verify("this.foo={bar:function(){}}", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["this", "foo"], loc: [[1, 10], [1, 13]] });

  verify("\nthis.foo\n=\n{\nbar\n:\nfunction\n(\n)\n{\n}\n}\n", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["this", "foo"], loc: [[5, 0], [5, 3]] });

  verify("this.foo={bar:{baz:function(){}}}", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["this", "foo", "bar"], loc: [[1, 15], [1, 18]] });

  verify("\nthis.foo\n=\n{\nbar\n:\n{\nbaz\n:\nfunction\n(\n)\n{\n}\n}\n}\n", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["this", "foo", "bar"], loc: [[8, 0], [8, 3]] });

  verify("this.nested.foo={bar:function(){}}", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["this", "nested", "foo"], loc: [[1, 17], [1, 20]] });

  verify("\nthis.nested.foo\n=\n{\nbar\n:\nfunction\n(\n)\n{\n}\n}\n", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["this", "nested", "foo"], loc: [[5, 0], [5, 3]] });

  verify("this.nested.foo={bar:{baz:function(){}}}", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["this", "nested", "foo", "bar"], loc: [[1, 22], [1, 25]] });

  verify("\nthis.nested.foo\n=\n{\nbar\n:\n{\nbaz\n:\nfunction\n(\n)\n{\n}\n}\n}\n", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["this", "nested", "foo", "bar"], loc: [[8, 0], [8, 3]] });

  verify("nested.this.foo={bar:function(){}}", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["nested", "this", "foo"], loc: [[1, 17], [1, 20]] });

  verify("\nnested.this.foo\n=\n{\nbar\n:\nfunction\n(\n)\n{\n}\n}\n", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["nested", "this", "foo"], loc: [[5, 0], [5, 3]] });

  verify("nested.this.foo={bar:{baz:function(){}}}", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["nested", "this", "foo", "bar"], loc: [[1, 22], [1, 25]] });

  verify("\nnested.this.foo\n=\n{\nbar\n:\n{\nbaz\n:\nfunction\n(\n)\n{\n}\n}\n}\n", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["nested", "this", "foo", "bar"], loc: [[8, 0], [8, 3]] });

  // VariableDeclarator + AssignmentExpression + ObjectExpression

  verify("let foo={bar:function(){}}", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["foo"], loc: [[1, 9], [1, 12]] });

  verify("\nlet\nfoo\n=\n{\nbar\n:\nfunction\n(\n)\n{\n}\n}\n", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["foo"], loc: [[6, 0], [6, 3]] });

  verify("let foo={bar:{baz:function(){}}}", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["foo", "bar"], loc: [[1, 14], [1, 17]] });

  verify("\nlet\nfoo\n=\n{\nbar\n:\n{\nbaz\n:\nfunction\n(\n)\n{\n}\n}\n}\n", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["foo", "bar"], loc: [[9, 0], [9, 3]] });

  // New/CallExpression + AssignmentExpression + ObjectExpression

  verify("foo({bar:function(){}})", e => e.type == "FunctionExpression",
    { name: "bar", chain: [], loc: [[1, 5], [1, 8]] });

  verify("\nfoo\n(\n{\nbar\n:\nfunction\n(\n)\n{\n}\n}\n)\n", e => e.type == "FunctionExpression",
    { name: "bar", chain: [], loc: [[5, 0], [5, 3]] });

  verify("foo({bar:{baz:function(){}}})", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["bar"], loc: [[1, 10], [1, 13]] });

  verify("\nfoo\n(\n{\nbar\n:\n{\nbaz\n:\nfunction\n(\n)\n{\n}\n}\n}\n)\n", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["bar"], loc: [[8, 0], [8, 3]] });

  verify("nested.foo({bar:function(){}})", e => e.type == "FunctionExpression",
    { name: "bar", chain: [], loc: [[1, 12], [1, 15]] });

  verify("\nnested.foo\n(\n{\nbar\n:\nfunction\n(\n)\n{\n}\n}\n)\n", e => e.type == "FunctionExpression",
    { name: "bar", chain: [], loc: [[5, 0], [5, 3]] });

  verify("nested.foo({bar:{baz:function(){}}})", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["bar"], loc: [[1, 17], [1, 20]] });

  verify("\nnested.foo\n(\n{\nbar\n:\n{\nbaz\n:\nfunction\n(\n)\n{\n}\n}\n}\n)\n", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["bar"], loc: [[8, 0], [8, 3]] });

  verify("this.foo({bar:function(){}})", e => e.type == "FunctionExpression",
    { name: "bar", chain: [], loc: [[1, 10], [1, 13]] });

  verify("\nthis.foo\n(\n{\nbar\n:\nfunction\n(\n)\n{\n}\n}\n)\n", e => e.type == "FunctionExpression",
    { name: "bar", chain: [], loc: [[5, 0], [5, 3]] });

  verify("this.foo({bar:{baz:function(){}}})", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["bar"], loc: [[1, 15], [1, 18]] });

  verify("\nthis.foo\n(\n{\nbar\n:\n{\nbaz\n:\nfunction\n(\n)\n{\n}\n}\n}\n)\n", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["bar"], loc: [[8, 0], [8, 3]] });

  verify("this.nested.foo({bar:function(){}})", e => e.type == "FunctionExpression",
    { name: "bar", chain: [], loc: [[1, 17], [1, 20]] });

  verify("\nthis.nested.foo\n(\n{\nbar\n:\nfunction\n(\n)\n{\n}\n}\n)\n", e => e.type == "FunctionExpression",
    { name: "bar", chain: [], loc: [[5, 0], [5, 3]] });

  verify("this.nested.foo({bar:{baz:function(){}}})", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["bar"], loc: [[1, 22], [1, 25]] });

  verify("\nthis.nested.foo\n(\n{\nbar\n:\n{\nbaz\n:\nfunction\n(\n)\n{\n}\n}\n}\n)\n", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["bar"], loc: [[8, 0], [8, 3]] });

  verify("nested.this.foo({bar:function(){}})", e => e.type == "FunctionExpression",
    { name: "bar", chain: [], loc: [[1, 17], [1, 20]] });

  verify("\nnested.this.foo\n(\n{\nbar\n:\nfunction\n(\n)\n{\n}\n}\n)\n", e => e.type == "FunctionExpression",
    { name: "bar", chain: [], loc: [[5, 0], [5, 3]] });

  verify("nested.this.foo({bar:{baz:function(){}}})", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["bar"], loc: [[1, 22], [1, 25]] });

  verify("\nnested.this.foo\n(\n{\nbar\n:\n{\nbaz\n:\nfunction\n(\n)\n{\n}\n}\n}\n)\n", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["bar"], loc: [[8, 0], [8, 3]] });

  // New/CallExpression + VariableDeclarator + AssignmentExpression + ObjectExpression

  verify("let target=foo({bar:function(){}})", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["target"], loc: [[1, 16], [1, 19]] });

  verify("\nlet\ntarget=\nfoo\n(\n{\nbar\n:\nfunction\n(\n)\n{\n}\n}\n)\n", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["target"], loc: [[7, 0], [7, 3]] });

  verify("let target=foo({bar:{baz:function(){}}})", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["target", "bar"], loc: [[1, 21], [1, 24]] });

  verify("\nlet\ntarget=\nfoo\n(\n{\nbar\n:\n{\nbaz\n:\nfunction\n(\n)\n{\n}\n}\n}\n)\n", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["target", "bar"], loc: [[10, 0], [10, 3]] });

  verify("let target=nested.foo({bar:function(){}})", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["target"], loc: [[1, 23], [1, 26]] });

  verify("\nlet\ntarget=\nnested.foo\n(\n{\nbar\n:\nfunction\n(\n)\n{\n}\n}\n)\n", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["target"], loc: [[7, 0], [7, 3]] });

  verify("let target=nested.foo({bar:{baz:function(){}}})", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["target", "bar"], loc: [[1, 28], [1, 31]] });

  verify("\nlet\ntarget=\nnested.foo\n(\n{\nbar\n:\n{\nbaz\n:\nfunction\n(\n)\n{\n}\n}\n}\n)\n", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["target", "bar"], loc: [[10, 0], [10, 3]] });

  verify("let target=this.foo({bar:function(){}})", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["target"], loc: [[1, 21], [1, 24]] });

  verify("\nlet\ntarget=\nthis.foo\n(\n{\nbar\n:\nfunction\n(\n)\n{\n}\n}\n)\n", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["target"], loc: [[7, 0], [7, 3]] });

  verify("let target=this.foo({bar:{baz:function(){}}})", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["target", "bar"], loc: [[1, 26], [1, 29]] });

  verify("\nlet\ntarget=\nthis.foo\n(\n{\nbar\n:\n{\nbaz\n:\nfunction\n(\n)\n{\n}\n}\n}\n)\n", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["target", "bar"], loc: [[10, 0], [10, 3]] });

  verify("let target=this.nested.foo({bar:function(){}})", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["target"], loc: [[1, 28], [1, 31]] });

  verify("\nlet\ntarget=\nthis.nested.foo\n(\n{\nbar\n:\nfunction\n(\n)\n{\n}\n}\n)\n", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["target"], loc: [[7, 0], [7, 3]] });

  verify("let target=this.nested.foo({bar:{baz:function(){}}})", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["target", "bar"], loc: [[1, 33], [1, 36]] });

  verify("\nlet\ntarget=\nthis.nested.foo\n(\n{\nbar\n:\n{\nbaz\n:\nfunction\n(\n)\n{\n}\n}\n}\n)\n", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["target", "bar"], loc: [[10, 0], [10, 3]] });

  verify("let target=nested.this.foo({bar:function(){}})", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["target"], loc: [[1, 28], [1, 31]] });

  verify("\nlet\ntarget=\nnested.this.foo\n(\n{\nbar\n:\nfunction\n(\n)\n{\n}\n}\n)\n", e => e.type == "FunctionExpression",
    { name: "bar", chain: ["target"], loc: [[7, 0], [7, 3]] });

  verify("let target=nested.this.foo({bar:{baz:function(){}}})", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["target", "bar"], loc: [[1, 33], [1, 36]] });

  verify("\nlet\ntarget=\nnested.this.foo\n(\n{\nbar\n:\n{\nbaz\n:\nfunction\n(\n)\n{\n}\n}\n}\n)\n", e => e.type == "FunctionExpression",
    { name: "baz", chain: ["target", "bar"], loc: [[10, 0], [10, 3]] });

  finish();
}
