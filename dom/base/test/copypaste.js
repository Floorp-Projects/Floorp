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
  var Ci = SpecialPowers.Ci;
  return SpecialPowers.wrap(window).QueryInterface(Ci.nsIInterfaceRequestor)
                                   .getInterface(Ci.nsIWebNavigation)
                                   .QueryInterface(Ci.nsILoadContext);
}

function testCopyPaste (isXHTML) {
  var suppressUnicodeCheckIfHidden = !!isXHTML;
  var suppressHTMLCheck = !!isXHTML;

  var webnav = SpecialPowers.wrap(window).QueryInterface(SpecialPowers.Ci.nsIInterfaceRequestor)
                     .getInterface(SpecialPowers.Ci.nsIWebNavigation)

  var docShell = webnav.QueryInterface(SpecialPowers.Ci.nsIDocShell);

  var documentViewer = docShell.contentViewer
                               .QueryInterface(SpecialPowers.Ci.nsIContentViewerEdit);

  var clipboard = SpecialPowers.Services.clipboard;

  var textarea = SpecialPowers.wrap(document.getElementById('input'));

  function copySelectionToClipboard(suppressUnicodeCheck) {
    documentViewer.copySelection();
    if (!suppressUnicodeCheck)
      ok(clipboard.hasDataMatchingFlavors(["text/unicode"], 1,1), "check text/unicode");
    if (!suppressHTMLCheck)
      ok(clipboard.hasDataMatchingFlavors(["text/html"], 1,1), "check text/html");
  }
  function clear(node, suppressUnicodeCheck) {
    textarea.blur();
    clipboard.emptyClipboard(1);
    var sel = window.getSelection();
    sel.removeAllRanges();
  }
  function copyToClipboard(node, suppressUnicodeCheck) {
    clear();
    var r = document.createRange();
    r.selectNode(node);
    window.getSelection().addRange(r);
    copySelectionToClipboard(suppressUnicodeCheck);
  }
  function addRange(startNode,startIndex,endNode,endIndex) {
    var sel = window.getSelection();
    var r = document.createRange();
    r.setStart(startNode,startIndex)
    r.setEnd(endNode,endIndex)
    sel.addRange(r);
  }
  function copyRangeToClipboard(startNode,startIndex,endNode,endIndex,suppressUnicodeCheck) {
    clear();
    addRange(startNode,startIndex,endNode,endIndex);
    copySelectionToClipboard(suppressUnicodeCheck);
  }
  function copyChildrenToClipboard(id) {
    clear();
    window.getSelection().selectAllChildren(document.getElementById(id));
    copySelectionToClipboard();
  }
  function getClipboardData(mime) {
    var transferable = SpecialPowers.Cc['@mozilla.org/widget/transferable;1']
                                    .createInstance(SpecialPowers.Ci.nsITransferable);
    transferable.init(getLoadContext());
    transferable.addDataFlavor(mime);
    clipboard.getData(transferable, 1);
    var data = SpecialPowers.createBlankObject();
    transferable.getTransferData(mime, data, {}) ;
    return data;
  }
  function testClipboardValue(mime, expected) {
    if (suppressHTMLCheck && mime == "text/html")
      return null;
    var data = SpecialPowers.wrap(getClipboardData(mime));
    is (data.value == null ? data.value :
        data.value.QueryInterface(SpecialPowers.Ci.nsISupportsString).data,
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
  function testPasteHTML(id, expected) {
    var contentEditable = $(id);
    contentEditable.focus();
    synthesizeKey("v", {accelKey: 1});
    is(contentEditable.innerHTML, expected, id+".innerHtml after the paste");
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
  testSelectionToString(" T     ");
  testClipboardValue("text/unicode", " T     ");
  if (isXHTML) {
    testClipboardValue("text/html", "<div id=\"div5\">\n  T<textarea xmlns=\"http://www.w3.org/1999/xhtml\">     </textarea>\n</div>");
    testInnerHTML("div5", "\n  T<textarea xmlns=\"http://www.w3.org/1999/xhtml\">     </textarea>\n");
  }
  else {
    testClipboardValue("text/html", "<div id=\"div5\">\n  T<textarea>     </textarea>\n</div>");
    testInnerHTML("div5", "\n  T<textarea>     </textarea>\n");
  }
  testPasteText(" T     ");

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

  if (!isXHTML) {
    // ============ copy/paste multi-range selection (bug 1123505)
    // with text start node
    var sel = window.getSelection();
    sel.removeAllRanges();
    var r = document.createRange();
    var ul = $('ul1');
    var parent = ul.parentNode;
    r.setStart(parent, 0);
    r.setEnd(parent.firstChild, 15);  // the end of "Copy..."
    sel.addRange(r);

    r = document.createRange();
    r.setStart(ul, 1);  // before the space inside the UL
    r.setEnd(parent, 2);  // after the UL
    sel.addRange(r);
    copySelectionToClipboard(true);
    testPasteHTML('contentEditable1', 'Copy1then Paste');

    // with text end node
    var sel = window.getSelection();
    sel.removeAllRanges();
    var r = document.createRange();
    var ul = $('ul2');
    var parent = ul.parentNode;
    r.setStart(parent, 0);
    r.setEnd(ul, 1);  // after the space
    sel.addRange(r);

    r = document.createRange();
    r.setStart(parent.childNodes[1], 0);  // the start of "Copy..."
    r.setEnd(parent, 2);
    sel.addRange(r);
    copySelectionToClipboard(true);
    testPasteHTML('contentEditable2', 'Copy2then Paste');

    // with text end node and non-empty start
    var sel = window.getSelection();
    sel.removeAllRanges();
    var r = document.createRange();
    var ul = $('ul3');
    var parent = ul.parentNode;
    r.setStart(parent, 0);
    r.setEnd(ul, 1);  // after the space
    sel.addRange(r);

    r = document.createRange();
    r.setStart(parent.childNodes[1], 0);  // the start of "Copy..."
    r.setEnd(parent, 2);
    sel.addRange(r);
    copySelectionToClipboard(true);
    testPasteHTML('contentEditable3', '<ul id="ul3"><li>\n<br></li></ul>Copy3then Paste');

    // with elements of different depth
    var sel = window.getSelection();
    sel.removeAllRanges();
    var r = document.createRange();
    var div1 = $('div1s');
    var parent = div1.parentNode;
    r.setStart(parent, 0);
    r.setEnd(document.getElementById('div1se1'), 1);  // after the "inner" DIV
    sel.addRange(r);

    r = document.createRange();
    r.setStart(div1.childNodes[1], 0);  // the start of "after"
    r.setEnd(parent, 1);
    sel.addRange(r);
    copySelectionToClipboard(true);
    testPasteHTML('contentEditable4', '<div id="div1s"><div id="div1se1">before</div></div><div id="div1s">after</div>');

    // with elements of different depth, and a text node at the end
    var sel = window.getSelection();
    sel.removeAllRanges();
    var r = document.createRange();
    var div1 = $('div2s');
    var parent = div1.parentNode;
    r.setStart(parent, 0);
    r.setEnd(document.getElementById('div2se1'), 1);  // after the "inner" DIV
    sel.addRange(r);

    r = document.createRange();
    r.setStart(div1.childNodes[1], 0);  // the start of "after"
    r.setEnd(parent, 1);
    sel.addRange(r);
    copySelectionToClipboard(true);
    testPasteHTML('contentEditable5', '<div id="div2s"><div id="div2se1">before</div></div><div id="div2s">after</div>');

    // crash test for bug 1127835
    var e1 = document.getElementById('1127835crash1');
    var e2 = document.getElementById('1127835crash2');
    var e3 = document.getElementById('1127835crash3');
    var t1 = e1.childNodes[0];
    var t3 = e3.childNodes[0];
    
    var sel = window.getSelection();
    sel.removeAllRanges();
  
    var r = document.createRange();
    r.setStart(t1, 1);
    r.setEnd(e2, 0);
    sel.addRange(r);
  
    r = document.createRange();
    r.setStart(e2, 1);
    r.setEnd(t3, 0);
    sel.addRange(r);
    copySelectionToClipboard(true);
    testPasteHTML('contentEditable6', '<span id="1127835crash1"></span><div id="1127835crash2"><div>\n</div></div><br>');
  }

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

  if (!isXHTML) {
    // ============ spanning multiple rows

    copyRangeToClipboard($("tr2"),0,$("tr3"),0);
    testClipboardValue("text/unicode", "1\t2\n3\t4\n");
    testClipboardValue("text/html", '<table><tbody><tr id="tr2"><tr id="tr2"><td>1</td><td>2</td></tr><tr><td>3</td><td>4</td></tr><tr id="tr3"></tr></tr></tbody></table>');

    // ============ spanning multiple rows in multi-range selection

    clear();
    addRange($("tr2"),0,$("tr2"),2);
    addRange($("tr3"),0,$("tr3"),2);
    copySelectionToClipboard();
    testClipboardValue("text/unicode", "1\t2\n5\t6");
    testClipboardValue("text/html", '<table><tbody><tr id="tr2"><td>1</td><td>2</td></tr><tr id="tr3"><td>5</td><td>6</td></tr></tbody></table>');
  }

  // ============ manipulating Selection in oncopy

  copyRangeToClipboard($("div11").childNodes[0],0, $("div11").childNodes[1],2);
  testClipboardValue("text/unicode", "Xdiv11");
  testClipboardValue("text/html", "<div><p>X<span>div</span>11</p></div>");
  setTimeout(function(){testSelectionToString("div11")},0);

  setTimeout(function(){
    copyRangeToClipboard($("div12").childNodes[0],0, $("div12").childNodes[1],2);
    testClipboardValue("text/unicode", "Xdiv12");
    testClipboardValue("text/html", "<div><p>X<span>div</span>12</p></div>");
    setTimeout(function(){ 
      testSelectionToString("div12"); 
      setTimeout(SimpleTest.finish,0);
    },0);
  },0);
}
