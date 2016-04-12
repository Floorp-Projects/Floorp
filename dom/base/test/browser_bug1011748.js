const gHttpTestRoot = "http://example.com/browser/dom/base/test/";

add_task(function* () {
  var statusTexts = [];
  var xhr = new XMLHttpRequest();
  var observer = {
    observe: function (aSubject, aTopic, aData) {
      try {
        var channel = aSubject.QueryInterface(Ci.nsIHttpChannel);
        channel.getResponseHeader("Location");
      } catch (e) {
        return;
      }
      statusTexts.push(xhr.statusText);
    }
  };
  
  Services.obs.addObserver(observer, "http-on-examine-response", false);
  yield new Promise((resolve) => {
    xhr.addEventListener("load", function() {
      statusTexts.push(this.statusText);
      is(statusTexts[0], "", "Empty statusText value for HTTP 302");
      is(statusTexts[1], "OK", "OK statusText value for the redirect.");
      resolve();
    });
    xhr.open("GET", gHttpTestRoot+ "file_bug1011748_redirect.sjs", true);
    xhr.send();
  });

  Services.obs.removeObserver(observer, "http-on-examine-response");
});
