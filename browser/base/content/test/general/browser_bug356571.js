// Bug 356571 - loadOneOrMoreURIs gives up if one of the URLs has an unknown protocol

const Cr = Components.results;
const Cm = Components.manager;

// Set to true when docShell alerts for unknown protocol error
var didFail = false;

// Override Alert to avoid blocking the test due to unknown protocol error
const kPromptServiceUUID = "{6cc9c9fe-bc0b-432b-a410-253ef8bcc699}";
const kPromptServiceContractID = "@mozilla.org/embedcomp/prompt-service;1";

// Save original prompt service factory
const kPromptServiceFactory = Cm.getClassObject(Cc[kPromptServiceContractID],
                                                Ci.nsIFactory);

let fakePromptServiceFactory = {
  createInstance: function(aOuter, aIid) {
    if (aOuter != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return promptService.QueryInterface(aIid);
  }
};

let promptService = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPromptService]),
  alert: function() {
    didFail = true;
  }
};

/* FIXME
Cm.QueryInterface(Ci.nsIComponentRegistrar)
  .registerFactory(Components.ID(kPromptServiceUUID), "Prompt Service",
                   kPromptServiceContractID, fakePromptServiceFactory);
*/

const kCompleteState = Ci.nsIWebProgressListener.STATE_STOP +
                       Ci.nsIWebProgressListener.STATE_IS_NETWORK;

const kDummyPage = "http://example.org/browser/browser/base/content/test/general/dummy_page.html";
const kURIs = [
  "bad://www.mozilla.org/",
  kDummyPage,
  kDummyPage,
];

var gProgressListener = {
  _runCount: 0,
  onStateChange: function (aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
    if ((aStateFlags & kCompleteState) == kCompleteState) {
      if (++this._runCount != kURIs.length)
        return;
      // Check we failed on unknown protocol (received an alert from docShell)
      ok(didFail, "Correctly failed on unknown protocol");
      // Check we opened all tabs
      ok(gBrowser.tabs.length == kURIs.length, "Correctly opened all expected tabs");
      finishTest();
    }
  }
}

function test() {
  todo(false, "temp. disabled");
  return; /* FIXME */
  waitForExplicitFinish();
  // Wait for all tabs to finish loading
  gBrowser.addTabsProgressListener(gProgressListener);
  loadOneOrMoreURIs(kURIs.join("|"));
}

function finishTest() {
  // Unregister the factory so we do not leak
  Cm.QueryInterface(Ci.nsIComponentRegistrar)
    .unregisterFactory(Components.ID(kPromptServiceUUID),
                       fakePromptServiceFactory);

  // Restore the original factory
  Cm.QueryInterface(Ci.nsIComponentRegistrar)
    .registerFactory(Components.ID(kPromptServiceUUID), "Prompt Service",
                     kPromptServiceContractID, kPromptServiceFactory);

  // Remove the listener
  gBrowser.removeTabsProgressListener(gProgressListener);

  // Close opened tabs
  for (var i = gBrowser.tabs.length-1; i > 0; i--)
    gBrowser.removeTab(gBrowser.tabs[i]);

  finish();
}
