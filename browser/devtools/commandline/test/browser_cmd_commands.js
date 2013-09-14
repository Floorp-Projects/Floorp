/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test various GCLI commands

const TEST_URI = "data:text/html;charset=utf-8,gcli-commands";

let tests = {};

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.runTests(options, tests);
  }).then(finish);
}

tests.testConsole = function(options) {
  let deferred = promise.defer();
  let hud = null;

  let onWebConsoleOpen = function(subject) {
    Services.obs.removeObserver(onWebConsoleOpen, "web-console-created");

    subject.QueryInterface(Ci.nsISupportsString);
    hud = HUDService.getHudReferenceById(subject.data);
    ok(hud, "console open");

    hud.jsterm.execute("pprint(window)", onExecute);
  }
  Services.obs.addObserver(onWebConsoleOpen, "web-console-created", false);

  function onExecute (msg) {
    ok(msg, "output for pprint(window)");

    hud.jsterm.once("messages-cleared", onClear);

    helpers.audit(options, [
      {
        setup: "console clear",
        exec: {
          output: ""
        },
      }
    ]);
  }

  function onClear() {
    let labels = hud.outputNode.querySelectorAll(".message");
    is(labels.length, 0, "no output in console");

    helpers.audit(options, [
      {
        setup: "console close",
        exec: {
          output: ""
        },
        post: function() {
          ok(!HUDService.getHudReferenceById(hud.hudId), "console closed");
        }
      }
    ]).then(function() {
      deferred.resolve();
    });
  }

  helpers.audit(options, [
    {
      setup: "console open",
      exec: { }
    }
  ]);

  return deferred.promise;
};
