/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test various GCLI commands

const TEST_URI = "data:text/html;charset=utf-8,gcli-commands";

function test() {
  return Task.spawn(spawnTest).then(finish, helpers.handleError);
}

function spawnTest() {
  let options = yield helpers.openTab(TEST_URI);
  yield helpers.openToolbar(options);

  let subjectPromise = helpers.observeOnce("web-console-created");

  helpers.audit(options, [
    {
      setup: "console open",
      exec: { }
    }
  ]);

  let subject = yield subjectPromise;

  subject.QueryInterface(Ci.nsISupportsString);
  let hud = HUDService.getHudReferenceById(subject.data);
  ok(hud, "console open");

  let jstermExecute = helpers.promiseify(hud.jsterm.execute, hud.jsterm);
  let msg = yield jstermExecute("pprint(window)");

  ok(msg, "output for pprint(window)");

  let oncePromise = hud.jsterm.once("messages-cleared");

  helpers.audit(options, [
    {
      setup: "console clear",
      exec: { output: "" }
    }
  ]);

  yield oncePromise;

  let labels = hud.outputNode.querySelectorAll(".message");
  is(labels.length, 0, "no output in console");

  yield helpers.audit(options, [
    {
      setup: "console close",
      exec: { output: "" }
    }
  ]);

  ok(!HUDService.getHudReferenceById(hud.hudId), "console closed");

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
}
