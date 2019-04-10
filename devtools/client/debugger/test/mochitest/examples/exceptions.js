function uncaughtException() {
  throw "unreachable"
}

function caughtException() {
  try {
    throw "reachable";
  } catch (e) {
    debugger;
  }
}

function deepError() {
  function a() { b(); }
  function b() { c(); }
  function c() { throw new Error(); }

  try {
    a();
  } catch (e) {}

  debugger;
}

function deepErrorFinally() {
  function a() { b(); }
  function b() {
    try {
      c();
    } finally {
      debugger;
    }
  }
  function c() { throw new Error(); }

  try {
    a();
  } catch (e) {}

  debugger;
}

function deepErrorCatch() {
  function a() { b(); }
  function b() {
    try {
      c();
    } catch (e) {
      debugger;
      throw e;
    }
  }
  function c() { throw new Error(); }

  try {
    a();
  } catch (e) {}

  debugger;
}

function deepErrorThrowDifferent() {
  function a() { b(); }
  function b() {
    try {
      c();
    } catch (e) {
      throw new Error();
    }
  }
  function c() { throw new Error(); }

  try {
    a();
  } catch (e) {}

  debugger;
}
