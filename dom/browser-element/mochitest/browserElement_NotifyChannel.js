"use strict";

SimpleTest.waitForExplicitFinish();

const { classes: Cc, interfaces: Ci } = Components;
const systemMessenger = Cc["@mozilla.org/system-message-internal;1"]
                          .getService(Ci.nsISystemMessagesInternal);
const ioService = Cc["@mozilla.org/network/io-service;1"]
                    .getService(Ci.nsIIOService);

var tests = [false /* INPROC */, true /* OOP */];
var rootURI = "http://test/chrome/dom/browser-element/mochitest/";
var manifestURI = rootURI + "manifest.webapp";
var srcURI = rootURI +  "file_browserElement_NotifyChannel.html";
var generator = runTests();
var app = null;

addLoadEvent(() => {
  SpecialPowers.pushPermissions(
    [{ "type": "webapps-manage", "allow": 1, "context": document },
     { "type": "browser", "allow": 1, "context": document },
     { "type": "embed-apps", "allow": 1, "context": document }],
    function() {
      SpecialPowers.pushPrefEnv(
        {'set': [["dom.mozBrowserFramesEnabled", true],
                 ["dom.sysmsg.enabled", true]]},
        () => { generator.next(); })
    });
});

function error(message) {
  ok(false, message);
  SimpleTest.finish();
}

function continueTest() {
  try {
    generator.next();
  } catch (e if e instanceof StopIteration) {
    error("Stop test because of exception!");
  }
}

function registerPage(aEvent) {
  systemMessenger.registerPage(aEvent,
                               ioService.newURI(srcURI, null, null),
                               ioService.newURI(manifestURI, null, null));
}

function runTest(aEnable) {
  var request = navigator.mozApps.install(manifestURI, {});
  request.onerror = () => {
    error("Install app failed!");
  };

  request.onsuccess = () => {
    app = request.result;
    ok(app, "App is installed. remote = " + aEnable);
    is(app.manifestURL, manifestURI, "App manifest url is correct.");

    var iframe = document.createElement('iframe');
    iframe.setAttribute('mozbrowser', true);
    iframe.setAttribute('remote', aEnable);
    iframe.setAttribute('mozapp', manifestURI);
    iframe.src = srcURI;
    document.body.appendChild(iframe);

    iframe.addEventListener('mozbrowserloadend', () => {
      var channels = iframe.allowedAudioChannels;
      is(channels.length, 1, "1 audio channel by default");

      var ac = channels[0];
      ok(ac instanceof BrowserElementAudioChannel, "Correct class");
      ok("notifyChannel" in ac, "ac.notifyChannel exists");

      var message = "audiochannel-interruption-begin";
      registerPage(message);
      ac.notifyChannel(message);
      iframe.addEventListener("mozbrowsershowmodalprompt", function (e) {
        is(e.detail.message, message,
           "App got audiochannel-interruption-begin.");

        if (app) {
          request = navigator.mozApps.mgmt.uninstall(app);
          app = null;
          request.onerror = () => {
            error("Uninstall app failed!");
          };
          request.onsuccess = () => {
            is(request.result, manifestURI, "App uninstalled.");
            runNextTest();
          }
        }
      });
    });
  };
}

function runNextTest() {
  if (tests.length) {
    var isEnabledOOP = tests.shift();
    runTest(isEnabledOOP);
  } else {
    SimpleTest.finish();
  }
}

function runTests() {
  SpecialPowers.setAllAppsLaunchable(true);
  SpecialPowers.autoConfirmAppInstall(continueTest);
  yield undefined;

  SpecialPowers.autoConfirmAppUninstall(continueTest);
  yield undefined;

  runNextTest();
  yield undefined;
}
