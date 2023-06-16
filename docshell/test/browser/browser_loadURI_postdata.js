/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const gPostData = "postdata=true";
const gUrl =
  "http://mochi.test:8888/browser/docshell/test/browser/print_postdata.sjs";

add_task(async function test_loadURI_persists_postData() {
  waitForExplicitFinish();

  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser));
  registerCleanupFunction(function () {
    gBrowser.removeTab(tab);
  });

  var dataStream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
    Ci.nsIStringInputStream
  );
  dataStream.data = gPostData;

  var postStream = Cc[
    "@mozilla.org/network/mime-input-stream;1"
  ].createInstance(Ci.nsIMIMEInputStream);
  postStream.addHeader("Content-Type", "application/x-www-form-urlencoded");
  postStream.setData(dataStream);
  var systemPrincipal = Cc["@mozilla.org/systemprincipal;1"].getService(
    Ci.nsIPrincipal
  );

  tab.linkedBrowser.loadURI(Services.io.newURI(gUrl), {
    triggeringPrincipal: systemPrincipal,
    postData: postStream,
  });
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, gUrl);
  let body = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    () => content.document.body.textContent
  );
  is(body, gPostData, "post data was submitted correctly");
  finish();
});
