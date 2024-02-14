onmessage = event => {
  let caught1 = false;
  try {
    const lazy = {};
    ChromeUtils.defineESModuleGetters(lazy, {
      obj: "resource://test/esmified-1.sys.mjs"
    });
    lazy.obj;
  } catch (e) {
    caught1 = true;
  }

  let caught2 = false;
  try {
    const lazy = {};
    ChromeUtils.defineESModuleGetters(lazy, {
      obj: "resource://test/esmified-1.sys.mjs"
    }, {
      global: "shared",
    });
    lazy.obj;
  } catch (e) {
    caught2 = true;
  }

  let caught3 = false;
  try {
    const lazy = {};
    ChromeUtils.defineESModuleGetters(lazy, {
      obj: "resource://test/esmified-1.sys.mjs"
    }, {
      global: "devtools",
    });
    lazy.obj;
  } catch (e) {
    caught3 = true;
  }

  let caught4 = false;
  try {
    const lazy = {};
    ChromeUtils.defineESModuleGetters(lazy, {
      obj: "resource://test/esmified-1.sys.mjs"
    }, {
      loadInDevToolsLoader: true,
    });
    lazy.obj;
  } catch (e) {
    caught4 = true;
  }
  postMessage({ caught1, caught2, caught3, caught4 });
};
