"use strict";

var gScript = SpecialPowers.loadChromeScript(
  SimpleTest.getTestFileURL("PresentationSessionChromeScript.js")
);
var receiverUrl = SimpleTest.getTestFileURL(
  "file_presentation_unknown_content_type.test"
);

var obs = SpecialPowers.Services.obs;

var receiverIframe;

function setup() {
  gScript.sendAsyncMessage("trigger-device-add");

  receiverIframe = document.createElement("iframe");
  receiverIframe.setAttribute("mozbrowser", "true");
  receiverIframe.setAttribute("mozpresentation", receiverUrl);
  receiverIframe.setAttribute("src", receiverUrl);
  var oop = !location.pathname.includes("_inproc");
  receiverIframe.setAttribute("remote", oop);

  var promise = new Promise(function(aResolve, aReject) {
    document.body.appendChild(receiverIframe);

    aResolve(receiverIframe);
  });
  obs.notifyObservers(promise, "setup-request-promise");
}

function testIncomingSessionRequestReceiverLaunchUnknownContentType() {
  let promise = Promise.all([
    new Promise(function(aResolve, aReject) {
      gScript.addMessageListener(
        "receiver-launching",
        function launchReceiverHandler(aSessionId) {
          gScript.removeMessageListener(
            "receiver-launching",
            launchReceiverHandler
          );
          info("Trying to launch receiver page.");

          receiverIframe.addEventListener("mozbrowserclose", function() {
            ok(true, "observe receiver window closed");
            aResolve();
          });
        }
      );
    }),
    new Promise(function(aResolve, aReject) {
      gScript.addMessageListener(
        "control-channel-closed",
        function controlChannelClosedHandler(aReason) {
          gScript.removeMessageListener(
            "control-channel-closed",
            controlChannelClosedHandler
          );
          is(
            aReason,
            0x80530020 /* NS_ERROR_DOM_OPERATION_ERR */,
            "The control channel is closed due to load failure."
          );
          aResolve();
        }
      );
    }),
  ]);

  gScript.sendAsyncMessage("trigger-incoming-session-request", receiverUrl);
  return promise;
}

function teardown() {
  gScript.addMessageListener(
    "teardown-complete",
    function teardownCompleteHandler() {
      gScript.removeMessageListener(
        "teardown-complete",
        teardownCompleteHandler
      );
      gScript.destroy();
      SimpleTest.finish();
    }
  );

  gScript.sendAsyncMessage("teardown");
}

function runTests() {
  setup();

  testIncomingSessionRequestReceiverLaunchUnknownContentType().then(teardown);
}

SimpleTest.waitForExplicitFinish();
SpecialPowers.pushPermissions(
  [
    { type: "presentation-device-manage", allow: false, context: document },
    { type: "browser", allow: true, context: document },
  ],
  function() {
    SpecialPowers.pushPrefEnv(
      {
        set: [
          ["dom.presentation.enabled", true],
          ["dom.presentation.controller.enabled", true],
          ["dom.presentation.receiver.enabled", true],
          ["dom.presentation.session_transport.data_channel.enable", false],
          ["network.disable.ipc.security", true],
          ["dom.ipc.tabs.disabled", false],
        ],
      },
      runTests
    );
  }
);
