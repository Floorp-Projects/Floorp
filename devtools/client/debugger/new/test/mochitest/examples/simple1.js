function main() {
  // A comment so we can test that breakpoint sliding works across
  // multiple lines
  const func = foo(1, 2);
  const result = func();
  return result;
}

function doEval() {
  eval("(" + function() {
    debugger;

    window.evaledFunc = function() {
      var foo = 1;
      var bar = 2;
      return foo + bar;
    };
  }.toString() + ")()");
}

function doNamedEval() {
  eval("(" + function() {
    debugger;

    window.evaledFunc = function() {
      var foo = 1;
      var bar = 2;
      return foo + bar;
    };
  }.toString() + ")();\n //# sourceURL=evaled.js");
}
