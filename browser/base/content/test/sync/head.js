// Mocks a getter or a function
// This is basically sinon.js (our in-tree version doesn't do getters :/) (see bug 1369855)
function mockReturn(obj, symbol, fixture) {
  let getter = Object.getOwnPropertyDescriptor(obj, symbol).get;
  if (getter) {
    Object.defineProperty(obj, symbol, {
      get() { return fixture; }
    });
    return {
      restore() {
        Object.defineProperty(obj, symbol, {
          get: getter
        });
      }
    }
  }
  let func = obj[symbol];
  obj[symbol] = () => fixture;
  return {
    restore() {
      obj[symbol] = func;
    }
  }
}
