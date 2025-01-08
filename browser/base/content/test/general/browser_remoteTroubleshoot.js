/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { WebChannel } = ChromeUtils.importESModule(
  "resource://gre/modules/WebChannel.sys.mjs"
);
const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
);

const TEST_URL_TAIL =
  "example.com/browser/browser/base/content/test/general/test_remoteTroubleshoot.html";
const TEST_URI_GOOD = Services.io.newURI("https://" + TEST_URL_TAIL);
const TEST_URI_BAD = Services.io.newURI("http://" + TEST_URL_TAIL);

// Creates a one-shot web-channel for the test data to be sent back from the test page.
function promiseChannelResponse(channelID, originOrPermission) {
  return new Promise(resolve => {
    let channel = new WebChannel(channelID, originOrPermission);
    channel.listen((id, data) => {
      channel.stopListening();
      resolve(data);
    });
  });
}

// Loads the specified URI in a new tab and waits for it to send us data on our
// test web-channel and resolves with that data.
function promiseNewChannelResponse(uri) {
  let channelPromise = promiseChannelResponse(
    "test-remote-troubleshooting-backchannel",
    uri
  );
  let tab = gBrowser.addTab(uri.spec, {
    inBackground: false,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  return promiseTabLoaded(tab)
    .then(() => channelPromise)
    .then(data => {
      gBrowser.removeTab(tab);
      return data;
    });
}

add_task(async function () {
  // We haven't set a permission yet - so even the "good" URI should fail.
  let got = await promiseNewChannelResponse(TEST_URI_GOOD);
  // Should return an error.
  Assert.ok(
    got.message.errno === 2,
    "should have failed with errno 2, no such channel"
  );

  // Add a permission manager entry for our URI.
  PermissionTestUtils.add(
    TEST_URI_GOOD,
    "remote-troubleshooting",
    Services.perms.ALLOW_ACTION
  );
  registerCleanupFunction(() => {
    PermissionTestUtils.remove(TEST_URI_GOOD, "remote-troubleshooting");
  });

  // Try again - now we are expecting a response with the actual data.
  got = await promiseNewChannelResponse(TEST_URI_GOOD);

  // Check some keys we expect to always get.
  Assert.ok(got.message.addons, "should have addons");
  Assert.ok(got.message.graphics, "should have graphics");

  // Check we have channel and build ID info:
  Assert.equal(
    got.message.application.buildID,
    Services.appinfo.appBuildID,
    "should have correct build ID"
  );

  let updateChannel = null;
  try {
    updateChannel = ChromeUtils.importESModule(
      "resource://gre/modules/UpdateUtils.sys.mjs"
    ).UpdateUtils.UpdateChannel;
  } catch (ex) {}
  if (!updateChannel) {
    Assert.ok(
      !("updateChannel" in got.message.application),
      "should not have update channel where not available."
    );
  } else {
    Assert.equal(
      got.message.application.updateChannel,
      updateChannel,
      "should have correct update channel."
    );
  }

  // And check some keys we know we decline to return.
  Assert.ok(
    !got.message.modifiedPreferences,
    "should not have a modifiedPreferences key"
  );
  Assert.ok(
    !got.message.printingPreferences,
    "should not have a printingPreferences key"
  );
  Assert.ok(!got.message.crashes, "should not have crash info");

  // Now a http:// URI - should receive an error
  got = await promiseNewChannelResponse(TEST_URI_BAD);
  Assert.ok(
    got.message.errno === 2,
    "should have failed with errno 2, no such channel"
  );
});
