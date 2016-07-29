add_task(function* () {
  requestLongerTimeout(2);

  let testingList = [
    { uri: "data:text/html,<body onload=\"setTimeout(function () { document.getElementById('target').focus(); }, 10);\"><input id='target'></body>",
      tagName: "INPUT", methodName: "focus" },
    { uri: "data:text/html,<body onload=\"setTimeout(function () { document.getElementById('target').select(); }, 10);\"><input id='target'></body>",
      tagName: "INPUT", methodName: "select" },
    { uri: "data:text/html,<body onload=\"setTimeout(function () { document.getElementById('target').focus(); }, 10);\"><a href='about:blank' id='target'>anchor</a></body>",
      tagName: "A", methodName: "focus" },
    { uri: "data:text/html,<body onload=\"setTimeout(function () { document.getElementById('target').focus(); }, 10);\"><button id='target'>button</button></body>",
      tagName: "BUTTON", methodName: "focus" },
    { uri: "data:text/html,<body onload=\"setTimeout(function () { document.getElementById('target').focus(); }, 10);\"><select id='target'><option>item1</option></select></body>",
      tagName: "SELECT", methodName: "focus" },
    { uri: "data:text/html,<body onload=\"setTimeout(function () { document.getElementById('target').focus(); }, 10);\"><textarea id='target'>textarea</textarea></body>",
      tagName: "TEXTAREA", methodName: "focus" },
    { uri: "data:text/html,<body onload=\"setTimeout(function () { document.getElementById('target').select(); }, 10);\"><textarea id='target'>textarea</textarea></body>",
      tagName: "TEXTAREA", methodName: "select" },
    { uri: "data:text/html,<body onload=\"setTimeout(function () { document.getElementById('target').focus(); }, 10);\"><label id='target'><input></label></body>",
      tagName: "INPUT", methodName: "focus of label element" },
    { uri: "data:text/html,<body onload=\"setTimeout(function () { document.getElementById('target').focus(); }, 10);\"><fieldset><legend id='target'>legend</legend><input></fieldset></body>",
      tagName: "INPUT", methodName: "focus of legend element" },
    { uri: "data:text/html,<body onload=\"setTimeout(function () {" +
      "  var element = document.getElementById('target');" +
      "  var event = document.createEvent('MouseEvent');" +
      "  event.initMouseEvent('click', true, true, window," +
      "    1, 0, 0, 0, 0, false, false, false, false, 0, element);" +
      "  element.dispatchEvent(event); }, 10);\">" +
      "<label id='target'><input></label></body>",
      tagName: "INPUT", methodName: "click event on the label element" },
  ];

  if (navigator.platform.indexOf("Mac") == -1) {
    // clicking buttons doesn't focus on mac, so skip this test
    testingList.push(
      { uri: "data:text/html,<body onload=\"setTimeout(function () {" +
        "  var element = document.getElementById('target');" +
        "  var event = document.createEvent('MouseEvent');" +
        "  event.initMouseEvent('mousedown', true, true, window," +
        "    0, 0, 0, 0, 0, false, false, false, false, 0, element);" +
        "  element.dispatchEvent(event); }, 10);\">" +
        "<button id='target'>button</button></body>",
        tagName: "BUTTON", methodName: "mousedown event on the button element" });
  }

  yield BrowserTestUtils.withNewTab("about:blank", function*(bg) {
    yield BrowserTestUtils.withNewTab("about:blank", function*(fg) {
      for (let test of testingList) {
        // Focus the foreground tab's content
        fg.focus();

        // Load the URIs.
        yield BrowserTestUtils.loadURI(bg, test.uri);
        yield BrowserTestUtils.browserLoaded(bg);
        yield BrowserTestUtils.loadURI(fg, test.uri);
        yield BrowserTestUtils.browserLoaded(fg);

        ok(true, "Test1: Both of the tabs are loaded");

        // Confirm that the contents should be able to steal focus from content.
        yield ContentTask.spawn(fg, test, test => {
          return new Promise(res => {
            function f() {
              let e = content.document.activeElement;
              if (e.tagName != test.tagName) {
                setTimeout(f, 10);
              } else {
                is(Services.focus.focusedElement, e,
                   "the foreground tab's " + test.tagName +
                   " element isn't focused by the " + test.methodName +
                   " (Test1: content can steal focus)");
                res();
              }
            }
            f();
          });
        });

        yield ContentTask.spawn(bg, test, test => {
          return new Promise(res => {
            function f() {
              let e = content.document.activeElement;
              if (e.tagName != test.tagName) {
                setTimeout(f, 10);
              } else {
                isnot(Services.focus.focusedElement, e,
                      "the background tab's " + test.tagName +
                      " element is focused by the " + test.methodName +
                      " (Test1: content can steal focus)");
                res();
              }
            }
            f();
          });
        });

        if (fg.isRemoteBrowser) {
          is(Services.focus.focusedElement, fg,
             "Focus should be on the content in the parent process");
        }

        // Focus chrome
        document.getElementById("urlbar").focus();
        let originalFocus = Services.focus.focusedElement;

        // Load about:blank just to make sure that everything works nicely
        yield BrowserTestUtils.loadURI(bg, "about:blank");
        yield BrowserTestUtils.browserLoaded(bg);
        yield BrowserTestUtils.loadURI(fg, "about:blank");
        yield BrowserTestUtils.browserLoaded(fg);

        // Load the URIs.
        yield BrowserTestUtils.loadURI(bg, test.uri);
        yield BrowserTestUtils.browserLoaded(bg);
        yield BrowserTestUtils.loadURI(fg, test.uri);
        yield BrowserTestUtils.browserLoaded(fg);

        ok(true, "Test2: Both of the tabs are loaded");

        // Confirm that the contents should be able to steal focus from content.
        yield ContentTask.spawn(fg, test, test => {
          return new Promise(res => {
            function f() {
              let e = content.document.activeElement;
              if (e.tagName != test.tagName) {
                setTimeout(f, 10);
              } else {
                isnot(Services.focus.focusedElement, e,
                      "the foreground tab's " + test.tagName +
                      " element is focused by the " + test.methodName +
                      " (Test2: content can NOT steal focus)");
                res();
              }
            }
            f();
          });
        });

        yield ContentTask.spawn(bg, test, test => {
          return new Promise(res => {
            function f() {
              let e = content.document.activeElement;
              if (e.tagName != test.tagName) {
                setTimeout(f, 10);
              } else {
                isnot(Services.focus.focusedElement, e,
                      "the background tab's " + test.tagName +
                      " element is focused by the " + test.methodName +
                      " (Test2: content can NOT steal focus)");
                res();
              }
            }
            f();
          });
        });

        is(Services.focus.focusedElement, originalFocus,
           "The parent process's focus has shifted " +
           "(methodName = " + test.methodName + ")" +
           " (Test2: content can NOT steal focus)");
      }
    });
  });
});
