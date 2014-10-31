var badPin = "https://include-subdomains.pinning.example.com";
var enabledPref = false;
var automaticPref = false;
var urlPref = "security.ssl.errorReporting.url";
var enforcement_level = 1;

function loadFrameScript() {
  let mm = Cc["@mozilla.org/globalmessagemanager;1"]
           .getService(Ci.nsIMessageListenerManager);
  const ROOT = getRootDirectory(gTestPath);
  mm.loadFrameScript(ROOT+"browser_bug846489_content.js", true);
}

add_task(function*(){
  waitForExplicitFinish();
  loadFrameScript();
  SimpleTest.requestCompleteLog();
  yield testSendReportDisabled();
  yield testSendReportManual();
  yield testSendReportAuto();
  yield testSendReportError();
  yield testSetAutomatic();
});

// creates a promise of the message in an error page
function createNetworkErrorMessagePromise(aBrowser) {
  return new Promise(function(resolve, reject) {
    // Error pages do not fire "load" events, so use a progressListener.
    var originalDocumentURI = aBrowser.contentDocument.documentURI;

    var progressListener = {
      onLocationChange: function(aWebProgress, aRequest, aLocation, aFlags) {
        // Make sure nothing other than an error page is loaded.
        if (!(aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_ERROR_PAGE)) {
          reject("location change was not to an error page");
        }
      },

      onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
        let doc = aBrowser.contentDocument;

        if (doc.getElementById("reportCertificateError")) {
          // Wait until the documentURI changes (from about:blank) this should
          // be the error page URI.
          var documentURI = doc.documentURI;
          if (documentURI == originalDocumentURI) {
            return;
          }

          aWebProgress.removeProgressListener(progressListener,
            Ci.nsIWebProgress.NOTIFY_LOCATION |
            Ci.nsIWebProgress.NOTIFY_STATE_REQUEST);
          var matchArray = /about:neterror\?.*&d=([^&]*)/.exec(documentURI);
          if (!matchArray) {
            reject("no network error message found in URI")
          return;
          }

          var errorMsg = matchArray[1];
          resolve(decodeURIComponent(errorMsg));
        }
      },

      QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                          Ci.nsISupportsWeakReference])
    };

    aBrowser.addProgressListener(progressListener,
            Ci.nsIWebProgress.NOTIFY_LOCATION |
            Ci.nsIWebProgress.NOTIFY_STATE_REQUEST);
  });
}

// check we can set the 'automatically send' pref
let testSetAutomatic = Task.async(function*() {
  setup();
  let tab = gBrowser.addTab(badPin, {skipAnimation: true});
  let browser = tab.linkedBrowser;
  let mm = browser.messageManager;

  gBrowser.selectedTab = tab;

  // ensure we have the correct error message from about:neterror
  let netError = createNetworkErrorMessagePromise(browser);
  yield netError;

  //  ensure that setting automatic when unset works
  let prefEnabled = new Promise(function(resolve, reject){
    mm.addMessageListener("ssler-test:AutoPrefUpdated", function() {
      if (Services.prefs.getBoolPref("security.ssl.errorReporting.automatic")) {
        resolve();
      } else {
        reject();
      }
    });
  });

  mm.sendAsyncMessage("ssler-test:SetAutoPref",{value:true});

  yield prefEnabled;

  // ensure un-setting automatic, when set, works
  let prefDisabled = new Promise(function(resolve, reject){
    mm.addMessageListener("ssler-test:AutoPrefUpdated", function () {
      if (!Services.prefs.getBoolPref("security.ssl.errorReporting.automatic")) {
        resolve();
      } else {
        reject();
      }
    });
  });

  mm.sendAsyncMessage("ssler-test:SetAutoPref",{value:false});

  yield prefDisabled;

  gBrowser.removeTab(tab);
  cleanup();
});

// test that manual report sending (with button clicks) works
let testSendReportManual = Task.async(function*() {
  setup();
  Services.prefs.setBoolPref("security.ssl.errorReporting.enabled", true);
  Services.prefs.setCharPref("security.ssl.errorReporting.url", "https://example.com/browser/browser/base/content/test/general/pinning_reports.sjs?succeed");

  let tab = gBrowser.addTab(badPin, {skipAnimation: true});
  let browser = tab.linkedBrowser;
  let mm = browser.messageManager;

  gBrowser.selectedTab = tab;

  // ensure we have the correct error message from about:neterror
  let netError = createNetworkErrorMessagePromise(browser);
  yield netError;
  netError.then(function(val){
    is(val.startsWith("An error occurred during a connection to include-subdomains.pinning.example.com"), true ,"ensure the correct error message came from about:neterror");
  });

  // Check the report starts on click
  let btn = browser.contentDocument.getElementById("reportCertificateError");

  // check the content script sends the message to report
  let reportWillStart = new Promise(function(resolve, reject){
    mm.addMessageListener("Browser:SendSSLErrorReport", function() {
      resolve();
    });
  });

  let deferredReportActivity = Promise.defer()
  let deferredReportSucceeds = Promise.defer();

  // ensure we see the correct statuses in the correct order...
  mm.addMessageListener("ssler-test:SSLErrorReportStatus", function(message) {
    switch(message.data.reportStatus) {
      case "activity":
        deferredReportActivity.resolve(message.data.reportStatus);
        break;
      case "complete":
        deferredReportSucceeds.resolve(message.data.reportStatus);
        break;
      case "error":
        deferredReportSucceeds.reject();
        deferredReportActivity.reject();
        break;
    }
  });

  // ... once the button is clicked, that is
  mm.sendAsyncMessage("ssler-test:SendBtnClick",{});

  yield reportWillStart;

  yield deferredReportActivity.promise;
  yield deferredReportSucceeds.promise;

  gBrowser.removeTab(tab);
  cleanup();
});

