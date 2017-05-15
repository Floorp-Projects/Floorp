// This test is used to check copy and paste in editable areas to ensure that non-text
// types (html and images) are copied to and pasted from the clipboard properly.

var testPage = "<body style='margin: 0'>" +
               "  <img id='img' tabindex='1' src='http://example.org/browser/browser/base/content/test/general/moz.png'>" +
               "  <div id='main' contenteditable='true'>Test <b>Bold</b> After Text</div>" +
               "</body>";

add_task(async function() {
  let tab = BrowserTestUtils.addTab(gBrowser);
  let browser = gBrowser.getBrowserForTab(tab);

  gBrowser.selectedTab = tab;

  await promiseTabLoadEvent(tab, "data:text/html," + escape(testPage));
  await SimpleTest.promiseFocus(browser.contentWindowAsCPOW);

  const modifier = (navigator.platform.indexOf("Mac") >= 0) ?
                   Components.interfaces.nsIDOMWindowUtils.MODIFIER_META :
                   Components.interfaces.nsIDOMWindowUtils.MODIFIER_CONTROL;

  // On windows, HTML clipboard includes extra data.
  // The values are from widget/windows/nsDataObj.cpp.
  const htmlPrefix = (navigator.platform.indexOf("Win") >= 0) ? "<html><body>\n<!--StartFragment-->" : "";
  const htmlPostfix = (navigator.platform.indexOf("Win") >= 0) ? "<!--EndFragment-->\n</body>\n</html>" : "";

  await ContentTask.spawn(browser, { modifier, htmlPrefix, htmlPostfix }, async function(arg) {
    var doc = content.document;
    var main = doc.getElementById("main");
    main.focus();

    const utils = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                         .getInterface(Components.interfaces.nsIDOMWindowUtils);

    function sendKey(key) {
      if (utils.sendKeyEvent("keydown", key, 0, arg.modifier)) {
        utils.sendKeyEvent("keypress", key, key.charCodeAt(0), arg.modifier);
      }
      utils.sendKeyEvent("keyup", key, 0, arg.modifier);
    }

    // Select an area of the text.
    let selection = doc.getSelection();
    selection.modify("move", "left", "line");
    selection.modify("move", "right", "character");
    selection.modify("move", "right", "character");
    selection.modify("move", "right", "character");
    selection.modify("extend", "right", "word");
    selection.modify("extend", "right", "word");

    await new Promise((resolve, reject) => {
      addEventListener("copy", function copyEvent(event) {
        removeEventListener("copy", copyEvent, true);
        // The data is empty as the selection is copied during the event default phase.
        Assert.equal(event.clipboardData.mozItemCount, 0, "Zero items on clipboard");
        resolve();
      }, true)

      sendKey("c");
    });

    selection.modify("move", "right", "line");

    await new Promise((resolve, reject) => {
      addEventListener("paste", function copyEvent(event) {
        removeEventListener("paste", copyEvent, true);
        let clipboardData = event.clipboardData;
        Assert.equal(clipboardData.mozItemCount, 1, "One item on clipboard");
        Assert.equal(clipboardData.types.length, 2, "Two types on clipboard");
        Assert.equal(clipboardData.types[0], "text/html", "text/html on clipboard");
        Assert.equal(clipboardData.types[1], "text/plain", "text/plain on clipboard");
        Assert.equal(clipboardData.getData("text/html"), arg.htmlPrefix +
          "t <b>Bold</b>" + arg.htmlPostfix, "text/html value");
        Assert.equal(clipboardData.getData("text/plain"), "t Bold", "text/plain value");
        resolve();
      }, true)
      sendKey("v");
    });

    Assert.equal(main.innerHTML, "Test <b>Bold</b> After Textt <b>Bold</b>", "Copy and paste html");

    selection.modify("extend", "left", "word");
    selection.modify("extend", "left", "word");
    selection.modify("extend", "left", "character");

    await new Promise((resolve, reject) => {
      addEventListener("cut", function copyEvent(event) {
        removeEventListener("cut", copyEvent, true);
        event.clipboardData.setData("text/plain", "Some text");
        event.clipboardData.setData("text/html", "<i>Italic</i> ");
        selection.deleteFromDocument();
        event.preventDefault();
        resolve();
      }, true)
      sendKey("x");
    });

    selection.modify("move", "left", "line");

    await new Promise((resolve, reject) => {
      addEventListener("paste", function copyEvent(event) {
        removeEventListener("paste", copyEvent, true);
        let clipboardData = event.clipboardData;
        Assert.equal(clipboardData.mozItemCount, 1, "One item on clipboard 2");
        Assert.equal(clipboardData.types.length, 2, "Two types on clipboard 2");
        Assert.equal(clipboardData.types[0], "text/html", "text/html on clipboard 2");
        Assert.equal(clipboardData.types[1], "text/plain", "text/plain on clipboard 2");
        Assert.equal(clipboardData.getData("text/html"), arg.htmlPrefix +
          "<i>Italic</i> " + arg.htmlPostfix, "text/html value 2");
        Assert.equal(clipboardData.getData("text/plain"), "Some text", "text/plain value 2");
        resolve();
      }, true)
      sendKey("v");
    });

    Assert.equal(main.innerHTML, "<i>Italic</i> Test <b>Bold</b> After<b></b>",
      "Copy and paste html 2");
  });

  // Next, check that the Copy Image command works.

  // The context menu needs to be opened to properly initialize for the copy
  // image command to run.
  let contextMenu = document.getElementById("contentAreaContextMenu");
  let contextMenuShown = promisePopupShown(contextMenu);
  BrowserTestUtils.synthesizeMouseAtCenter("#img", { type: "contextmenu", button: 2 }, gBrowser.selectedBrowser);
  await contextMenuShown;

  document.getElementById("context-copyimage-contents").doCommand();

  contextMenu.hidePopup();
  await promisePopupHidden(contextMenu);

  // Focus the content again
  await SimpleTest.promiseFocus(browser.contentWindowAsCPOW);

  await ContentTask.spawn(browser, { modifier, htmlPrefix, htmlPostfix }, async function(arg) {
    var doc = content.document;
    var main = doc.getElementById("main");
    main.focus();

    await new Promise((resolve, reject) => {
      addEventListener("paste", function copyEvent(event) {
        removeEventListener("paste", copyEvent, true);
        let clipboardData = event.clipboardData;

        // DataTransfer doesn't support the image types yet, so only text/html
        // will be present.
        if (clipboardData.getData("text/html") !== arg.htmlPrefix +
            '<img id="img" tabindex="1" src="http://example.org/browser/browser/base/content/test/general/moz.png">' +
            arg.htmlPostfix) {
          reject("Clipboard Data did not contain an image, was " + clipboardData.getData("text/html"));
        }
        resolve();
      }, true)

      const utils = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                           .getInterface(Components.interfaces.nsIDOMWindowUtils);

      if (utils.sendKeyEvent("keydown", "v", 0, arg.modifier)) {
        utils.sendKeyEvent("keypress", "v", "v".charCodeAt(0), arg.modifier);
      }
      utils.sendKeyEvent("keyup", "v", 0, arg.modifier);
    });

    // The new content should now include an image.
    Assert.equal(main.innerHTML, '<i>Italic</i> <img id="img" tabindex="1" ' +
      'src="http://example.org/browser/browser/base/content/test/general/moz.png">' +
      "Test <b>Bold</b> After<b></b>", "Paste after copy image");
  });

  gBrowser.removeCurrentTab();
});

