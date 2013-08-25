/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Make sure the listAddons request works as specified.

var gAddon1 = null;
var gAddon1Actor = null;

var gAddon2 = null;
var gAddon2Actor = null;

var gClient = null;

function test()
{
  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect(function (aType, aTraits) {
    is(aType, "browser", "Root actor should identify itself as a browser.");
    test_first_addon();
  })
}

function test_first_addon()
{
  let addonListChanged = false;
  gClient.addOneTimeListener("addonListChanged", function () {
    addonListChanged = true;
  });
  addAddon(ADDON1_URL, function(aAddon) {
    gAddon1 = aAddon;
    gClient.listAddons(function(aResponse) {
      for each (let addon in aResponse.addons) {
        if (addon.url == ADDON1_URL) {
          gAddon1Actor = addon.actor;
        }
      }
      ok(!addonListChanged, "Should not yet be notified that list of addons changed.");
      ok(gAddon1Actor, "Should find an addon actor for addon1.");
      test_second_addon();
    });
  });
}

function test_second_addon()
{
  let addonListChanged = false;
  gClient.addOneTimeListener("addonListChanged", function () {
    addonListChanged = true;
  });
  addAddon(ADDON2_URL, function(aAddon) {
    gAddon2 = aAddon;
    gClient.listAddons(function(aResponse) {
      let foundAddon1 = false;
      for each (let addon in aResponse.addons) {
        if (addon.url == ADDON1_URL) {
          is(addon.actor, gAddon1Actor, "Addon1's actor shouldn't have changed.");
          foundAddon1 = true;
        }
        if (addon.url == ADDON2_URL) {
          gAddon2Actor = addon.actor;
        }
      }
      ok(addonListChanged, "Should be notified that list of addons changed.");
      ok(foundAddon1, "Should find an addon actor for addon1.");
      ok(gAddon2Actor, "Should find an actor for addon2.");
      test_remove_addon();
    });
  });
}

function test_remove_addon()
{
  let addonListChanged = false;
  gClient.addOneTimeListener("addonListChanged", function () {
    addonListChanged = true;
  });
  removeAddon(gAddon1, function() {
    gClient.listAddons(function(aResponse) {
      let foundAddon1 = false;
      for each (let addon in aResponse.addons) {
        if (addon.url == ADDON1_URL) {
          foundAddon1 = true;
        }
      }
      ok(addonListChanged, "Should be notified that list of addons changed.");
      ok(!foundAddon1, "Addon1 should be gone");
      finish_test();
    });
  });
}

function finish_test()
{
  removeAddon(gAddon2, function() {
    gClient.close(function() {
      finish();
    });
  });
}
