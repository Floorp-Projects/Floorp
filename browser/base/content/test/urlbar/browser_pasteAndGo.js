"use strict";

let clipboardHelper = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);

add_task(function*() {
  const kURLs = [
    "http://example.com/1",
    "http://example.org/2\n",
    "http://\nexample.com/3\n",
  ];
  for (let url of kURLs) {
    yield BrowserTestUtils.withNewTab("about:blank", function* (browser) {
      gURLBar.focus();
      yield new Promise((resolve, reject) => {
        waitForClipboard(url, function() {
          clipboardHelper.copyString(url);
        }, resolve,
          () => reject(new Error(`Failed to copy string '${url}' to clipboard`))
        );
      });
      let textBox = document.getAnonymousElementByAttribute(gURLBar,
        "anonid", "textbox-input-box");
      let cxmenu = document.getAnonymousElementByAttribute(textBox,
        "anonid", "input-box-contextmenu");
      let cxmenuPromise = BrowserTestUtils.waitForEvent(cxmenu, "popupshown");
      EventUtils.synthesizeMouseAtCenter(gURLBar, {type: "contextmenu", button: 2});
      yield cxmenuPromise;
      let menuitem = document.getAnonymousElementByAttribute(textBox,
        "anonid", "paste-and-go");
      let browserLoadedPromise = BrowserTestUtils.browserLoaded(browser, url.replace(/\n/g, ""));
      EventUtils.synthesizeMouseAtCenter(menuitem, {});
      // Using toSource in order to get the newlines escaped:
      info("Paste and go, loading " + url.toSource());
      yield browserLoadedPromise;
      ok(true, "Successfully loaded " + url);
    });
  }
});
