// |jit-test| --enable-private-fields; --enable-ergonomic-brand-checks;
class Base {
  constructor(o) {
    return o;
  }
}

class A extends Base {
  #x = 12;
  #y = 13;
  static has(o) {
    return A.evalStr(o, '#x in o');
  }

  static evalStr(o, str) {
    return eval(str);
  }
}

var obj = {};
assertEq(A.has(obj), false);
new A(obj);
assertEq(A.has(obj), true);

A.evalStr(obj, `#x in o in o`)  // 'false' in o

function assertSyntaxError(functionText) {
  let threw = false;
  let exception = undefined;
  try {
    A.evalStr({}, functionText)
  } catch (e) {
    exception = e;
    threw = true;
  }
  assertEq(threw, true);
  assertEq(exception instanceof SyntaxError, true);
}

assertSyntaxError(`#x`);
assertSyntaxError(`#x == undefined`);
assertSyntaxError(`1 + #x in o`)
assertSyntaxError(`#z in o`);

assertSyntaxError(`for (#x in o) { return 1;}`)
assertSyntaxError(`!#x in o`)
assertSyntaxError(`+#x in o`)
assertSyntaxError(`-#x in o`)
assertSyntaxError(`~#x in o`)
assertSyntaxError(`void #x in o`)
assertSyntaxError(`typeof #x in o`)
assertSyntaxError(`++#x in o`)
assertSyntaxError(`--#x in o`)

assertSyntaxError(`#x in #y in o`)
assertSyntaxError(`{} instanceof #x in o`)
assertSyntaxError(`10 > #x in o`)
var threw = true
try {
  eval(`class Async {
    #x;
    async func(o) {
      await #x in o;
    }
  }`);
  threw = false;
} catch (e) {
  assertEq(e instanceof SyntaxError, true);
}
assertEq(threw, true);
