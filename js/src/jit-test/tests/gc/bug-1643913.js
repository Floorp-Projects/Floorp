// |jit-test| --enable-weak-refs; --no-ti

for (let i = 70; i > 50; i--) {
  gc();
  gczeal(10, i);

  f(true, false, false);
  f(true, false, true);
}

function ccwToObject() {
  return evalcx('({})', newGlobal({newCompartment: true}));
}

function f(x, y, z) {
  let registry = new FinalizationRegistry(value => {});
  let target = x ? ccwToObject() : {};
  let heldValue = y ? ccwToObject() : {};
  let token = z ? ccwToObject() : {};
  registry.register(target, heldValue, token);
}
