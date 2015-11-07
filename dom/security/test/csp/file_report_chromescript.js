Components.utils.import("resource://gre/modules/Services.jsm");

const Ci = Components.interfaces;
const Cc = Components.classes;

const reportURI = "http://mochi.test:8888/foo.sjs";

var openingObserver = {
  observe: function(subject, topic, data) {
    // subject should be an nsURI
    if (subject.QueryInterface == undefined)
      return;

    var message = {report: "", error: false};

    if (topic == 'http-on-opening-request') {
      var asciiSpec = subject.QueryInterface(Ci.nsIHttpChannel).URI.asciiSpec;
      if (asciiSpec !== reportURI) return;

      var reportText = false;
      try {
        // Verify that the report was properly formatted.
        // We'll parse the report text as JSON and verify that the properties
        // have expected values.
        var reportText = "{}";
        var uploadStream = subject.QueryInterface(Ci.nsIUploadChannel).uploadStream;

        if (uploadStream) {
          // get the bytes from the request body
          var binstream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(Ci.nsIBinaryInputStream);
          binstream.setInputStream(uploadStream);

          var segments = [];
          for (var count = uploadStream.available(); count; count = uploadStream.available()) {
            var data = binstream.readBytes(count);
            segments.push(data);
          }

          var reportText = segments.join("");
          // rewind stream as we are supposed to - there will be an assertion later if we don't.
          uploadStream.QueryInterface(Ci.nsISeekableStream).seek(Ci.nsISeekableStream.NS_SEEK_SET, 0);
        }
        message.report = reportText;
      } catch (e) {
        message.error = e.toString();
      }

      sendAsyncMessage('opening-request-completed', message);
      Services.obs.removeObserver(openingObserver, 'http-on-opening-request');
    }
  }
};

Services.obs.addObserver(openingObserver, 'http-on-opening-request', false);
