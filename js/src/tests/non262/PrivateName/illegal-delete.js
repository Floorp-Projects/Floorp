// |reftest| skip-if(!xulRuntime.shell) -- needs newGlobal()

class A {
  #x = {a: 1};
  b = null;
  es(s) {
    eval(s);
  }
}

var a = new A;
a.b = new A;

assertThrowsInstanceOf(() => a.es('delete this.#x'), SyntaxError);
assertThrowsInstanceOf(() => a.es('delete (this.#x)'), SyntaxError);
assertThrowsInstanceOf(() => a.es('delete this?.#x'), SyntaxError);
assertThrowsInstanceOf(() => a.es('delete this?.b.#x'), SyntaxError);
// Should be OK
a.es('delete (0, this.#x.a)')
a.es('delete this?.b.#x.a')


// Make sure the above works in a different context, with emphasis on
// lazy/syntax parsing.
function eval_and_report(str) {
  var classTest = `
  class B {
    #x = {a: 1};
    b = null;
    test() {
      ${str};
    }
  }
  var b = new B;
  b.b = new B;
  b.test();
 `;

  var str = `
  function f(run) {
    if (run) {
      ${classTest}
    }
  }
  f(run)`;


  var throws = [];
  // Evalutate in a new global; has the advantage that it makes successes
  // idempotent.
  var g = newGlobal();
  g.run = false;

  try {
    // The idea is that this is a full parse
    evaluate(classTest, {global: g});
  } catch (e) {
    throws.push(e);
  }

  try {
    // The idea is this is a lazy parse; however, fields currently
    // disable lazy parsing, so right now
    evaluate(str, {global: g});
  } catch (e) {
    throws.push(e);
  }

  return throws;
}

function assertSyntaxError(str) {
  var exceptions = eval_and_report(str);
  assertEq(exceptions.length, 2);
  for (var e of exceptions) {
    assertEq(/SyntaxError/.test(e.name), true);
  }
}

function assertNoSyntaxError(str) {
  var exceptions = eval_and_report(str);
  assertEq(exceptions.length, 0);
}

assertSyntaxError('delete this.#x');
assertSyntaxError('delete (this.#x)');
assertSyntaxError('delete this?.#x');
assertSyntaxError('delete this?.b.#x');
// Should be OK
assertNoSyntaxError('delete (0, this.#x.a)')
assertNoSyntaxError('delete this?.b.#x.a')


if (typeof reportCompare === 'function') reportCompare(0, 0);
