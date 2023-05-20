// This test is used to check copy and paste in editable areas to ensure that non-text
// types (html and images) are copied to and pasted from the clipboard properly.

var testPage =
  "<body style='margin: 0'>" +
  "  <img id='img' tabindex='1' src='http://example.org/browser/browser/base/content/test/general/moz.png'>" +
  "  <div id='main' contenteditable='true'>Test <b>Bold</b> After Text</div>" +
  "</body>";

add_task(async function () {
  let tab = BrowserTestUtils.addTab(gBrowser);
  let browser = gBrowser.getBrowserForTab(tab);

  gBrowser.selectedTab = tab;

  await promiseTabLoadEvent(tab, "data:text/html," + escape(testPage));
  await SimpleTest.promiseFocus(browser);

  function sendKey(key, code) {
    return BrowserTestUtils.synthesizeKey(
      key,
      { code, accelKey: true },
      browser
    );
  }

  // On windows, HTML clipboard includes extra data.
  // The values are from widget/windows/nsDataObj.cpp.
  const htmlPrefix = navigator.platform.includes("Win")
    ? "<html><body>\n<!--StartFragment-->"
    : "";
  const htmlPostfix = navigator.platform.includes("Win")
    ? "<!--EndFragment-->\n</body>\n</html>"
    : "";

  await SpecialPowers.spawn(browser, [], () => {
    var doc = content.document;
    var main = doc.getElementById("main");
    main.focus();

    // Select an area of the text.
    let selection = doc.getSelection();
    selection.modify("move", "left", "line");
    selection.modify("move", "right", "character");
    selection.modify("move", "right", "character");
    selection.modify("move", "right", "character");
    selection.modify("extend", "right", "word");
    selection.modify("extend", "right", "word");
  });

  // The data is empty as the selection was copied during the event default phase.
  let copyEventPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "copy",
    false,
    event => {
      return event.clipboardData.mozItemCount == 0;
    }
  );
  await SpecialPowers.spawn(browser, [], () => {});
  await sendKey("c");
  await copyEventPromise;

  let pastePromise = SpecialPowers.spawn(
    browser,
    [htmlPrefix, htmlPostfix],
    (htmlPrefixChild, htmlPostfixChild) => {
      let selection = content.document.getSelection();
      selection.modify("move", "right", "line");

      return new Promise((resolve, reject) => {
        content.addEventListener(
          "paste",
          event => {
            let clipboardData = event.clipboardData;
            Assert.equal(
              clipboardData.mozItemCount,
              1,
              "One item on clipboard"
            );
            Assert.equal(
              clipboardData.types.length,
              2,
              "Two types on clipboard"
            );
            Assert.equal(
              clipboardData.types[0],
              "text/html",
              "text/html on clipboard"
            );
            Assert.equal(
              clipboardData.types[1],
              "text/plain",
              "text/plain on clipboard"
            );
            Assert.equal(
              clipboardData.getData("text/html"),
              htmlPrefixChild + "t <b>Bold</b>" + htmlPostfixChild,
              "text/html value"
            );
            Assert.equal(
              clipboardData.getData("text/plain"),
              "t Bold",
              "text/plain value"
            );
            resolve();
          },
          { capture: true, once: true }
        );
      });
    }
  );

  await SpecialPowers.spawn(browser, [], () => {});

  await sendKey("v");
  await pastePromise;

  let copyPromise = SpecialPowers.spawn(browser, [], () => {
    var main = content.document.getElementById("main");

    Assert.equal(
      main.innerHTML,
      "Test <b>Bold</b> After Textt <b>Bold</b>",
      "Copy and paste html"
    );

    let selection = content.document.getSelection();
    selection.modify("extend", "left", "word");
    selection.modify("extend", "left", "word");
    selection.modify("extend", "left", "character");

    return new Promise((resolve, reject) => {
      content.addEventListener(
        "cut",
        event => {
          event.clipboardData.setData("text/plain", "Some text");
          event.clipboardData.setData("text/html", "<i>Italic</i> ");
          selection.deleteFromDocument();
          event.preventDefault();
          resolve();
        },
        { capture: true, once: true }
      );
    });
  });

  await SpecialPowers.spawn(browser, [], () => {});

  await sendKey("x");
  await copyPromise;

  pastePromise = SpecialPowers.spawn(
    browser,
    [htmlPrefix, htmlPostfix],
    (htmlPrefixChild, htmlPostfixChild) => {
      let selection = content.document.getSelection();
      selection.modify("move", "left", "line");

      return new Promise((resolve, reject) => {
        content.addEventListener(
          "paste",
          event => {
            let clipboardData = event.clipboardData;
            Assert.equal(
              clipboardData.mozItemCount,
              1,
              "One item on clipboard 2"
            );
            Assert.equal(
              clipboardData.types.length,
              2,
              "Two types on clipboard 2"
            );
            Assert.equal(
              clipboardData.types[0],
              "text/html",
              "text/html on clipboard 2"
            );
            Assert.equal(
              clipboardData.types[1],
              "text/plain",
              "text/plain on clipboard 2"
            );
            Assert.equal(
              clipboardData.getData("text/html"),
              htmlPrefixChild + "<i>Italic</i> " + htmlPostfixChild,
              "text/html value 2"
            );
            Assert.equal(
              clipboardData.getData("text/plain"),
              "Some text",
              "text/plain value 2"
            );
            resolve();
          },
          { capture: true, once: true }
        );
      });
    }
  );

  await SpecialPowers.spawn(browser, [], () => {});

  await sendKey("v");
  await pastePromise;

  await SpecialPowers.spawn(browser, [], () => {
    var main = content.document.getElementById("main");
    Assert.equal(
      main.innerHTML,
      "<i>Italic</i> Test <b>Bold</b> After<b></b>",
      "Copy and paste html 2"
    );
  });

  // Next, check that the Copy Image command works.

  // The context menu needs to be opened to properly initialize for the copy
  // image command to run.
  let contextMenu = document.getElementById("contentAreaContextMenu");
  let contextMenuShown = promisePopupShown(contextMenu);
  BrowserTestUtils.synthesizeMouseAtCenter(
    "#img",
    { type: "contextmenu", button: 2 },
    gBrowser.selectedBrowser
  );
  await contextMenuShown;

  document.getElementById("context-copyimage-contents").doCommand();

  contextMenu.hidePopup();
  await promisePopupHidden(contextMenu);

  // Focus the content again
  await SimpleTest.promiseFocus(browser);

  pastePromise = SpecialPowers.spawn(
    browser,
    [htmlPrefix, htmlPostfix],
    (htmlPrefixChild, htmlPostfixChild) => {
      var doc = content.document;
      var main = doc.getElementById("main");
      main.focus();

      return new Promise((resolve, reject) => {
        content.addEventListener(
          "paste",
          event => {
            let clipboardData = event.clipboardData;

            // DataTransfer doesn't support the image types yet, so only text/html
            // will be present.
            if (
              clipboardData.getData("text/html") !==
              htmlPrefixChild +
                '<img id="img" tabindex="1" src="http://example.org/browser/browser/base/content/test/general/moz.png">' +
                htmlPostfixChild
            ) {
              reject(
                "Clipboard Data did not contain an image, was " +
                  clipboardData.getData("text/html")
              );
            }
            resolve();
          },
          { capture: true, once: true }
        );
      });
    }
  );

  await SpecialPowers.spawn(browser, [], () => {});
  await sendKey("v");
  await pastePromise;

  // The new content should now include an image.
  await SpecialPowers.spawn(browser, [], () => {
    var main = content.document.getElementById("main");
    Assert.equal(
      main.innerHTML,
      '<i>Italic</i> <img id="img" tabindex="1" ' +
        'src="http://example.org/browser/browser/base/content/test/general/moz.png">' +
        "Test <b>Bold</b> After<b></b>",
      "Paste after copy image"
    );
  });

  gBrowser.removeCurrentTab();
});
