"use strict";

const PAGE = "http://example.com/browser/browser/base/content/test/general/page_style_sample.html";

/*
 * Test that the right stylesheets do (and others don't) show up
 * in the page style menu.
 */
add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank", false);
  let browser = tab.linkedBrowser;
  await BrowserTestUtils.loadURI(browser, PAGE);
  await promiseStylesheetsUpdated(browser);

  let menupopup = document.getElementById("pageStyleMenu").menupopup;
  gPageStyleMenu.fillPopup(menupopup);

  var items = [];
  var current = menupopup.getElementsByTagName("menuseparator")[0];
  while (current.nextElementSibling) {
    current = current.nextElementSibling;
    items.push(current);
  }

  items = items.map(el => ({
    label: el.getAttribute("label"),
    checked: el.getAttribute("checked") == "true",
  }));

  let validLinks = await ContentTask.spawn(gBrowser.selectedBrowser, items, function(contentItems) {
    let contentValidLinks = 0;
    Array.prototype.forEach.call(content.document.querySelectorAll("link, style"), function(el) {
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
          throw new Error("data-state attribute is missing or has invalid value");
      }
    });
    return contentValidLinks;
  });

  ok(items.length, "At least one item in the menu");
  is(items.length, validLinks, "all valid links found");

  BrowserTestUtils.removeTab(tab);
});
