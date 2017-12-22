function expectSyntaxError(str) {
  var threwSyntaxError;
  try {
    eval(str);
  } catch (e) {
    threwSyntaxError = e instanceof SyntaxError;
  }
  assertEq(threwSyntaxError, true);

  try {
    eval('"use strict";' + str);
  } catch (e) {
    threwSyntaxError = e instanceof SyntaxError;
  }
  assertEq(threwSyntaxError, true);
}

function expectSloppyPass(str) {
  eval(str);

  try {
    eval('"use strict";' + str);
  } catch (e) {
    threwSyntaxError = e instanceof SyntaxError;
  }
  assertEq(threwSyntaxError, true);
}

expectSloppyPass(`l: function f1() {}`);
expectSloppyPass(`l0: l: function f1() {}`);
expectSloppyPass(`{ f1(); l: function f1() {} }`);
expectSloppyPass(`{ f1(); l0: l: function f1() {} }`);
expectSloppyPass(`{ f1(); l: function f1() { return 42; } } assertEq(f1(), 42);`);
expectSloppyPass(`eval("fe(); l: function fe() {}")`);
expectSyntaxError(`if (1) l: function f2() {}`);
expectSyntaxError(`if (1) {} else l: function f3() {}`);
expectSyntaxError(`do l: function f4() {} while (0)`);
expectSyntaxError(`while (0) l: function f5() {}`);
expectSyntaxError(`for (;;) l: function f6() {}`);
expectSloppyPass(`switch (1) { case 1: l: function f7() {} }`);
expectSloppyPass(`switch (1) { case 1: assertEq(f8(), 'f8'); case 2: l: function f8() { return 'f8'; } } assertEq(f8(), 'f8');`);

reportCompare(0, 0);
