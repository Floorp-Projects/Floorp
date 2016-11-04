"use strict";

/**
 * Stylesheets are updated for a browser after the pageshow event.
 * This helper function returns a Promise that waits for that pageshow
 * event, and then resolves on the next tick to ensure that gPageStyleMenu
 * has had a chance to update the stylesheets.
 *
 * @param browser
 *        The <xul:browser> to wait for.
 * @return Promise
 */
function promiseStylesheetsUpdated(browser) {
  return ContentTask.spawn(browser, { PAGE }, function*(args) {
    return new Promise((resolve) => {
      addEventListener("pageshow", function onPageShow(e) {
        if (e.target.location == args.PAGE) {
          removeEventListener("pageshow", onPageShow);
          content.setTimeout(resolve, 0);
        }
      });
    })
  });
}

const PAGE = "http://example.com/browser/browser/base/content/test/general/page_style_sample.html";

/*
 * Test that the right stylesheets do (and others don't) show up
 * in the page style menu.
 */
add_task(function*() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank", false);
  let browser = tab.linkedBrowser;
  yield BrowserTestUtils.loadURI(browser, PAGE);
  yield promiseStylesheetsUpdated(browser);

  let menupopup = document.getElementById("pageStyleMenu").menupopup;
  gPageStyleMenu.fillPopup(menupopup);

  var items = [];
  var current = menupopup.getElementsByTagName("menuseparator")[0];
  while (current.nextSibling) {
    current = current.nextSibling;
    items.push(current);
  }

  items = items.map(el => ({
    label: el.getAttribute("label"),
    checked: el.getAttribute("checked") == "true",
  }));

  let validLinks = yield ContentTask.spawn(gBrowser.selectedBrowser, items, function(contentItems) {
    let contentValidLinks = 0;
    Array.forEach(content.document.querySelectorAll("link, style"), function (el) {
      var title = el.getAttribute("title");
      var rel = el.getAttribute("rel");
      var media = el.getAttribute("media");
      var idstring = el.nodeName + " " + (title ? title : "without title and") +
                     " with rel=\"" + rel + "\"" +
                     (media ? " and media=\"" + media + "\"" : "");

      var item = contentItems.filter(aItem => aItem.label == title);
      var found = item.length == 1;
      var checked = found && item[0].checked;

      switch (el.getAttribute("data-state")) {
        case "0":
          ok(!found, idstring + " should not show up in page style menu");
          break;
        case "0-todo":
          contentValidLinks++;
          todo(!found, idstring + " should not show up in page style menu");
          ok(!checked, idstring + " should not be selected");
          break;
        case "1":
          contentValidLinks++;
          ok(found, idstring + " should show up in page style menu");
          ok(!checked, idstring + " should not be selected");
          break;
        case "2":
          contentValidLinks++;
          ok(found, idstring + " should show up in page style menu");
          ok(checked, idstring + " should be selected");
          break;
        default:
          throw "data-state attribute is missing or has invalid value";
      }
    });
    return contentValidLinks;
  });

  ok(items.length, "At least one item in the menu");
  is(items.length, validLinks, "all valid links found");

  yield BrowserTestUtils.removeTab(tab);
});
