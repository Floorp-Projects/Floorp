"use strict";

const PAGE =
  "http://example.com/browser/browser/base/content/test/general/page_style_sample.html";

/*
 * Test that the right stylesheets do (and others don't) show up
 * in the page style menu.
 */
add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank",
    false
  );
  let browser = tab.linkedBrowser;
  await BrowserTestUtils.loadURI(browser, PAGE);
  await promiseStylesheetsLoaded(tab, 17);

  let menupopup = document.getElementById("pageStyleMenu").menupopup;
  gPageStyleMenu.fillPopup(menupopup);

  let menuitems = [];
  let current = menupopup.querySelector("menuseparator");
  while (current.nextElementSibling) {
    current = current.nextElementSibling;
    menuitems.push(current);
  }

  let items = menuitems.map(el => ({
    label: el.getAttribute("label"),
    checked: el.getAttribute("checked") == "true",
  }));

  let validLinks = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [items],
    function(contentItems) {
      let contentValidLinks = 0;
      for (let el of content.document.querySelectorAll("link, style")) {
        var title = el.getAttribute("title");
        var rel = el.getAttribute("rel");
        var media = el.getAttribute("media");
        var idstring =
          el.nodeName +
          " " +
          (title ? title : "without title and") +
          ' with rel="' +
          rel +
          '"' +
          (media ? ' and media="' + media + '"' : "");

        var item = contentItems.filter(aItem => aItem.label == title);
        var found = item.length == 1;
        var checked = found && item[0].checked;

        switch (el.getAttribute("data-state")) {
          case "0":
            ok(!found, idstring + " should not show up in page style menu");
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
            throw new Error(
              "data-state attribute is missing or has invalid value"
            );
        }
      }
      return contentValidLinks;
    }
  );

  ok(menuitems.length, "At least one item in the menu");
  is(menuitems.length, validLinks, "all valid links found");

  function getRootColor() {
    return SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
      return content.document.defaultView.getComputedStyle(content.document.documentElement).color;
    });
  }

  is(await getRootColor(), "rgb(0, 255, 0)", "Root should be lime (styles should apply)");

  let disableStyles = document.getElementById("menu_pageStyleNoStyle");
  let defaultStyles = document.getElementById("menu_pageStylePersistentOnly");
  let otherStyles = menuitems[menuitems.length - 1];

  // Assert that the menu works as expected.
  disableStyles.click();

  await TestUtils.waitForCondition(async function() {
    let color = await getRootColor();
    return color != "rgb(0, 255, 0)" && color != "rgb(0, 0, 255)";
  }, "ensuring disabled styles work");

  otherStyles.click();

  await TestUtils.waitForCondition(async function() {
    let color = await getRootColor();
    return color == "rgb(0, 0, 255)";
  }, "ensuring alternate styles work. clicking on: " + otherStyles.label);

  defaultStyles.click();

  info("ensuring default styles work");
  await TestUtils.waitForCondition(async function() {
    let color = await getRootColor();
    return color == "rgb(0, 255, 0)";
  }, "ensuring default styles work");

  BrowserTestUtils.removeTab(tab);
});
