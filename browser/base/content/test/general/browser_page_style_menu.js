function test() {
  waitForExplicitFinish();

  var tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;
  tab.linkedBrowser.addEventListener("pageshow", function () {
    tab.linkedBrowser.removeEventListener("pageshow", arguments.callee, true);
    checkPageStyleMenu();
  }, true);
  let rootDir = getRootDirectory(gTestPath);
  content.location = rootDir + "page_style_sample.html";
}

function checkPageStyleMenu() {
  var menupopup = document.getElementById("pageStyleMenu")
                          .getElementsByTagName("menupopup")[0];
  gPageStyleMenu.fillPopup(menupopup);

  var items = [];
  var current = menupopup.getElementsByTagName("menuseparator")[0];
  while (current.nextSibling) {
    current = current.nextSibling;
    items.push(current);
  }

  var validLinks = 0;
  Array.forEach(content.document.getElementsByTagName("link"), function (link) {
    var title = link.getAttribute("title");
    var rel = link.getAttribute("rel");
    var media = link.getAttribute("media");
    var idstring = "link " + (title ? title : "without title and") +
                   " with rel=\"" + rel + "\"" +
                   (media ? " and media=\"" + media + "\"" : "");

    var item = items.filter(item => item.getAttribute("label") == title);
    var found = item.length == 1;
    var checked = found && (item[0].getAttribute("checked") == "true");

    switch (link.getAttribute("data-state")) {
      case "0":
        ok(!found, idstring + " does not show up in page style menu");
        break;
      case "0-todo":
        validLinks++;
        todo(!found, idstring + " should not show up in page style menu");
        ok(!checked, idstring + " is not selected");
        break;
      case "1":
        validLinks++;
        ok(found, idstring + " shows up in page style menu");
        ok(!checked, idstring + " is not selected");
        break;
      case "2":
        validLinks++;
        ok(found, idstring + " shows up in page style menu");
        ok(checked, idstring + " is selected");
        break;
      default:
        throw "data-state attribute is missing or has invalid value";
    }
  });

  is(validLinks, items.length, "all valid links found");

  gBrowser.removeCurrentTab();
  finish();
}
