/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var browser = Services.wm.getMostRecentWindow("navigator:browser");
var connection = browser.navigator.mozMobileConnections[0];

// provide a fake APN and enable data connection.
// enable 3G radio
function enableRadio() {
  if (connection.radioState !== "enabled") {
    connection.setRadioEnabled(true);
  }
}

// disable 3G radio
function disableRadio() {
  if (connection.radioState === "enabled") {
    connection.setRadioEnabled(false);
  }
}

addMessageListener("prepare-network", function(message) {
  connection.addEventListener("datachange", function onDataChange() {
    if (connection.data.connected) {
      connection.removeEventListener("datachange", onDataChange);
      Services.prefs.setIntPref("network.proxy.type", 2);
      sendAsyncMessage("network-ready", true);
    }
  });

  enableRadio();
});

addMessageListener("network-cleanup", function(message) {
  connection.addEventListener("datachange", function onDataChange() {
    if (!connection.data.connected) {
      connection.removeEventListener("datachange", onDataChange);
      Services.prefs.setIntPref("network.proxy.type", 2);
      sendAsyncMessage("network-disabled", true);
    }
  });
  disableRadio();
});
