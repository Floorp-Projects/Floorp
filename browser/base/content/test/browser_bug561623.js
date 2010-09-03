function test() {
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function () {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    let doc = gBrowser.contentDocument;
    let tooltip = document.getElementById("aHTMLTooltip");
    let i = doc.getElementById("i");

    ok(!FillInHTMLTooltip(i),
       "No tooltip should be shown when @title is null");

    i.title = "foo";
    ok(FillInHTMLTooltip(i),
       "A tooltip should be shown when @title is not the empty string");

    i.pattern = "bar";
    ok(FillInHTMLTooltip(i),
       "A tooltip should be shown when @title is not the empty string");

    gBrowser.removeCurrentTab();
    finish();
  }, true);

  content.location = 
    "data:text/html,<!DOCTYPE html><html><body><input id='i'></body></html>";
}

