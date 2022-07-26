/* eslint-env mozilla/chrome-script */

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

Cu.importGlobalProperties(["TextDecoder"]);

const reportURI = "http://mochi.test:8888/foo.sjs";

var openingObserver = {
  observe(subject, topic, data) {
    // subject should be an nsURI
    if (subject.QueryInterface == undefined) {
      return;
    }

    var message = { report: "", error: false };

    if (topic == "http-on-opening-request") {
      var asciiSpec = subject.QueryInterface(Ci.nsIHttpChannel).URI.asciiSpec;
      if (asciiSpec !== reportURI) {
        return;
      }

      var reportText = false;
      try {
        // Verify that the report was properly formatted.
        // We'll parse the report text as JSON and verify that the properties
        // have expected values.
        var reportText = "{}";
        var uploadStream = subject.QueryInterface(Ci.nsIUploadChannel)
          .uploadStream;

        if (uploadStream) {
          // get the bytes from the request body
          var binstream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
            Ci.nsIBinaryInputStream
          );
          binstream.setInputStream(uploadStream);

          let bytes = NetUtil.readInputStream(binstream);

          // rewind stream as we are supposed to - there will be an assertion later if we don't.
          uploadStream
            .QueryInterface(Ci.nsISeekableStream)
            .seek(Ci.nsISeekableStream.NS_SEEK_SET, 0);

          let textDecoder = new TextDecoder();
          reportText = textDecoder.decode(bytes);
        }

        message.report = reportText;
      } catch (e) {
        message.error = e.toString();
      }

      sendAsyncMessage("opening-request-completed", message);
    }
  },
};

Services.obs.addObserver(openingObserver, "http-on-opening-request");
addMessageListener("finish", function() {
  Services.obs.removeObserver(openingObserver, "http-on-opening-request");
});
