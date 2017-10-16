/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var {WebChannel} = Cu.import("resource://gre/modules/WebChannel.jsm", {});

const TEST_URL_TAIL = "example.com/browser/browser/base/content/test/general/test_remoteTroubleshoot.html";
const TEST_URI_GOOD = Services.io.newURI("https://" + TEST_URL_TAIL);
const TEST_URI_BAD = Services.io.newURI("http://" + TEST_URL_TAIL);
const TEST_URI_GOOD_OBJECT = Services.io.newURI("https://" + TEST_URL_TAIL + "?object");

// Creates a one-shot web-channel for the test data to be sent back from the test page.
function promiseChannelResponse(channelID, originOrPermission) {
  return new Promise((resolve, reject) => {
    let channel = new WebChannel(channelID, originOrPermission);
    channel.listen((id, data, target) => {
      channel.stopListening();
      resolve(data);
    });
  });
}

// Loads the specified URI in a new tab and waits for it to send us data on our
// test web-channel and resolves with that data.
function promiseNewChannelResponse(uri) {
  let channelPromise = promiseChannelResponse("test-remote-troubleshooting-backchannel",
                                              uri);
  let tab = gBrowser.loadOneTab(uri.spec, {
    inBackground: false,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  return promiseTabLoaded(tab).then(
    () => channelPromise
  ).then(data => {
    gBrowser.removeTab(tab);
    return data;
  });
}

add_task(async function() {
  // We haven't set a permission yet - so even the "good" URI should fail.
  let got = await promiseNewChannelResponse(TEST_URI_GOOD);
  // Should return an error.
  Assert.ok(got.message.errno === 2, "should have failed with errno 2, no such channel");

  // Add a permission manager entry for our URI.
  Services.perms.add(TEST_URI_GOOD,
                     "remote-troubleshooting",
                     Services.perms.ALLOW_ACTION);
  registerCleanupFunction(() => {
    Services.perms.remove(TEST_URI_GOOD, "remote-troubleshooting");
  });

  // Try again - now we are expecting a response with the actual data.
  got = await promiseNewChannelResponse(TEST_URI_GOOD);

  // Check some keys we expect to always get.
  Assert.ok(got.message.extensions, "should have extensions");
  Assert.ok(got.message.graphics, "should have graphics");

  // Check we have channel and build ID info:
  Assert.equal(got.message.application.buildID, Services.appinfo.appBuildID,
               "should have correct build ID");

  let updateChannel = null;
  try {
    updateChannel = Cu.import("resource://gre/modules/UpdateUtils.jsm", {}).UpdateUtils.UpdateChannel;
  } catch (ex) {}
  if (!updateChannel) {
    Assert.ok(!("updateChannel" in got.message.application),
                "should not have update channel where not available.");
  } else {
    Assert.equal(got.message.application.updateChannel, updateChannel,
                 "should have correct update channel.");
  }


  // And check some keys we know we decline to return.
  Assert.ok(!got.message.modifiedPreferences, "should not have a modifiedPreferences key");
  Assert.ok(!got.message.crashes, "should not have crash info");

  // Now a http:// URI - should receive an error
  got = await promiseNewChannelResponse(TEST_URI_BAD);
  Assert.ok(got.message.errno === 2, "should have failed with errno 2, no such channel");

  // Check that the page can send an object as well if it's in the whitelist
  let webchannelWhitelistPref = "webchannel.allowObject.urlWhitelist";
  let origWhitelist = Services.prefs.getCharPref(webchannelWhitelistPref);
  let newWhitelist = origWhitelist + " https://example.com";
  Services.prefs.setCharPref(webchannelWhitelistPref, newWhitelist);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(webchannelWhitelistPref);
  });
  got = await promiseNewChannelResponse(TEST_URI_GOOD_OBJECT);
  Assert.ok(got.message, "should have gotten some data back");
});
