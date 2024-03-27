// |jit-test| --enable-symbols-as-weakmap-keys; skip-if: getBuildConfiguration("release_or_beta")
for (x=0; x<10000; ++x) {
  try {
    m13 = new WeakMap;
    sym = Symbol();
    m13.set(sym, new Debugger);
    startgc(1, );
  } catch (exc) {}
}

