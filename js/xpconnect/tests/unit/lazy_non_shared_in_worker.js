onmessage = event => {
  const lazy1 = {};
  const lazy2 = {};

  ChromeUtils.defineESModuleGetters(lazy1, {
    GetX: "resource://test/esm_lazy-1.sys.mjs",
  }, {
    global: "current",
  });

  ChromeUtils.defineESModuleGetters(lazy2, {
    GetX: "resource://test/esm_lazy-1.sys.mjs",
  }, {
    global: "contextual",
  });

  lazy1.GetX; // delazify before import.
  lazy2.GetX; // delazify before import.

  const ns = ChromeUtils.importESModule("resource://test/esm_lazy-1.sys.mjs", {
    global: "current",
  });

  const equal1 = ns.GetX == lazy1.GetX;
  const equal2 = ns.GetX == lazy2.GetX;

  postMessage({ equal1, equal2 });
};
