onmessage = event => {
  globalThis["loaded"] = [];

  var ns = ChromeUtils.importESModule("resource://test/non_shared_1.mjs", {
    global: "current",
  });
  const c1 = ns.getCounter();
  ns.incCounter();
  const c2 = ns.getCounter();
  postMessage({ c1, c2, loaded: globalThis["loaded"] });
};
