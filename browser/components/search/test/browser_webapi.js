let ROOT = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");

function AddSearchProvider(...args) {
  return gBrowser.addTab(ROOT + "webapi.html?AddSearchProvider:" + encodeURIComponent(JSON.stringify(args)));
}

function addSearchEngine(...args) {
  return gBrowser.addTab(ROOT + "webapi.html?addSearchEngine:" + encodeURIComponent(JSON.stringify(args)));
}

function promiseDialogOpened() {
  return new Promise((resolve, reject) => {
    Services.wm.addListener({
      onOpenWindow: function(xulWin) {
        Services.wm.removeListener(this);

        let win = xulWin.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDOMWindow);
        waitForFocus(() => {
          if (win.location == "chrome://global/content/commonDialog.xul")
            resolve(win)
          else
            reject();
        }, win);
      }
    });
  });
}

add_task(function* test_working_AddSearchProvider() {
  gBrowser.selectedTab = AddSearchProvider(ROOT + "testEngine.xml");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "confirmEx", "Should see the confirmation dialog.");
  is(dialog.args.text, "Add \"Foo\" to the list of engines available in the search bar?\n\nFrom: example.com",
     "Should have seen the right install message");
  dialog.document.documentElement.cancelDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_HTTP_AddSearchProvider() {
  gBrowser.selectedTab = AddSearchProvider(ROOT.replace("http:", "HTTP:") + "testEngine.xml");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "confirmEx", "Should see the confirmation dialog.");
  is(dialog.args.text, "Add \"Foo\" to the list of engines available in the search bar?\n\nFrom: example.com",
     "Should have seen the right install message");
  dialog.document.documentElement.cancelDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_relative_AddSearchProvider() {
  gBrowser.selectedTab = AddSearchProvider("testEngine.xml");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "confirmEx", "Should see the confirmation dialog.");
  is(dialog.args.text, "Add \"Foo\" to the list of engines available in the search bar?\n\nFrom: example.com",
     "Should have seen the right install message");
  dialog.document.documentElement.cancelDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_invalid_AddSearchProvider() {
  gBrowser.selectedTab = AddSearchProvider("z://foobar");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "alert", "Should see the alert dialog.");
  is(dialog.args.text, "This search engine isn't supported by Nightly and can't be installed.",
     "Should have seen the right error message")
  dialog.document.documentElement.acceptDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_missing_AddSearchProvider() {
  let url = ROOT + "foobar.xml";
  gBrowser.selectedTab = AddSearchProvider(url);

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "alert", "Should see the alert dialog.");
  is(dialog.args.text, "Nightly could not download the search plugin from:\n" + url,
     "Should have seen the right error message")
  dialog.document.documentElement.acceptDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_working_addSearchEngine_xml() {
  gBrowser.selectedTab = addSearchEngine(ROOT + "testEngine.xml", "", "", "");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "confirmEx", "Should see the confirmation dialog.");
  is(dialog.args.text, "Add \"Foo\" to the list of engines available in the search bar?\n\nFrom: example.com",
     "Should have seen the right install message");
  dialog.document.documentElement.cancelDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_working_addSearchEngine_src() {
  gBrowser.selectedTab = addSearchEngine(ROOT + "testEngine.src", "", "", "");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "confirmEx", "Should see the confirmation dialog.");
  is(dialog.args.text, "Add \"Test Sherlock\" to the list of engines available in the search bar?\n\nFrom: example.com",
     "Should have seen the right install message");
  dialog.document.documentElement.cancelDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_relative_addSearchEngine_xml() {
  gBrowser.selectedTab = addSearchEngine("testEngine.xml", "", "", "");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "confirmEx", "Should see the confirmation dialog.");
  is(dialog.args.text, "Add \"Foo\" to the list of engines available in the search bar?\n\nFrom: example.com",
     "Should have seen the right install message");
  dialog.document.documentElement.cancelDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_relative_addSearchEngine_src() {
  gBrowser.selectedTab = addSearchEngine("testEngine.src", "", "", "");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "confirmEx", "Should see the confirmation dialog.");
  is(dialog.args.text, "Add \"Test Sherlock\" to the list of engines available in the search bar?\n\nFrom: example.com",
     "Should have seen the right install message");
  dialog.document.documentElement.cancelDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_invalid_addSearchEngine() {
  gBrowser.selectedTab = addSearchEngine("z://foobar", "", "", "");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "alert", "Should see the alert dialog.");
  is(dialog.args.text, "This search engine isn't supported by Nightly and can't be installed.",
     "Should have seen the right error message")
  dialog.document.documentElement.acceptDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_invalid_icon_addSearchEngine() {
  gBrowser.selectedTab = addSearchEngine(ROOT + "testEngine.src", "z://foobar", "", "");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "alert", "Should see the alert dialog.");
  is(dialog.args.text, "This search engine isn't supported by Nightly and can't be installed.",
     "Should have seen the right error message")
  dialog.document.documentElement.acceptDialog();

  gBrowser.removeCurrentTab();
});

add_task(function* test_missing_addSearchEngine() {
  let url = ROOT + "foobar.xml";
  gBrowser.selectedTab = addSearchEngine(url, "", "", "");

  let dialog = yield promiseDialogOpened();
  is(dialog.args.promptType, "alert", "Should see the alert dialog.");
  is(dialog.args.text, "Nightly could not download the search plugin from:\n" + url,
     "Should have seen the right error message")
  dialog.document.documentElement.acceptDialog();

  gBrowser.removeCurrentTab();
});
