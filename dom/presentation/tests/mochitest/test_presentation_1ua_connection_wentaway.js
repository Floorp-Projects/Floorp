"use strict";

SimpleTest.waitForExplicitFinish();
SimpleTest.requestFlakyTimeout("Test for guarantee not firing async event");

function debug(str) {
  // info(str);
}

var gScript = SpecialPowers.loadChromeScript(
  SimpleTest.getTestFileURL("PresentationSessionChromeScript1UA.js")
);
var receiverUrl = SimpleTest.getTestFileURL(
  "file_presentation_1ua_wentaway.html"
);
var request;
var connection;
var receiverIframe;

function setup() {
  gScript.addMessageListener("device-prompt", function devicePromptHandler() {
    debug("Got message: device-prompt");
    gScript.removeMessageListener("device-prompt", devicePromptHandler);
    gScript.sendAsyncMessage("trigger-device-prompt-select");
  });

  gScript.addMessageListener(
    "control-channel-established",
    function controlChannelEstablishedHandler() {
      gScript.removeMessageListener(
        "control-channel-established",
        controlChannelEstablishedHandler
      );
      gScript.sendAsyncMessage("trigger-control-channel-open");
    }
  );

  gScript.addMessageListener("sender-launch", function senderLaunchHandler(
    url
  ) {
    debug("Got message: sender-launch");
    gScript.removeMessageListener("sender-launch", senderLaunchHandler);
    is(url, receiverUrl, "Receiver: should receive the same url");
    receiverIframe = document.createElement("iframe");
    receiverIframe.setAttribute("mozbrowser", "true");
    receiverIframe.setAttribute("mozpresentation", receiverUrl);
    var oop = !location.pathname.includes("_inproc");
    receiverIframe.setAttribute("remote", oop);

    receiverIframe.setAttribute("src", receiverUrl);
    receiverIframe.addEventListener(
      "mozbrowserloadend",
      function() {
        info("Receiver loaded.");
      },
      { once: true }
    );

    // This event is triggered when the iframe calls "alert".
    receiverIframe.addEventListener(
      "mozbrowsershowmodalprompt",
      function receiverListener(evt) {
        var message = evt.detail.message;
        if (/^OK /.exec(message)) {
          ok(true, message.replace(/^OK /, ""));
        } else if (/^KO /.exec(message)) {
          ok(false, message.replace(/^KO /, ""));
        } else if (/^INFO /.exec(message)) {
          info(message.replace(/^INFO /, ""));
        } else if (/^COMMAND /.exec(message)) {
          var command = JSON.parse(message.replace(/^COMMAND /, ""));
          gScript.sendAsyncMessage(command.name, command.data);
        } else if (/^DONE$/.exec(message)) {
          receiverIframe.removeEventListener(
            "mozbrowsershowmodalprompt",
            receiverListener
          );
          teardown();
        }
      }
    );

    var promise = new Promise(function(aResolve, aReject) {
      document.body.appendChild(receiverIframe);
      aResolve(receiverIframe);
    });

    var obs = SpecialPowers.Services.obs;
    obs.notifyObservers(promise, "setup-request-promise");
  });

  gScript.addMessageListener(
    "promise-setup-ready",
    function promiseSetupReadyHandler() {
      debug("Got message: promise-setup-ready");
      gScript.removeMessageListener(
        "promise-setup-ready",
        promiseSetupReadyHandler
      );
      gScript.sendAsyncMessage("trigger-on-session-request", receiverUrl);
    }
  );

  return Promise.resolve();
}

function testCreateRequest() {
  return new Promise(function(aResolve, aReject) {
    info("Sender: --- testCreateRequest ---");
    request = new PresentationRequest(receiverUrl);
    request
      .getAvailability()
      .then(aAvailability => {
        is(
          aAvailability.value,
          false,
          "Sender: should have no available device after setup"
        );
        aAvailability.onchange = function() {
          aAvailability.onchange = null;
          ok(aAvailability.value, "Sender: Device should be available.");
          aResolve();
        };

        gScript.sendAsyncMessage("trigger-device-add");
      })
      .catch(aError => {
        ok(
          false,
          "Sender: Error occurred when getting availability: " + aError
        );
        teardown();
        aReject();
      });
  });
}

function testStartConnection() {
  return new Promise(function(aResolve, aReject) {
    request
      .start()
      .then(aConnection => {
        connection = aConnection;
        ok(connection, "Sender: Connection should be available.");
        ok(connection.id, "Sender: Connection ID should be set.");
        is(
          connection.state,
          "connecting",
          "Sender: The initial state should be connecting."
        );
        connection.onconnect = function() {
          connection.onconnect = null;
          is(connection.state, "connected", "Connection should be connected.");
          aResolve();
        };
      })
      .catch(aError => {
        ok(
          false,
          "Sender: Error occurred when establishing a connection: " + aError
        );
        teardown();
        aReject();
      });
  });
}

function testConnectionWentaway() {
  return new Promise(function(aResolve, aReject) {
    info("Sender: --- testConnectionWentaway ---");
    connection.onclose = function() {
      connection.onclose = null;
      is(connection.state, "closed", "Sender: Connection should be closed.");
      receiverIframe.addEventListener(
        "mozbrowserclose",
        function closeHandler() {
          ok(false, "wentaway should not trigger receiver close");
          aResolve();
        }
      );
      setTimeout(aResolve, 3000);
    };
    gScript.addMessageListener(
      "ready-to-remove-receiverFrame",
      function onReadyToRemove() {
        gScript.removeMessageListener(
          "ready-to-remove-receiverFrame",
          onReadyToRemove
        );
        receiverIframe.src = "http://example.com";
      }
    );
  });
}

function teardown() {
  gScript.addMessageListener(
    "teardown-complete",
    function teardownCompleteHandler() {
      debug("Got message: teardown-complete");
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
  setup()
    .then(testCreateRequest)
    .then(testStartConnection)
    .then(testConnectionWentaway)
    .then(teardown);
}

SpecialPowers.pushPermissions(
  [
    { type: "presentation-device-manage", allow: false, context: document },
    { type: "browser", allow: true, context: document },
  ],
  () => {
    SpecialPowers.pushPrefEnv(
      {
        set: [
          ["dom.presentation.enabled", true],
          ["dom.presentation.controller.enabled", true],
          ["dom.presentation.receiver.enabled", true],
          ["dom.presentation.test.enabled", true],
          ["dom.ipc.tabs.disabled", false],
          ["network.disable.ipc.security", true],
          ["dom.presentation.test.stage", 0],
        ],
      },
      runTests
    );
  }
);
