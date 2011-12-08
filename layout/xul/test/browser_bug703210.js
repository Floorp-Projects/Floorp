function test() {
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();

  let doStopPropagation = function (aEvent)
  {
    aEvent.stopPropagation();
  }

  let onPopupShown = function (aEvent)
  {
    is(aEvent.originalTarget.localName, "tooltip", "tooltip is showing");

    let doc = gBrowser.contentDocument;
    let win = gBrowser.contentWindow;
    let p2 = doc.getElementById("p2");
    setTimeout(function () {
      EventUtils.synthesizeMouseAtCenter(p2, { type: "mousemove" }, win); }, 0);
  }

  let onPopupHiding = function (aEvent)
  {
    is(aEvent.originalTarget.localName, "tooltip", "tooltip is hiding");

    let doc = gBrowser.contentDocument;

    doc.removeEventListener("mousemove", doStopPropagation, true);
    doc.removeEventListener("mouseenter", doStopPropagation, true);
    doc.removeEventListener("mouseleave", doStopPropagation, true);
    doc.removeEventListener("mouseover", doStopPropagation, true);
    doc.removeEventListener("mouseout", doStopPropagation, true);
    document.removeEventListener("popupshown", onPopupShown, true);
    document.removeEventListener("popuphiding", onPopupHiding, true);

    gBrowser.removeCurrentTab();
    finish();
  }

  let onLoad = function (aEvent)
  {
    let doc = gBrowser.contentDocument;
    let win = gBrowser.contentWindow;
    let p1 = doc.getElementById("p1");
    let p2 = doc.getElementById("p2");

    EventUtils.synthesizeMouseAtCenter(p2, { type: "mousemove" }, win);

    doc.addEventListener("mousemove", doStopPropagation, true);
    doc.addEventListener("mouseenter", doStopPropagation, true);
    doc.addEventListener("mouseleave", doStopPropagation, true);
    doc.addEventListener("mouseover", doStopPropagation, true);
    doc.addEventListener("mouseout", doStopPropagation, true);
    document.addEventListener("popupshown", onPopupShown, true);
    document.addEventListener("popuphiding", onPopupHiding, true);

    EventUtils.synthesizeMouseAtCenter(p1, { type: "mousemove" }, win);
  }

  gBrowser.selectedBrowser.addEventListener("load",
    function () { setTimeout(onLoad, 0); }, true);

  content.location = "data:text/html," +
    "<p id=\"p1\" title=\"tooltip is here\">This paragraph has a tooltip.</p>" +
    "<p id=\"p2\">This paragraph doesn't have tooltip.</p>";
}
