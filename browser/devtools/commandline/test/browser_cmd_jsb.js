/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the jsb command works as it should

const TEST_URI = "http://example.com/browser/browser/devtools/commandline/" +
                 "test/browser_cmd_jsb_script.jsi";

let scratchpadWin = null;
let Scratchpad = null;

function test() {
  DeveloperToolbarTest.test("about:blank", [ GJT_test ]);
}

function GJT_test() {
  helpers.setInput('jsb');
  helpers.check({
    input:  'jsb',
    hints:     ' <url> [options]',
    markup: 'VVV',
    status: 'ERROR'
  });
  DeveloperToolbarTest.exec({ completed: false });

  Services.ww.registerNotification(function(aSubject, aTopic, aData) {
      if (aTopic == "domwindowopened") {
        Services.ww.unregisterNotification(arguments.callee);

        scratchpadWin = aSubject.QueryInterface(Ci.nsIDOMWindow);
        scratchpadWin.addEventListener("load", function GDT_onLoad() {
          scratchpadWin.removeEventListener("load", GDT_onLoad, false);
          Scratchpad = scratchpadWin.Scratchpad;

          let observer = {
            onReady: function GJT_onReady() {
              Scratchpad.removeObserver(observer);

              let result = Scratchpad.getText();
              result = result.replace(/[\r\n]]*/g, "\n");
              let correct = "function somefunc() {\n" +
                        "  if (true) // Some comment\n" +
                        "  doSomething();\n" +
                        "  for (let n = 0; n < 500; n++) {\n" +
                        "    if (n % 2 == 1) {\n" +
                        "      console.log(n);\n" +
                        "      console.log(n + 1);\n" +
                        "    }\n" +
                        "  }\n" +
                        "}";
              is(result, correct, "JS has been correctly prettified");

              finishUp();
            },
          };
          Scratchpad.addObserver(observer);
        }, false);
      }
    });

  info("Checking beautification");
  DeveloperToolbarTest.exec({
    typed: "jsb " + TEST_URI,
    completed: false
  });
}

let finishUp = DeveloperToolbarTest.checkCalled(function GJT_finishUp() {
  if (scratchpadWin) {
    scratchpadWin.close();
    scratchpadWin = null;
  }
});
