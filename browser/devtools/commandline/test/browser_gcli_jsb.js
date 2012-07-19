function test() {
  const TEST_URI = "http://example.com/browser/browser/devtools/commandline/" +
                   "test/resources_jsb_script.js";

  DeveloperToolbarTest.test("about:blank", function GJT_test() {
    /* Commented out by bug 774057, re-enable with un-hidden jsb command
    DeveloperToolbarTest.exec({
      typed: "jsb AAA",
      outputMatch: /valid/
    });

    gBrowser.addTabsProgressListener({
      onProgressChange: function GJT_onProgressChange(aBrowser) {
        gBrowser.removeTabsProgressListener(this);

        let win = aBrowser._contentWindow;
        let uri = win.document.location.href;
        let result = win.atob(uri.replace(/.*,/, ""));

        result = result.replace(/[\r\n]]/g, "\n");

        checkResult(result);
        finish();
      }
    });

    info("Checking beautification");
    DeveloperToolbarTest.checkInputStatus({
      typed: "jsb " + TEST_URI + " 4 space true -1 false collapse true false",
      status: "VALID"
    });
    DeveloperToolbarTest.exec({ completed: false });

    function checkResult(aResult) {
      let correct = "function somefunc() {\n" +
                    "    for (let n = 0; n < 500; n++) {\n" +
                    "        if (n % 2 == 1) {\n" +
                    "            console.log(n);\n" +
                    "            console.log(n + 1);\n" +
                    "        }\n" +
                    "    }\n" +
                    "}";
      is(aResult, correct, "JS has been correctly prettified");
    }
    */
    finish();
  });
}
