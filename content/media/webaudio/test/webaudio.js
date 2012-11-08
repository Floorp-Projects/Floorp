// Helpers for Web Audio tests

function expectException(func, exceptionCode) {
  var threw = false;
  try {
    func();
  } catch (ex) {
    threw = true;
    ok(ex instanceof DOMException, "Expect a DOM exception");
    ok(ex.code, exceptionCode, "Expect the correct exception code");
  }
  ok(threw, "The exception was thrown");
}

