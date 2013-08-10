/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function modifySelection(s) {
  var g = window.getSelection();
  var l = g.getRangeAt(0);
  var d = document.createElement("p");
  d.innerHTML = s;
  d.appendChild(l.cloneContents());

  var e = document.createElement("div");
  document.body.appendChild(e);
  e.appendChild(d);
  var a = document.createRange();
  a.selectNode(d);
  g.removeAllRanges();
  g.addRange(a);
  window.setTimeout(function () {
      e.parentNode.removeChild(e);
      g.removeAllRanges();
      g.addRange(l);
  }, 0)
}

function getLoadContext() {
  var Ci = SpecialPowers.wrap(Components).interfaces;
  return SpecialPowers.wrap(window).QueryInterface(Ci.nsIInterfaceRequestor)
                                   .getInterface(Ci.nsIWebNavigation)
                                   .QueryInterface(Ci.nsILoadContext);
}

function testCopyPaste (isXHTML) {
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  var suppressUnicodeCheckIfHidden = !!isXHTML;
  var suppressHTMLCheck = !!isXHTML;

  var webnav = window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                     .getInterface(Components.interfaces.nsIWebNavigation)

  var docShell = webnav.QueryInterface(Components.interfaces.nsIDocShell);

  var documentViewer = docShell.contentViewer
                               .QueryInterface(Components.interfaces.nsIContentViewerEdit);

  var clipboard = Components.classes["@mozilla.org/widget/clipboard;1"]
                            .getService(Components.interfaces.nsIClipboard);

  var textarea = SpecialPowers.wrap(document.getElementById('input'));

  function copySelectionToClipboard(suppressUnicodeCheck) {
    documentViewer.copySelection();
    if (!suppressUnicodeCheck)
      ok(clipboard.hasDataMatchingFlavors(["text/unicode"], 1,1), "check text/unicode");
    if (!suppressHTMLCheck)
      ok(clipboard.hasDataMatchingFlavors(["text/html"], 1,1), "check text/html");
  }
  function copyToClipboard(node, suppressUnicodeCheck) {
    textarea.blur();
    clipboard.emptyClipboard(1);
    var sel = window.getSelection();
    sel.removeAllRanges();
    var r = document.createRange();
    r.selectNode(node);
    window.getSelection().addRange(r);
    copySelectionToClipboard(suppressUnicodeCheck);
  }
  function copyRangeToClipboard(startNode,startIndex,endNode,endIndex,suppressUnicodeCheck) {
    textarea.blur();
    clipboard.emptyClipboard(1);
    var sel = window.getSelection();
    sel.removeAllRanges();
    var r = document.createRange();
    r.setStart(startNode,startIndex)
    r.setEnd(endNode,endIndex)
    window.getSelection().addRange(r);
    copySelectionToClipboard(suppressUnicodeCheck);
  }
  function copyChildrenToClipboard(id) {
    textarea.blur();
    clipboard.emptyClipboard(1);
    window.getSelection().selectAllChildren(document.getElementById(id));
    copySelectionToClipboard();
  }
  function getClipboardData(mime) {
    var transferable = Components.classes['@mozilla.org/widget/transferable;1']
                                 .createInstance(Components.interfaces.nsITransferable);
    transferable.init(getLoadContext());
    transferable.addDataFlavor(mime);
    clipboard.getData(transferable, 1);
    var data = {};
    transferable.getTransferData(mime, data, {}) ;
    return data;
  }
  function testClipboardValue(mime, expected) {
    if (suppressHTMLCheck && mime == "text/html")
      return null;
    var data = getClipboardData(mime);
    is (data.value == null ? data.value :
        data.value.QueryInterface(Components.interfaces.nsISupportsString).data,
      expected,
      mime + " value in the clipboard");
    return data.value;
  }
  function testPasteText(expected) {
    textarea.value="";
    textarea.focus();
    textarea.editor.paste(1);
    is(textarea.value, expected, "value of the textarea after the paste");
  }
  function testSelectionToString(expected) {
    is(window.getSelection().toString().replace(/\r\n/g,"\n"), expected, "Selection.toString");
  }
  function testInnerHTML(id, expected) {
    var value = document.getElementById(id).innerHTML;
    is(value, expected, id + ".innerHTML");
  }
  function testEmptyChildren(id) {
    copyChildrenToClipboard(id);
    testSelectionToString("");
    testClipboardValue("text/unicode", null);
    testClipboardValue("text/html", null);
    testPasteText("");
  }

  copyChildrenToClipboard("draggable");
  testSelectionToString("This is a draggable bit of text.");
  testClipboardValue("text/unicode",
                     "This is a draggable bit of text.");
  testClipboardValue("text/html",
                     "<div id=\"draggable\" title=\"title to have a long HTML line\">This is a <em>draggable</em> bit of text.</div>");
  testPasteText("This is a draggable bit of text.");

  copyChildrenToClipboard("alist");
  testSelectionToString(" bla\n\n    foo\n    bar\n\n");
  testClipboardValue("text/unicode", " bla\n\n    foo\n    bar\n\n");
  testClipboardValue("text/html", "<div id=\"alist\">\n    bla\n    <ul>\n      <li>foo</li>\n      \n      <li>bar</li>\n    </ul>\n  </div>");
  testPasteText(" bla\n\n    foo\n    bar\n\n");

  copyChildrenToClipboard("blist");
  testSelectionToString(" mozilla\n\n    foo\n    bar\n\n");
  testClipboardValue("text/unicode", " mozilla\n\n    foo\n    bar\n\n");
  testClipboardValue("text/html", "<div id=\"blist\">\n    mozilla\n    <ol>\n      <li>foo</li>\n      \n      <li>bar</li>\n    </ol>\n  </div>");
  testPasteText(" mozilla\n\n    foo\n    bar\n\n");

  copyChildrenToClipboard("clist");
  testSelectionToString(" mzla\n\n    foo\n        bazzinga!\n    bar\n\n");
  testClipboardValue("text/unicode", " mzla\n\n    foo\n        bazzinga!\n    bar\n\n");
  testClipboardValue("text/html", "<div id=\"clist\">\n    mzla\n    <ul>\n      <li>foo<ul>\n        <li>bazzinga!</li>\n      </ul></li>\n      \n      <li>bar</li>\n    </ul>\n  </div>");
  testPasteText(" mzla\n\n    foo\n        bazzinga!\n    bar\n\n");

  copyChildrenToClipboard("div4");
  testSelectionToString(" Tt t t ");
  testClipboardValue("text/unicode", " Tt t t ");
  if (isXHTML) {
    testClipboardValue("text/html", "<div id=\"div4\">\n  T<textarea xmlns=\"http://www.w3.org/1999/xhtml\">t t t</textarea>\n</div>");
    testInnerHTML("div4", "\n  T<textarea xmlns=\"http://www.w3.org/1999/xhtml\">t t t</textarea>\n");
  }
  else {
    testClipboardValue("text/html", "<div id=\"div4\">\n  T<textarea>t t t</textarea>\n</div>");
    testInnerHTML("div4", "\n  T<textarea>t t t</textarea>\n");
  }
  testPasteText(" Tt t t ");

  copyChildrenToClipboard("div5");
  testSelectionToString(" T ");
  testClipboardValue("text/unicode", " T ");
  if (isXHTML) {
    testClipboardValue("text/html", "<div id=\"div5\">\n  T<textarea xmlns=\"http://www.w3.org/1999/xhtml\">     </textarea>\n</div>");
    testInnerHTML("div5", "\n  T<textarea xmlns=\"http://www.w3.org/1999/xhtml\">     </textarea>\n");
  }
  else {
    testClipboardValue("text/html", "<div id=\"div5\">\n  T<textarea>     </textarea>\n</div>");
    testInnerHTML("div5", "\n  T<textarea>     </textarea>\n");
  }
  testPasteText(" T ");

  copyRangeToClipboard($("div6").childNodes[0],0, $("div6").childNodes[1],1,suppressUnicodeCheckIfHidden);
  testSelectionToString("");
// START Disabled due to bug 564688
if (false) {
  testClipboardValue("text/unicode", "");
  testClipboardValue("text/html", "");
}
// END Disabled due to bug 564688
  testInnerHTML("div6", "div6");

  copyRangeToClipboard($("div7").childNodes[0],0, $("div7").childNodes[0],4,suppressUnicodeCheckIfHidden);
  testSelectionToString("");
// START Disabled due to bug 564688
if (false) {
  testClipboardValue("text/unicode", "");
  testClipboardValue("text/html", "");
}
// END Disabled due to bug 564688
  testInnerHTML("div7", "div7");

  copyRangeToClipboard($("div8").childNodes[0],0, $("div8").childNodes[0],4,suppressUnicodeCheckIfHidden);
  testSelectionToString("");
// START Disabled due to bug 564688
if (false) {
  testClipboardValue("text/unicode", "");
  testClipboardValue("text/html", "");
}
// END Disabled due to bug 564688
  testInnerHTML("div8", "div8");

  copyRangeToClipboard($("div9").childNodes[0],0, $("div9").childNodes[0],4,suppressUnicodeCheckIfHidden);
  testSelectionToString("div9");
  testClipboardValue("text/unicode", "div9");
  testClipboardValue("text/html", "div9");
  testInnerHTML("div9", "div9");

  copyToClipboard($("div10"), suppressUnicodeCheckIfHidden);
  testSelectionToString("");
  testInnerHTML("div10", "div10");

  copyToClipboard($("div10").firstChild, suppressUnicodeCheckIfHidden);
  testSelectionToString("");

  copyRangeToClipboard($("div10").childNodes[0],0, $("div10").childNodes[0],1,suppressUnicodeCheckIfHidden);
  testSelectionToString("");

  copyRangeToClipboard($("div10").childNodes[1],0, $("div10").childNodes[1],1,suppressUnicodeCheckIfHidden);
  testSelectionToString("");

  // ============ copy/paste test from/to a textarea

  var val = "1\n 2\n  3";
  textarea.value=val;
  textarea.select();
  textarea.editor.copy();
  
  textarea.value="";
  textarea.editor.paste(1);
  is(textarea.value, val);
  textarea.value="";

  // ============ NOSCRIPT should not be copied

  copyChildrenToClipboard("div13");
  testSelectionToString("__");
  testClipboardValue("text/unicode", "__");
  testClipboardValue("text/html", "<div id=\"div13\">__</div>");
  testPasteText("__");

  // ============ converting cell boundaries to tabs in tables

  copyToClipboard($("tr1"));
  testClipboardValue("text/unicode", "foo\tbar");

  // ============ manipulating Selection in oncopy

  copyRangeToClipboard($("div11").childNodes[0],0, $("div11").childNodes[1],2);
  testClipboardValue("text/unicode", "Xdiv11");
  testClipboardValue("text/html", "<div><p>X<span>div</span>11</p></div>");
  setTimeout(function(){testSelectionToString("div11")},0);

  setTimeout(function(){
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
    copyRangeToClipboard($("div12").childNodes[0],0, $("div12").childNodes[1],2);
    testClipboardValue("text/unicode", "Xdiv12");
    testClipboardValue("text/html", "<div><p>X<span>div</span>12</p></div>");
    setTimeout(function(){ 
      testSelectionToString("div12"); 
      setTimeout(SimpleTest.finish,0);
    },0);
  },0);
}
