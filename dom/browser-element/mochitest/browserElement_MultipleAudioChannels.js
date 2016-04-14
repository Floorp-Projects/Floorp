"use strict";

SimpleTest.waitForExplicitFinish();

var tests = [false /* INPROC */, true /* OOP */];
var rootURI = "http://test/chrome/dom/browser-element/mochitest/";
var manifestURI = rootURI + "multipleAudioChannels_manifest.webapp";
var srcURI = rootURI +  "file_browserElement_MultipleAudioChannels.html";
var generator = startTest();
var app = null;
var channelsNum = 2;

addLoadEvent(() => {
  SpecialPowers.pushPermissions(
    [{ "type": "webapps-manage", "allow": 1, "context": document },
     { "type": "browser", "allow": 1, "context": document },
     { "type": "embed-apps", "allow": 1, "context": document }],
    function() {
      SpecialPowers.pushPrefEnv(
        {'set': [["dom.mozBrowserFramesEnabled", true],
                 ["dom.webapps.useCurrentProfile", true],
                 ["b2g.system_startup_url", window.location.href],
                 ["media.useAudioChannelAPI", true]]},
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

function showTestInfo(aOOPCEnabled) {
  if (aOOPCEnabled) {
    info("=== Start OOP testing ===");
  } else {
    info("=== Start INPROC testing ===");
  }
}

function uninstallApp(aApp) {
  if (aApp) {
    var request = navigator.mozApps.mgmt.uninstall(app);
    app = null;
    request.onerror = () => {
      error("Uninstall app failed!");
    };
    request.onsuccess = () => {
      is(request.result, manifestURI, "App uninstalled.");
      runNextTest();
    }
  }
}

function runTest(aOOPCEnabled) {
  var request = navigator.mozApps.install(manifestURI, {});
  request.onerror = () => {
    error("Install app failed!");
  };

  request.onsuccess = () => {
    app = request.result;
    ok(app, "App is installed.");
    is(app.manifestURL, manifestURI, "App manifest url is correct.");

    var iframe = document.createElement('iframe');
    iframe.setAttribute('mozbrowser', true);
    iframe.setAttribute('remote', aOOPCEnabled);
    iframe.src = srcURI;

    iframe.addEventListener('mozbrowserloadend', () => {
      var channels = iframe.allowedAudioChannels;
      is(channels.length, channelsNum, "Have two channels.");

      var activeCounter = 0;
      for (var idx = 0; idx < channelsNum; idx++) {
        let ac = channels[idx];
        ok(ac instanceof BrowserElementAudioChannel, "Correct class.");
        ok("getMuted" in ac, "ac.getMuted exists");
        ok("onactivestatechanged" in ac, "ac.onactivestatechanged exists");

        if (ac.name == "normal" || ac.name == "content") {
          ok(true, "Get correct channel type.");
        } else {
          error("Get unexpected channel type!");
        }

        ac.getMuted().onsuccess = (e) => {
          is(e.target.result, false, "Channel is unmuted.")
        }

        ac.onactivestatechanged = () => {
          ok(true, "Receive activestatechanged event from " + ac.name);
          ac.onactivestatechanged = null;
          if (++activeCounter == channelsNum) {
            document.body.removeChild(iframe);
            uninstallApp(app);
          }
        };
      }
    });

    document.body.appendChild(iframe);
  };
}

function runNextTest() {
  if (tests.length) {
    var isEnabledOOP = tests.shift();
    showTestInfo(isEnabledOOP);
    runTest(isEnabledOOP);
  } else {
    SimpleTest.finish();
  }
}

function startTest() {
  SpecialPowers.autoConfirmAppInstall(continueTest);
  yield undefined;

  SpecialPowers.autoConfirmAppUninstall(continueTest);
  yield undefined;

  runNextTest();
  yield undefined;
}
