window.onload = function () {
  // We are the parent. Let's load the test.
  if (parent == this || !location.search.includes("worklet_iframe")) {
    SimpleTest.waitForExplicitFinish();

    // Let configureTest be optional.
    if (!window.configureTest) {
      window.configureTest = function () {
        return Promise.resolve();
      };
    }

    configureTest().then(() => {
      var iframe = document.createElement("iframe");
      iframe.src = location.href + "?worklet_iframe";
      document.body.appendChild(iframe);
    });

    return;
  }

  // Here we are in the iframe.
  window.SimpleTest = parent.SimpleTest;
  window.is = parent.is;
  window.isnot = parent.isnot;
  window.ok = parent.ok;
  window.info = parent.info;

  runTestInIframe();
};
