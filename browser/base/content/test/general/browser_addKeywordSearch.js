/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab("http://example.org/browser/browser/base/content/test/general/dummy_page.html");

  gBrowser.selectedBrowser.addEventListener("load", function runTests() {
    gBrowser.selectedBrowser.removeEventListener("load", runTests, true);

    let doc = gBrowser.contentDocument;
    let base = doc.createElement("base");
    doc.head.appendChild(base);

    let check = function (baseURI, fieldName, expected) {
      base.href = baseURI;

      let form = doc.createElement("form");
      let element = doc.createElement("input");
      element.setAttribute("type", "text");
      element.setAttribute("name", fieldName);
      form.appendChild(element);
      doc.body.appendChild(form);

      let data = GetSearchFieldBookmarkData(element);
      is(data.spec, expected, "Bookmark spec for search field named " + fieldName + " and baseURI " + baseURI + " incorrect");

      doc.body.removeChild(form);
    }

    let testData = [
    /* baseURI, field name, expected */
      [ 'http://example.com/', 'q', 'http://example.com/?q=%s' ],
      [ 'http://example.com/new-path-here/', 'q', 'http://example.com/new-path-here/?q=%s' ],
      [ '', 'q', 'http://example.org/browser/browser/base/content/test/general/dummy_page.html?q=%s' ],
    ]

    for (let data of testData) {
      check(data[0], data[1], data[2]);
    }

    // cleanup
    gBrowser.removeCurrentTab();
    finish();
  }, true);
}
