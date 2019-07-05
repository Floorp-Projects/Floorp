const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var modifyObserver = {
  observe: function(subject, topic, data) {
    if (topic == "http-on-modify-request") {
      var testOk = false;
      try {
        // We should be able to QI the request to an nsIChannel, then get
        // the notificationCallbacks without throwing an exception.
        var ir = subject.QueryInterface(Ci.nsIChannel).notificationCallbacks;

        // The notificationCallbacks should be an nsIInterfaceRequestor.
        testOk = ir.toString().includes(Ci.nsIInterfaceRequestor);
      } catch (e) {}
      sendAsyncMessage("modify-request-completed", testOk);
      Services.obs.removeObserver(modifyObserver, "http-on-modify-request");
    }
  },
};

Services.obs.addObserver(modifyObserver, "http-on-modify-request");
