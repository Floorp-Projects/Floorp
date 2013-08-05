/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the calllog commands works as they should

const TEST_URI = "data:text/html;charset=utf-8,cmd-calllog-chrome";

let tests = {};

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.runTests(options, tests);
  }).then(finish);
}

tests.testCallLogStatus = function(options) {
  return helpers.audit(options, [
    {
      setup: "calllog",
      check: {
        status: "ERROR",
        emptyParameters: [ " " ]
      }
    },
    {
      setup: "calllog chromestop",
      check: {
        status: "VALID",
        emptyParameters: [ " " ]
      }
    },
    {
      setup: "calllog chromestart content-variable window",
      check: {
        status: "VALID",
        emptyParameters: [ " " ]
      }
    },
    {
      setup: "calllog chromestart javascript \"({a1: function() {this.a2()},a2: function() {}});\"",
      check: {
        status: "VALID",
        emptyParameters: [ " " ]
      }
    },
  ]);
};

tests.testCallLogExec = function(options) {
  let deferred = promise.defer();

  function onWebConsoleOpen(subject) {
    Services.obs.removeObserver(onWebConsoleOpen, "web-console-created");

    subject.QueryInterface(Ci.nsISupportsString);
    let hud = HUDService.getHudReferenceById(subject.data);
    ok(hud, "console open");

    helpers.audit(options, [
      {
        setup: "calllog chromestop",
        exec: {
          output: /Stopped call logging/,
        }
      },
      {
        setup: "calllog chromestart javascript XXX",
        exec: {
          output: /following exception/,
        }
      },
      {
        setup: "console clear",
        exec: {
          output: '',
        },
        post: function() {
          let labels = hud.jsterm.outputNode.querySelectorAll(".webconsole-msg-output");
          is(labels.length, 0, "no output in console");
        }
      },
      {
        setup: "console close",
        exec: {
          output: '',
          completed: false,
        },
      },
    ]).then(function() {
      deferred.resolve();
    });
  }
  Services.obs.addObserver(onWebConsoleOpen, "web-console-created", false);

  helpers.audit(options, [
    {
      setup: "calllog chromestop",
      exec: {
        output: /No call logging/
      }
    },
    {
      setup: "calllog chromestart javascript \"({a1: function() {this.a2()},a2: function() {}});\"",
      exec: {
        output: /Call logging started/,
      }
    },
  ]);

  return deferred.promise;
};
