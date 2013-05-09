/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test various GCLI commands

let HUDService = (Cu.import("resource:///modules/HUDService.jsm", {})).HUDService;

const TEST_URI = "data:text/html;charset=utf-8,gcli-commands";

let tests = {};

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.runTests(options, tests);
  }).then(finish);
}

tests.testConsole = function(options) {
  let deferred = Promise.defer();
  let hud = null;

  let onWebConsoleOpen = function(subject) {
    Services.obs.removeObserver(onWebConsoleOpen, "web-console-created");

    subject.QueryInterface(Ci.nsISupportsString);
    hud = HUDService.getHudReferenceById(subject.data);
    ok(hud.hudId in HUDService.hudReferences, "console open");

    hud.jsterm.execute("pprint(window)", onExecute);
  }
  Services.obs.addObserver(onWebConsoleOpen, "web-console-created", false);

  let onExecute = function() {
    let labels = hud.outputNode.querySelectorAll(".webconsole-msg-output");
    ok(labels.length > 0, "output for pprint(window)");

    helpers.audit(options, [
      {
        setup: "console clear",
        exec: {
          output: ""
        },
        post: function() {
          let labels = hud.outputNode.querySelectorAll(".webconsole-msg-output");
          // Bug 845827 - The GCLI "console clear" command doesn't always work
          // is(labels.length, 0, "no output in console");
        }
      },
      {
        setup: "console close",
        exec: {
          output: ""
        },
        post: function() {
          ok(!(hud.hudId in HUDService.hudReferences), "console closed");
        }
      }
    ]).then(function() {
      // FIXME: Remove this hack once bug 842347 is fixed
      // Gak - our promises impl causes so many stack frames that we blow up the
      // JS engine. Jumping to a new event with a truncated stack solves this.
      executeSoon(function() {
        deferred.resolve();
      });
    });
  };

  helpers.audit(options, [
    {
      setup: "console open",
      exec: { }
    }
  ]);

  return deferred.promise;
};