// test that automatic sending works
let testSendReportAuto = Task.async(function*() {
  setup();
  Services.prefs.setBoolPref("security.ssl.errorReporting.enabled", true);
  Services.prefs.setBoolPref("security.ssl.errorReporting.automatic", true);
  Services.prefs.setCharPref("security.ssl.errorReporting.url", "https://example.com/browser/browser/base/content/test/general/pinning_reports.sjs?succeed");

  let tab = gBrowser.addTab(badPin, {skipAnimation: true});
  let browser = tab.linkedBrowser;
  let mm = browser.messageManager;

  gBrowser.selectedTab = tab;

  let reportWillStart = Promise.defer();
  mm.addMessageListener("Browser:SendSSLErrorReport", function() {
    reportWillStart.resolve();
  });

  let deferredReportActivity = Promise.defer();
  let deferredReportSucceeds = Promise.defer();

  mm.addMessageListener("ssler-test:SSLErrorReportStatus", function(message) {
    switch(message.data.reportStatus) {
      case "activity":
        deferredReportActivity.resolve(message.data.reportStatus);
        break;
      case "complete":
        deferredReportSucceeds.resolve(message.data.reportStatus);
        break;
      case "error":
        deferredReportSucceeds.reject();
        deferredReportActivity.reject();
        break;
    }
  });

  // Ensure the error page loads
  let netError = createNetworkErrorMessagePromise(browser);
  yield netError;

  // Ensure the reporting steps all occur with no interaction
  yield reportWillStart;
  yield deferredReportActivity.promise;
  yield deferredReportSucceeds.promise;

  gBrowser.removeTab(tab);
  cleanup();
});

// test that an error is shown if there's a problem with the report server
let testSendReportError = Task.async(function*() {
  setup();
  Services.prefs.setBoolPref("security.ssl.errorReporting.enabled", true);
  Services.prefs.setBoolPref("security.ssl.errorReporting.automatic", true);
  Services.prefs.setCharPref("security.ssl.errorReporting.url", "https://example.com/browser/browser/base/content/test/general/pinning_reports.sjs?error");

  let tab = gBrowser.addTab(badPin, {skipAnimation: true});
  let browser = tab.linkedBrowser;
  let mm = browser.messageManager;

  gBrowser.selectedTab = tab;

  // check the report send starts....
  let reportWillStart = new Promise(function(resolve, reject){
    mm.addMessageListener("Browser:SendSSLErrorReport", function() {
      resolve();
    });
  });

  let netError = createNetworkErrorMessagePromise(browser);
  yield netError;
  yield reportWillStart;

  // and that errors are seen
  let reportErrors = new Promise(function(resolve, reject) {
    mm.addMessageListener("ssler-test:SSLErrorReportStatus", function(message) {
      switch(message.data.reportStatus) {
      case "complete":
        reject(message.data.reportStatus);
        break;
      case "error":
        resolve(message.data.reportStatus);
        break;
      }
    });
  });

  yield reportErrors;

  gBrowser.removeTab(tab);
  cleanup();
});

let testSendReportDisabled = Task.async(function*() {
  setup();
  Services.prefs.setBoolPref("security.ssl.errorReporting.enabled", false);
  Services.prefs.setCharPref("security.ssl.errorReporting.url", "https://offdomain.com");

  let tab = gBrowser.addTab(badPin, {skipAnimation: true});
  let browser = tab.linkedBrowser;
  let mm = browser.messageManager;

  gBrowser.selectedTab = tab;

  // Ensure we have an error page
  let netError = createNetworkErrorMessagePromise(browser);
  yield netError;

  let reportErrors = new Promise(function(resolve, reject) {
    mm.addMessageListener("ssler-test:SSLErrorReportStatus", function(message) {
      switch(message.data.reportStatus) {
      case "complete":
        reject(message.data.reportStatus);
        break;
      case "error":
        resolve(message.data.reportStatus);
        break;
      }
    });
  });

  // click the button
  mm.sendAsyncMessage("ssler-test:SendBtnClick",{forceUI:true});

  // check we get an error
  yield reportErrors;

  gBrowser.removeTab(tab);
  cleanup();
});

function setup() {
  // ensure the relevant prefs are set
  enabledPref = Services.prefs.getBoolPref("security.ssl.errorReporting.enabled");
  automaticPref = Services.prefs.getBoolPref("security.ssl.errorReporting.automatic");
  urlPref = Services.prefs.getCharPref("security.ssl.errorReporting.url");

  enforcement_level = Services.prefs.getIntPref("security.cert_pinning.enforcement_level");
  Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 2);
}

function cleanup() {
  // reset prefs for other tests in the run
  Services.prefs.setBoolPref("security.ssl.errorReporting.enabled", enabledPref);
  Services.prefs.setBoolPref("security.ssl.errorReporting.automatic", automaticPref);
  Services.prefs.setCharPref("security.ssl.errorReporting.url", urlPref);
}
