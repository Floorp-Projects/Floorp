/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the calllog commands works as they should

const TEST_URI = "data:text/html;charset=utf-8,gcli-calllog";

var tests = {};

function test() {
  return Task.spawn(function* () {
    let options = yield helpers.openTab(TEST_URI);
    yield helpers.openToolbar(options);

    yield helpers.runTests(options, tests);

    yield helpers.closeToolbar(options);
    yield helpers.closeTab(options);
  }).then(finish, helpers.handleError);
}

tests.testCallLogStatus = function (options) {
  return helpers.audit(options, [
    {
      setup: "calllog",
      check: {
        input:  "calllog",
        hints:         "",
        markup: "IIIIIII",
        status: "ERROR"
      }
    },
    {
      setup: "calllog start",
      check: {
        input:  "calllog start",
        hints:               "",
        markup: "VVVVVVVVVVVVV",
        status: "VALID"
      }
    },
    {
      setup: "calllog stop",
      check: {
        input:  "calllog stop",
        hints:              "",
        markup: "VVVVVVVVVVVV",
        status: "VALID"
      }
    },
  ]);
};

tests.testCallLogExec = function (options) {
  return new Promise((resolve, reject) => {
    var onWebConsoleOpen = function (subject) {
      Services.obs.removeObserver(onWebConsoleOpen, "web-console-created");

      subject.QueryInterface(Ci.nsISupportsString);
      let hud = HUDService.getHudReferenceById(subject.data);
      ok(hud, "console open");

      helpers.audit(options, [
        {
          setup: "calllog stop",
          exec: {
            output: /Stopped call logging/,
          }
        },
        {
          setup: "console clear",
          exec: {
            output: "",
          },
          post: function () {
            let labels = hud.outputNode.querySelectorAll(".webconsole-msg-output");
            is(labels.length, 0, "no output in console");
          }
        },
        {
          setup: "console close",
          exec: {
            output: "",
          }
        },
      ]).then(resolve);
    };
    Services.obs.addObserver(onWebConsoleOpen, "web-console-created");

    helpers.audit(options, [
      {
        setup: "calllog stop",
        exec: {
          output: /No call logging/,
        }
      },
      {
        name: "calllog start",
        setup: function () {
          // This test wants to be in a different event
          return new Promise((resolve, reject) => {
            executeSoon(function () {
              helpers.setInput(options, "calllog start").then(resolve);
            });
          });
        },
        exec: {
          output: /Call logging started/,
        },
      },
    ]);
  });
};
