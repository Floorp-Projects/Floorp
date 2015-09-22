/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the jsb command works as it should

const TEST_URI = "http://example.com/browser/devtools/client/commandline/" +
                 "test/browser_cmd_jsb_script.jsi";

function test() {
  return Task.spawn(testTask).then(finish, helpers.handleError);
}

function* testTask() {
  let options = yield helpers.openTab("about:blank");
  yield helpers.openToolbar(options);

  let notifyPromise = wwNotifyOnce();

  helpers.audit(options, [
    {
      setup: 'jsb',
      check: {
        input:  'jsb',
        hints:     ' <url> [options]',
        markup: 'VVV',
        status: 'ERROR'
      }
    },
    {
      setup: 'jsb ' + TEST_URI,
      // Should result in a new scratchpad window
      exec: {
        output: '',
        error: false
      }
    }
  ]);

  let { subject } = yield notifyPromise;
  let scratchpadWin = subject.QueryInterface(Ci.nsIDOMWindow);
  yield helpers.listenOnce(scratchpadWin, "load");

  let scratchpad = scratchpadWin.Scratchpad;

  yield observeOnce(scratchpad);

  let result = scratchpad.getText();
  result = result.replace(/[\r\n]]*/g, "\n");
  let correct = "function somefunc() {\n" +
            "  if (true) // Some comment\n" +
            "    doSomething();\n" +
            "  for (let n = 0; n < 500; n++) {\n" +
            "    if (n % 2 == 1) {\n" +
            "      console.log(n);\n" +
            "      console.log(n + 1);\n" +
            "    }\n" +
            "  }\n" +
            "}";
  is(result, correct, "JS has been correctly prettified");

  if (scratchpadWin) {
    scratchpadWin.close();
    scratchpadWin = null;
  }

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
}

/**
 * A wrapper for calling Services.ww.[un]registerNotification using promises.
 * @return a promise that resolves when the notification service first notifies
 * with topic == "domwindowopened".
 * The value of the promise is { subject: subject, topic: topic, data: data }
 */
function wwNotifyOnce() {
  return new Promise(resolve => {
    let onNotify = (subject, topic, data) => {
      if (topic == "domwindowopened") {
        Services.ww.unregisterNotification(onNotify);
        resolve({ subject: subject, topic: topic, data: data });
      }
    };

    Services.ww.registerNotification(onNotify);
  });
}

/**
 * YET ANOTHER WRAPPER for a place where we are using events as poor-man's
 * promises. Perhaps this should be promoted to scratchpad?
 */
function observeOnce(scratchpad) {
  return new Promise(resolve => {
    let observer = {
      onReady: function() {
        scratchpad.removeObserver(observer);
        resolve();
      },
    };
    scratchpad.addObserver(observer);
  });
}
