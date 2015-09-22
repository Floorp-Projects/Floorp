/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure target is closed if client is closed directly
function test() {
  waitForExplicitFinish();

  getChromeActors((client, response) => {
    let options = {
      form: response,
      client: client,
      chrome: true
    };

    TargetFactory.forRemoteTab(options).then(target => {
      target.on("close", () => {
        ok(true, "Target was closed");
        finish();
      });
      client.close();
    });
  });
}
