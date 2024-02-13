onmessage = event => {
  const ns1 = ChromeUtils.importESModule("resource://test/esmified-1.sys.mjs", {
    global: "current",
  });

  const ns2 = ChromeUtils.importESModule("resource://test/esmified-1.sys.mjs", {
    global: "contextual",
  });

  const equal1 = ns1 == ns2;
  const equal2 = ns1.obj == ns2.obj;

  postMessage({ equal1, equal2 });
};
