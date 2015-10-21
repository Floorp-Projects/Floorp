/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that creating an evaluation string for certain nodes works properly.
 */

function test() {
  let { Parser, ParserHelpers, SyntaxTreeVisitor } =
    Cu.import("resource://devtools/shared/Parser.jsm", {});

  function verify(source, predicate, string) {
    let ast = Parser.reflectionAPI.parse(source);
    let node = SyntaxTreeVisitor.filter(ast, predicate).pop();
    let info = ParserHelpers.getIdentifierEvalString(node);
    is(info, string, "The identifier evaluation string is correct.");
  }

  // Indentifier or Literal

  verify("foo", e => e.type == "Identifier", "foo");
  verify("undefined", e => e.type == "Identifier", "undefined");
  verify("null", e => e.type == "Literal", "null");
  verify("42", e => e.type == "Literal", "42");
  verify("true", e => e.type == "Literal", "true");
  verify("\"nasu\"", e => e.type == "Literal", "\"nasu\"");

  // MemberExpression or ThisExpression

  verify("this", e => e.type == "ThisExpression", "this");
  verify("foo.bar", e => e.name == "foo", "foo");
  verify("foo.bar", e => e.name == "bar", "foo.bar");

  // MemberExpression + ThisExpression

  verify("this.foo.bar", e => e.type == "ThisExpression", "this");
  verify("this.foo.bar", e => e.name == "foo", "this.foo");
  verify("this.foo.bar", e => e.name == "bar", "this.foo.bar");

  verify("foo.this.bar", e => e.name == "foo", "foo");
  verify("foo.this.bar", e => e.name == "this", "foo.this");
  verify("foo.this.bar", e => e.name == "bar", "foo.this.bar");

  // ObjectExpression + VariableDeclarator

  verify("let foo={bar:baz}", e => e.name == "baz", "baz");
  verify("let foo={bar:undefined}", e => e.name == "undefined", "undefined");
  verify("let foo={bar:null}", e => e.type == "Literal", "null");
  verify("let foo={bar:42}", e => e.type == "Literal", "42");
  verify("let foo={bar:true}", e => e.type == "Literal", "true");
  verify("let foo={bar:\"nasu\"}", e => e.type == "Literal", "\"nasu\"");
  verify("let foo={bar:this}", e => e.type == "ThisExpression", "this");

  verify("let foo={bar:{nested:baz}}", e => e.name == "baz", "baz");
  verify("let foo={bar:{nested:undefined}}", e => e.name == "undefined", "undefined");
  verify("let foo={bar:{nested:null}}", e => e.type == "Literal", "null");
  verify("let foo={bar:{nested:42}}", e => e.type == "Literal", "42");
  verify("let foo={bar:{nested:true}}", e => e.type == "Literal", "true");
  verify("let foo={bar:{nested:\"nasu\"}}", e => e.type == "Literal", "\"nasu\"");
  verify("let foo={bar:{nested:this}}", e => e.type == "ThisExpression", "this");

  verify("let foo={bar:baz}", e => e.name == "bar", "foo.bar");
  verify("let foo={bar:baz}", e => e.name == "foo", "foo");

  verify("let foo={bar:{nested:baz}}", e => e.name == "nested", "foo.bar.nested");
  verify("let foo={bar:{nested:baz}}", e => e.name == "bar", "foo.bar");
  verify("let foo={bar:{nested:baz}}", e => e.name == "foo", "foo");

  // ObjectExpression + MemberExpression

  verify("parent.foo={bar:baz}", e => e.name == "bar", "parent.foo.bar");
  verify("parent.foo={bar:baz}", e => e.name == "foo", "parent.foo");
  verify("parent.foo={bar:baz}", e => e.name == "parent", "parent");

  verify("parent.foo={bar:{nested:baz}}", e => e.name == "nested", "parent.foo.bar.nested");
  verify("parent.foo={bar:{nested:baz}}", e => e.name == "bar", "parent.foo.bar");
  verify("parent.foo={bar:{nested:baz}}", e => e.name == "foo", "parent.foo");
  verify("parent.foo={bar:{nested:baz}}", e => e.name == "parent", "parent");

  verify("this.foo={bar:{nested:baz}}", e => e.name == "nested", "this.foo.bar.nested");
  verify("this.foo={bar:{nested:baz}}", e => e.name == "bar", "this.foo.bar");
  verify("this.foo={bar:{nested:baz}}", e => e.name == "foo", "this.foo");
  verify("this.foo={bar:{nested:baz}}", e => e.type == "ThisExpression", "this");

  verify("this.parent.foo={bar:{nested:baz}}", e => e.name == "nested", "this.parent.foo.bar.nested");
  verify("this.parent.foo={bar:{nested:baz}}", e => e.name == "bar", "this.parent.foo.bar");
  verify("this.parent.foo={bar:{nested:baz}}", e => e.name == "foo", "this.parent.foo");
  verify("this.parent.foo={bar:{nested:baz}}", e => e.name == "parent", "this.parent");
  verify("this.parent.foo={bar:{nested:baz}}", e => e.type == "ThisExpression", "this");

  verify("parent.this.foo={bar:{nested:baz}}", e => e.name == "nested", "parent.this.foo.bar.nested");
  verify("parent.this.foo={bar:{nested:baz}}", e => e.name == "bar", "parent.this.foo.bar");
  verify("parent.this.foo={bar:{nested:baz}}", e => e.name == "foo", "parent.this.foo");
  verify("parent.this.foo={bar:{nested:baz}}", e => e.name == "this", "parent.this");
  verify("parent.this.foo={bar:{nested:baz}}", e => e.name == "parent", "parent");

  // FunctionExpression

  verify("function foo(){}", e => e.name == "foo", "foo");
  verify("var foo=function(){}", e => e.name == "foo", "foo");
  verify("var foo=function bar(){}", e => e.name == "bar", "bar");

  // New/CallExpression

  verify("foo()", e => e.name == "foo", "foo");
  verify("new foo()", e => e.name == "foo", "foo");

  verify("foo(bar)", e => e.name == "bar", "bar");
  verify("foo(bar, baz)", e => e.name == "baz", "baz");
  verify("foo(undefined)", e => e.name == "undefined", "undefined");
  verify("foo(null)", e => e.type == "Literal", "null");
  verify("foo(42)", e => e.type == "Literal", "42");
  verify("foo(true)", e => e.type == "Literal", "true");
  verify("foo(\"nasu\")", e => e.type == "Literal", "\"nasu\"");
  verify("foo(this)", e => e.type == "ThisExpression", "this");

  // New/CallExpression + ObjectExpression + MemberExpression

  verify("fun(this.parent.foo={bar:{nested:baz}})", e => e.name == "nested", "this.parent.foo.bar.nested");
  verify("fun(this.parent.foo={bar:{nested:baz}})", e => e.name == "bar", "this.parent.foo.bar");
  verify("fun(this.parent.foo={bar:{nested:baz}})", e => e.name == "foo", "this.parent.foo");
  verify("fun(this.parent.foo={bar:{nested:baz}})", e => e.name == "parent", "this.parent");
  verify("fun(this.parent.foo={bar:{nested:baz}})", e => e.type == "ThisExpression", "this");
  verify("fun(this.parent.foo={bar:{nested:baz}})", e => e.name == "fun", "fun");

  finish();
}
