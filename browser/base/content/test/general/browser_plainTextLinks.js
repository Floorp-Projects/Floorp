function testExpected(expected, msg) {
  is(document.getElementById("context-openlinkincurrent").hidden, expected, msg);
}

function testLinkExpected(expected, msg) {
  is(gContextMenu.linkURL, expected, msg);
}

add_task(async function() {
  const url = "data:text/html;charset=UTF-8,Test For Non-Hyperlinked url selection";
  await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  await SimpleTest.promiseFocus(gBrowser.selectedBrowser);

  // Initial setup of the content area.
  await ContentTask.spawn(gBrowser.selectedBrowser, { }, async function(arg) {
    let doc = content.document;
    let range = doc.createRange();
    let selection = content.getSelection();

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

    function setSelection(el1, el2, index1, index2) {
      while (el1.nodeType != el1.TEXT_NODE)
        el1 = el1.firstChild;
      while (el2.nodeType != el1.TEXT_NODE)
        el2 = el2.firstChild;

      selection.removeAllRanges();
      range.setStart(el1, index1);
      range.setEnd(el2, index2);
      selection.addRange(range);

      return range;
    }

    // Each of these tests creates a selection and returns a range within it.
    content.tests = [
      () => setSelection(span1.firstChild, span2.firstChild, 0, 11),
      () => setSelection(span1.firstChild, span2.firstChild, 7, 11),
      () => setSelection(span1.firstChild, span2.firstChild, 8, 11),
      () => setSelection(span2.firstChild, span2.firstChild, 0, 11),
      () => setSelection(span2.firstChild, span2.firstChild, 11, 23),
      () => setSelection(span2.firstChild, span2.firstChild, 0, 10),
      () => setSelection(span2.firstChild, span3.firstChild, 12, 7),
      () => setSelection(span2.firstChild, span2.firstChild, 12, 19),
      () => setSelection(p1.firstChild, p1.firstChild, 0, 15),
      () => setSelection(p1.firstChild, p1.firstChild, 16, 31),
      () => setSelection(p2.firstChild, p2.firstChild, 0, 14),
      () => {
        selection.selectAllChildren(div2);
        return selection.getRangeAt(0);
      },
      () => {
        selection.selectAllChildren(span4);
        return selection.getRangeAt(0);
      },
      () => {
        mainDiv.innerHTML = "(open-suse.ru)";
        return setSelection(mainDiv, mainDiv, 1, 13);
      },
      () => setSelection(mainDiv, mainDiv, 1, 14)
    ];
  });

  let checks = [
    () => testExpected(false, "The link context menu should show for http://www.example.com"),
    () => testExpected(false, "The link context menu should show for www.example.com"),
    () => testExpected(true, "The link context menu should not show for ww.example.com"),
    () => {
      testExpected(false, "The link context menu should show for example.com");
      testLinkExpected("http://example.com/", "url for example.com selection should not prepend www");
    },
    () => testExpected(false, "The link context menu should show for example.com"),
    () => testExpected(true, "Link options should not show for selection that's not at a word boundary"),
    () => testExpected(true, "Link options should not show for selection that has whitespace"),
    () => testExpected(true, "Link options should not show unless a url is selected"),
    () => testExpected(true, "Link options should not show for mailto: links"),
    () => {
      testExpected(false, "Link options should show for ftp.example.com");
      testLinkExpected("http://ftp.example.com/", "ftp.example.com should be preceeded with http://");
    },
    () => testExpected(false, "Link options should show for www.example.com  "),
    () => testExpected(false, "Link options should show for triple-click selections"),
    () => testLinkExpected("http://www.example.com/", "Linkified text should open the correct link"),
    () => {
      testExpected(false, "Link options should show for open-suse.ru");
      testLinkExpected("http://open-suse.ru/", "Linkified text should open the correct link");
    },
    () => testExpected(true, "Link options should not show for 'open-suse.ru)'")
  ];

  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");

  for (let testid = 0; testid < checks.length; testid++) {
    let menuPosition = await ContentTask.spawn(gBrowser.selectedBrowser, { testid }, async function(arg) {
      let range = content.tests[arg.testid]();

      // Get the range of the selection and determine its coordinates. These
      // coordinates will be returned to the parent process and the context menu
      // will be opened at that location.
      let rangeRect = range.getBoundingClientRect();
      return [rangeRect.x + 3, rangeRect.y + 3];
    });

    // Trigger a mouse event until we receive the popupshown event.
    let sawPopup = false;
    let popupShownPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popupshown", false, () => {
      sawPopup = true;
      return true;
    });
    while (!sawPopup) {
      await BrowserTestUtils.synthesizeMouseAtPoint(menuPosition[0], menuPosition[1],
            { type: "contextmenu", button: 2 }, gBrowser.selectedBrowser);
      if (!sawPopup) {
        await new Promise(r => setTimeout(r, 100));
      }
    }
    await popupShownPromise;

    checks[testid]();

    // On Linux non-e10s it's possible the menu was closed by a focus-out event
    // on the window. Work around this by calling hidePopup only if the menu
    // hasn't been closed yet. See bug 1352709 comment 36.
    if (contentAreaContextMenu.state === "closed") {
      continue;
    }

    let popupHiddenPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popuphidden");
    contentAreaContextMenu.hidePopup();
    await popupHiddenPromise;
  }

  gBrowser.removeCurrentTab();
});

