/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var Cc = Components.classes;
var Ci = Components.interfaces;

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
  BrowserTestUtils.browserLoaded(tab.linkedBrowser).then(() => {
    ContentTask.spawn(tab.linkedBrowser, gPostData, function(postData) {
      var bodyText = content.document.body.textContent;
      is(bodyText, postData, "post data was submitted correctly");
    }).then(() => { finish(); });
  });
}
