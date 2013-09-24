let doc, range, selection;
function setSelection(el1, el2, index1, index2) {
  while (el1.nodeType != Node.TEXT_NODE)
    el1 = el1.firstChild;
  while (el2.nodeType != Node.TEXT_NODE)
    el2 = el2.firstChild;

  selection.removeAllRanges();
  range.setStart(el1, index1);
  range.setEnd(el2, index2);
  selection.addRange(range);
}

function initContextMenu(aNode) {
  document.popupNode = aNode;
  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
  let contextMenu = new nsContextMenu(contentAreaContextMenu);
  return contextMenu;
}

function testExpected(expected, msg, aNode) {
  let popupNode = aNode || doc.getElementsByTagName("DIV")[0];
  initContextMenu(popupNode);
  let linkMenuItem = document.getElementById("context-openlinkincurrent");
  is(linkMenuItem.hidden, expected, msg);
}

function testLinkExpected(expected, msg, aNode) {
  let popupNode = aNode || doc.getElementsByTagName("DIV")[0];
  let contextMenu = initContextMenu(popupNode);
  is(contextMenu.linkURL, expected, msg);
}

function runSelectionTests() {
  let mainDiv = doc.createElement("div");
  let div = doc.createElement("div");
  let div2 = doc.createElement("div");
  let span1 = doc.createElement("span");
  let span2 = doc.createElement("span");
  let span3 = doc.createElement("span");
  let span4 = doc.createElement("span");
  let p1 = doc.createElement("p");
  let p2 = doc.createElement("p");
  span1.textContent = "http://index.";
  span2.textContent = "example.com example.com";
  span3.textContent = " - Test";
  span4.innerHTML = "<a href='http://www.example.com'>http://www.example.com/example</a>";
  p1.textContent = "mailto:test.com ftp.example.com";
  p2.textContent = "example.com   -";
  div.appendChild(span1);
  div.appendChild(span2);
  div.appendChild(span3);
  div.appendChild(span4);
  div.appendChild(p1);
  div.appendChild(p2);
  let p3 = doc.createElement("p");
  p3.textContent = "main.example.com";
  div2.appendChild(p3);
  mainDiv.appendChild(div);
  mainDiv.appendChild(div2);
  doc.body.appendChild(mainDiv);
  setSelection(span1.firstChild, span2.firstChild, 0, 11);
  testExpected(false, "The link context menu should show for http://www.example.com");
  setSelection(span1.firstChild, span2.firstChild, 7, 11);
  testExpected(false, "The link context menu should show for www.example.com");
  setSelection(span1.firstChild, span2.firstChild, 8, 11);
  testExpected(true, "The link context menu should not show for ww.example.com");
  setSelection(span2.firstChild, span2.firstChild, 0, 11);
  testExpected(false, "The link context menu should show for example.com");
  testLinkExpected("http://example.com/", "url for example.com selection should not prepend www");
  setSelection(span2.firstChild, span2.firstChild, 11, 23);
  testExpected(false, "The link context menu should show for example.com");
  setSelection(span2.firstChild, span2.firstChild, 0, 10);
  testExpected(true, "Link options should not show for selection that's not at a word boundary");
  setSelection(span2.firstChild, span3.firstChild, 12, 7);
  testExpected(true, "Link options should not show for selection that has whitespace");
  setSelection(span2.firstChild, span2.firstChild, 12, 19);
  testExpected(true, "Link options should not show unless a url is selected");
  setSelection(p1.firstChild, p1.firstChild, 0, 15);
  testExpected(true, "Link options should not show for mailto: links");
  setSelection(p1.firstChild, p1.firstChild, 16, 31);
  testExpected(false, "Link options should show for ftp.example.com");
  testLinkExpected("ftp://ftp.example.com/", "ftp.example.com should be preceeded with ftp://");
  setSelection(p2.firstChild, p2.firstChild, 0, 14);
  testExpected(false, "Link options should show for www.example.com  ");
  selection.selectAllChildren(div2);
  testExpected(false, "Link options should show for triple-click selections");
  selection.selectAllChildren(span4);
  testLinkExpected("http://www.example.com/", "Linkified text should open the correct link", span4.firstChild);

  mainDiv.innerHTML = "(open-suse.ru)";
  setSelection(mainDiv, mainDiv, 1, 13);
  testExpected(false, "Link options should show for open-suse.ru");
  testLinkExpected("http://open-suse.ru/", "Linkified text should open the correct link");
  setSelection(mainDiv, mainDiv, 1, 14);
  testExpected(true, "Link options should not show for 'open-suse.ru)'");

  gBrowser.removeCurrentTab();
  finish();
}

function test() {
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    doc = content.document;
    range = doc.createRange();
    selection = content.getSelection();
    waitForFocus(runSelectionTests, content);
  }, true);

  content.location =
    "data:text/html;charset=UTF-8,Test For Non-Hyperlinked url selection";
}
