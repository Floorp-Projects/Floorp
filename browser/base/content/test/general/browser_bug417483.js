function test() {
  waitForExplicitFinish();
  
  var htmlContent = "data:text/html, <iframe src='data:text/html,text text'></iframe>";
  gBrowser.addEventListener("pageshow", onPageShow, false);
  gBrowser.loadURI(htmlContent);
}

function onPageShow() {
    gBrowser.removeEventListener("pageshow", onPageShow, false);
    var frame = content.frames[0];
    var sel = frame.getSelection();
    var range = frame.document.createRange();
    var tn = frame.document.body.childNodes[0];
    range.setStart(tn , 4);
    range.setEnd(tn , 5);
    sel.addRange(range);
    frame.focus();
    
    document.popupNode = frame.document.body;
    var contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
    var contextMenu = new nsContextMenu(contentAreaContextMenu);

    ok(document.getElementById("frame-sep").hidden, "'frame-sep' should be hidden if the selection contains only spaces");
    finish();
}
