onmessage = event => {
  let caught1 = false;
  try {
    ChromeUtils.importESModule("resource://test/esmified-1.sys.mjs");
  } catch (e) {
    caught1 = true;
  }

  let caught2 = false;
  try {
    ChromeUtils.importESModule("resource://test/esmified-1.sys.mjs", {
      global: "shared",
    });
  } catch (e) {
    caught2 = true;
  }

  let caught3 = false;
  try {
    ChromeUtils.importESModule("resource://test/esmified-1.sys.mjs", {
      global: "devtools",
    });
  } catch (e) {
    caught3 = true;
  }

  postMessage({ caught1, caught2, caught3 });
};
