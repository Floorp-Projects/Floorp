/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const Cc = Components.classes;
const Ci = Components.interfaces;

const gPostData = "postdata=true";

function test() {
  waitForExplicitFinish();

  let tab = gBrowser.selectedTab = gBrowser.addTab();
  registerCleanupFunction(function () {
    gBrowser.removeTab(tab);
  });

  var dataStream = Cc["@mozilla.org/io/string-input-stream;1"].
                   createInstance(Ci.nsIStringInputStream);
  dataStream.data = gPostData;

  var postStream = Cc["@mozilla.org/network/mime-input-stream;1"].
                   createInstance(Ci.nsIMIMEInputStream);
  postStream.addHeader("Content-Type", "application/x-www-form-urlencoded");
  postStream.addContentLength = true;
  postStream.setData(dataStream);

  tab.linkedBrowser.loadURIWithFlags("http://mochi.test:8888/browser/docshell/test/browser/print_postdata.sjs", 0, null, null, postStream);
  onTabLoad(tab, function (doc) {
    var bodyText = doc.body.textContent;
    is(bodyText, gPostData, "post data was submitted correctly");
    finish();
  });
}

function onTabLoad(tab, cb) {
  tab.linkedBrowser.addEventListener("load", function listener(event) {
    if (event.originalTarget != tab.linkedBrowser.contentDocument ||
        event.target.location.href == "about:blank") {
      info("skipping spurious load event");
      return;
    }
    tab.linkedBrowser.removeEventListener("load", listener, true);
    cb(tab.linkedBrowser.contentDocument);
  }, true);
}
