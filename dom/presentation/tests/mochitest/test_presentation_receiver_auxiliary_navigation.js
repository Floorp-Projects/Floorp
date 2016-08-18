"use strict";

var gScript = SpecialPowers.loadChromeScript(SimpleTest.getTestFileURL("PresentationSessionChromeScript.js"));
var receiverUrl = SimpleTest.getTestFileURL("file_presentation_receiver_auxiliary_navigation.html");

var obs = SpecialPowers.Cc["@mozilla.org/observer-service;1"]
          .getService(SpecialPowers.Ci.nsIObserverService);

function setup() {
  return new Promise(function(aResolve, aReject) {
    gScript.sendAsyncMessage("trigger-device-add");

    var iframe = document.createElement("iframe");
    iframe.setAttribute("mozbrowser", "true");
    iframe.setAttribute("mozpresentation", receiverUrl);
    var oop = location.pathname.indexOf('_inproc') == -1;
    iframe.setAttribute("remote", oop);
    iframe.setAttribute("src", receiverUrl);

    // This event is triggered when the iframe calls "postMessage".
    iframe.addEventListener("mozbrowsershowmodalprompt", function listener(aEvent) {
      var message = aEvent.detail.message;
      if (/^OK /.exec(message)) {
        ok(true, "Message from iframe: " + message);
      } else if (/^KO /.exec(message)) {
        ok(false, "Message from iframe: " + message);
      } else if (/^INFO /.exec(message)) {
        info("Message from iframe: " + message);
      } else if (/^COMMAND /.exec(message)) {
        var command = JSON.parse(message.replace(/^COMMAND /, ""));
        gScript.sendAsyncMessage(command.name, command.data);
      } else if (/^DONE$/.exec(message)) {
        ok(true, "Messaging from iframe complete.");
        iframe.removeEventListener("mozbrowsershowmodalprompt", listener);

        teardown();
      }
    }, false);

    var promise = new Promise(function(aResolve, aReject) {
      document.body.appendChild(iframe);

      aResolve(iframe);
    });
    obs.notifyObservers(promise, "setup-request-promise", null);

    aResolve();
  });
}

function teardown() {
  gScript.addMessageListener("teardown-complete", function teardownCompleteHandler() {
    gScript.removeMessageListener("teardown-complete", teardownCompleteHandler);
    gScript.destroy();
    SimpleTest.finish();
  });

  gScript.sendAsyncMessage("teardown");
}

function runTests() {
  setup().then();
}

SimpleTest.waitForExplicitFinish();
SpecialPowers.pushPermissions([
  {type: "presentation-device-manage", allow: false, context: document},
  {type: "browser", allow: true, context: document},
], function() {
  SpecialPowers.pushPrefEnv({ "set": [["dom.presentation.enabled", true],
                                      ["dom.presentation.controller.enabled", true],
                                      ["dom.presentation.receiver.enabled", true],
                                      ["dom.mozBrowserFramesEnabled", true],
                                      ["dom.presentation.session_transport.data_channel.enable", false]]},
                            runTests);
});
