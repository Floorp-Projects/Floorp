var map = new WeakMap();
var sym = Symbol();
try {
  map.set(sym, 1);
} catch (e) {
  assertEq(!!e.message.match(/an unregistered symbol/), false);
}
