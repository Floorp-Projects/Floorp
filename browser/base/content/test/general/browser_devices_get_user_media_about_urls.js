/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PREF_LOOP_CSP = "loop.CSP";

const CONTENT_SCRIPT_HELPER = getRootDirectory(gTestPath) + "get_user_media_content_script.js";
Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript(getRootDirectory(gTestPath) + "get_user_media_helpers.js",
                 this);

var gTab;

// Taken from dom/media/tests/mochitest/head.js
function isMacOSX10_6orOlder() {
  var is106orOlder = false;

  if (navigator.platform.indexOf("Mac") == 0) {
    var version = Cc["@mozilla.org/system-info;1"]
                    .getService(Ci.nsIPropertyBag2)
                    .getProperty("version");
    // the next line is correct: Mac OS 10.6 corresponds to Darwin version 10.x !
    // Mac OS 10.7 is Darwin version 11.x. the |version| string we've got here
    // is the Darwin version.
    is106orOlder = (parseFloat(version) < 11.0);
  }
  return is106orOlder;
}

// Screensharing is disabled on older platforms (WinXP and Mac 10.6).
function isOldPlatform() {
  const isWinXP = navigator.userAgent.indexOf("Windows NT 5.1") != -1;
  if (isMacOSX10_6orOlder() || isWinXP) {
    info(true, "Screensharing disabled for OSX10.6 and WinXP");
    return true;
  }
  return false;
}

// Linux prompts aren't working for screensharing.
function isLinux() {
  return navigator.platform.indexOf("Linux") != -1;
}

function loadPage(aUrl) {
  let deferred = Promise.defer();

  gTab.linkedBrowser.addEventListener("load", function onload() {
    gTab.linkedBrowser.removeEventListener("load", onload, true);

    is(PopupNotifications._currentNotifications.length, 0,
       "should start the test without any prior popup notification");

    deferred.resolve();
  }, true);
  content.location = aUrl;
  return deferred.promise;
}

registerCleanupFunction(function() {
  gBrowser.removeCurrentTab();
});

const permissionError = "error: NotAllowedError: The request is not allowed " +
    "by the user agent or the platform in the current context.";

var gTests = [

{
  desc: "getUserMedia about:loopconversation shouldn't prompt",
  run: function checkAudioVideoLoop() {
    yield new Promise(resolve => SpecialPowers.pushPrefEnv({
      "set": [[PREF_LOOP_CSP, "default-src 'unsafe-inline'"]],
    }, resolve));

    yield loadPage("about:loopconversation");

    info("requesting devices");
    let promise = promiseObserverCalled("recording-device-events");
    yield promiseRequestDevice(true, true);
    yield promise;

    // Wait for the devices to actually be captured and running before
    // proceeding.
    yield promisePopupNotification("webRTC-sharingDevices");

    is((yield getMediaCaptureState()), "CameraAndMicrophone",
       "expected camera and microphone to be shared");

    yield closeStream();

    yield new Promise((resolve) => SpecialPowers.popPrefEnv(resolve));
  }
},

{
  desc: "getUserMedia about:loopconversation should prompt for window sharing",
  run: function checkShareScreenLoop() {
    if (isOldPlatform() || isLinux()) {
      return;
    }

    yield new Promise(resolve => SpecialPowers.pushPrefEnv({
      "set": [[PREF_LOOP_CSP, "default-src 'unsafe-inline'"]],
    }, resolve));

    yield loadPage("about:loopconversation");

    info("requesting screen");
    let promise = promiseObserverCalled("getUserMedia:request");
    yield promiseRequestDevice(false, true, null, "window");

    // Wait for the devices to actually be captured and running before
    // proceeding.
    yield promisePopupNotification("webRTC-shareDevices");

    is((yield getMediaCaptureState()), "none",
       "expected camera and microphone not to be shared");

    yield promiseMessage(permissionError, () => {
      PopupNotifications.panel.firstChild.button.click();
    });

    yield expectObserverCalled("getUserMedia:response:deny");
    yield expectObserverCalled("recording-window-ended");

    yield new Promise((resolve) => SpecialPowers.popPrefEnv(resolve));
  }
},

{
  desc: "getUserMedia about:evil should prompt",
  run: function checkAudioVideoNonLoop() {
    yield loadPage("about:evil");

    let promise = promiseObserverCalled("getUserMedia:request");
    yield promiseRequestDevice(true, true);
    yield promise;

    is((yield getMediaCaptureState()), "none",
       "expected camera and microphone not to be shared");
  }
},

];

function test() {
  waitForExplicitFinish();

  gTab = gBrowser.addTab();
  gBrowser.selectedTab = gTab;

  gTab.linkedBrowser.messageManager.loadFrameScript(CONTENT_SCRIPT_HELPER, true);

  Task.spawn(function () {
    yield ContentTask.spawn(gBrowser.selectedBrowser,
                            getRootDirectory(gTestPath) + "get_user_media.html",
                            function* (url) {
      const Ci = Components.interfaces;
      Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
      Components.utils.import("resource://gre/modules/Services.jsm");

      /* A fake about module to map get_user_media.html to different about urls. */
      function fakeLoopAboutModule() {
      }

      fakeLoopAboutModule.prototype = {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsIAboutModule]),
        newChannel: function (aURI, aLoadInfo) {
          let uri = Services.io.newURI(url, null, null);
          let chan = Services.io.newChannelFromURIWithLoadInfo(uri, aLoadInfo);
          chan.owner = Services.scriptSecurityManager.getSystemPrincipal();
          return chan;
        },
        getURIFlags: function (aURI) {
          return Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT |
                 Ci.nsIAboutModule.ALLOW_SCRIPT |
                 Ci.nsIAboutModule.URI_CAN_LOAD_IN_CHILD |
                 Ci.nsIAboutModule.HIDE_FROM_ABOUTABOUT;
        }
      };

      var factory = XPCOMUtils._getFactory(fakeLoopAboutModule);
      var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
      let UUIDGenerator = Components.classes["@mozilla.org/uuid-generator;1"]
                                    .getService(Ci.nsIUUIDGenerator);
      registrar.registerFactory(UUIDGenerator.generateUUID(), "",
                                "@mozilla.org/network/protocol/about;1?what=loopconversation",
                                factory);
      registrar.registerFactory(UUIDGenerator.generateUUID(), "",
                                "@mozilla.org/network/protocol/about;1?what=evil",
                                factory);
    });

    yield new Promise(resolve => SpecialPowers.pushPrefEnv({
      "set": [[PREF_PERMISSION_FAKE, true],
              ["media.getusermedia.screensharing.enabled", true]],
    }, resolve));

    for (let test of gTests) {
      info(test.desc);
      yield test.run();

      // Cleanup before the next test
      expectNoObserverCalled();
    }

    yield ContentTask.spawn(gBrowser.selectedBrowser, null,
                            function* () {
      const Ci = Components.interfaces;
      const Cc = Components.classes;
      var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
      let cid = Cc["@mozilla.org/network/protocol/about;1?what=loopconversation"];
      registrar.unregisterFactory(cid,
                                  registrar.getClassObject(cid, Ci.nsIFactory));
      cid = Cc["@mozilla.org/network/protocol/about;1?what=evil"];
      registrar.unregisterFactory(cid,
                                  registrar.getClassObject(cid, Ci.nsIFactory));
    });
  }).then(finish, ex => {
    ok(false, "Unexpected Exception: " + ex);
    finish();
  });
}
