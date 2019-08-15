const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

Cu.importGlobalProperties(["TextDecoder"]);

var openingObserver = {
  observe: function(subject, topic, data) {
    sendAsyncMessage("request-found", { subject, topic, data });
    // subject should be an nsURI
    if (subject.QueryInterface == undefined) {
      return;
    }

    if (topic == "http-on-opening-request") {
      var asciiSpec = subject.QueryInterface(Ci.nsIHttpChannel).URI.asciiSpec;
      sendAsyncMessage("request-found", asciiSpec);
      if (!asciiSpec.includes("report")) {
        return;
      }
      sendAsyncMessage("report-found");
    }
  },
};

Services.obs.addObserver(openingObserver, "http-on-opening-request");
addMessageListener("finish", function() {
  Services.obs.removeObserver(openingObserver, "http-on-opening-request");
});

sendAsyncMessage("proxy-ready");
