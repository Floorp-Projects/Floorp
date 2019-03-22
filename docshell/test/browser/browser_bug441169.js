/* Make sure that netError won't allow HTML injection through badcert parameters.  See bug 441169. */
var newBrowser;

function task() {
  let resolve;
  let promise = new Promise(r => { resolve = r; });

  addEventListener("DOMContentLoaded", checkPage, false);

  function checkPage(event) {
    if (event.target != content.document) {
      return;
    }
    removeEventListener("DOMContentLoaded", checkPage, false);

    is(content.document.getElementById("test_span"), null, "Error message should not be parsed as HTML, and hence shouldn't include the 'test_span' element.");
    resolve();
  }

  var chromeURL = "about:neterror?e=nssBadCert&u=https%3A//test.kuix.de/&c=UTF-8&d=This%20sentence%20should%20not%20be%20parsed%20to%20include%20a%20%3Cspan%20id=%22test_span%22%3Enamed%3C/span%3E%20span%20tag.%0A%0AThe%20certificate%20is%20only%20valid%20for%20%3Ca%20id=%22cert_domain_link%22%20title=%22kuix.de%22%3Ekuix.de%3C/a%3E%0A%0A(Error%20code%3A%20ssl_error_bad_cert_domain)";
  content.location = chromeURL;

  return promise;
}

function test() {
  waitForExplicitFinish();

  var newTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = newTab;
  newBrowser = gBrowser.getBrowserForTab(newTab);

  ContentTask.spawn(newBrowser, null, task).then(() => {
    gBrowser.removeCurrentTab();
    finish();
  });
}
