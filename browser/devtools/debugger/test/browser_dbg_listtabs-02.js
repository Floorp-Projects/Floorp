/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure the root actor's live tab list implementation works as specified.
 */

let { BrowserTabList } = require("devtools/server/actors/webbrowser");

let gTestPage = "data:text/html;charset=utf-8," + encodeURIComponent(
  "<title>JS Debugger BrowserTabList test page</title><body>Yo.</body>");

// The tablist object whose behavior we observe.
let gTabList;
let gFirstActor, gActorA;
let gTabA, gTabB, gTabC;
let gNewWindow;

// Stock onListChanged handler.
let onListChangedCount = 0;
function onListChangedHandler() {
  onListChangedCount++;
}

function test() {
  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }

  gTabList = new BrowserTabList("fake DebuggerServerConnection");
  gTabList._testing = true;
  gTabList.onListChanged = onListChangedHandler;

  checkSingleTab()
    .then(addTabA)
    .then(testTabA)
    .then(addTabB)
    .then(testTabB)
    .then(removeTabA)
    .then(testTabClosed)
    .then(addTabC)
    .then(testTabC)
    .then(removeTabC)
    .then(testNewWindow)
    .then(removeNewWindow)
    .then(testWindowClosed)
    .then(removeTabB)
    .then(checkSingleTab)
    .then(finishUp);
}

function checkSingleTab() {
  return gTabList.getList().then(aTabActors => {
    is(aTabActors.length, 1, "initial tab list: contains initial tab");
    gFirstActor = aTabActors[0];
    is(gFirstActor.url, "about:blank", "initial tab list: initial tab URL is 'about:blank'");
    is(gFirstActor.title, "New Tab", "initial tab list: initial tab title is 'New Tab'");
  });
}

function addTabA() {
  return addTab(gTestPage).then(aTab => {
    gTabA = aTab;
  });
}

function testTabA() {
  is(onListChangedCount, 1, "onListChanged handler call count");

  return gTabList.getList().then(aTabActors => {
    let tabActors = new Set(aTabActors);
    is(tabActors.size, 2, "gTabA opened: two tabs in list");
    ok(tabActors.has(gFirstActor), "gTabA opened: initial tab present");

    info("actors: " + [...tabActors].map(a => a.url));
    gActorA = [...tabActors].filter(a => a !== gFirstActor)[0];
    ok(gActorA.url.match(/^data:text\/html;/), "gTabA opened: new tab URL");
    is(gActorA.title, "JS Debugger BrowserTabList test page", "gTabA opened: new tab title");
  });
}

function addTabB() {
  return addTab(gTestPage).then(aTab => {
    gTabB = aTab;
  });
}

function testTabB() {
  is(onListChangedCount, 2, "onListChanged handler call count");

  return gTabList.getList().then(aTabActors => {
    let tabActors = new Set(aTabActors);
    is(tabActors.size, 3, "gTabB opened: three tabs in list");
  });
}

function removeTabA() {
  let deferred = promise.defer();

  once(gBrowser.tabContainer, "TabClose").then(aEvent => {
    ok(!aEvent.detail, "This was a normal tab close");

    // Let the actor's TabClose handler finish first.
    executeSoon(deferred.resolve);
  }, false);

  removeTab(gTabA);
  return deferred.promise;
}

function testTabClosed() {
  is(onListChangedCount, 3, "onListChanged handler call count");

  gTabList.getList().then(aTabActors => {
    let tabActors = new Set(aTabActors);
    is(tabActors.size, 2, "gTabA closed: two tabs in list");
    ok(tabActors.has(gFirstActor), "gTabA closed: initial tab present");

    info("actors: " + [...tabActors].map(a => a.url));
    gActorA = [...tabActors].filter(a => a!== gFirstActor)[0];
    ok(gActorA.url.match(/^data:text\/html;/), "gTabA closed: new tab URL");
    is(gActorA.title, "JS Debugger BrowserTabList test page", "gTabA closed: new tab title");
  });
}

function addTabC() {
  return addTab(gTestPage).then(aTab => {
    gTabC = aTab;
  });
}

function testTabC() {
  is(onListChangedCount, 4, "onListChanged handler call count");

  gTabList.getList().then(aTabActors => {
    let tabActors = new Set(aTabActors);
    is(tabActors.size, 3, "gTabC opened: three tabs in list");
  });
}

function removeTabC() {
  let deferred = promise.defer();

  once(gBrowser.tabContainer, "TabClose").then(aEvent => {
    ok(aEvent.detail, "This was a tab closed by moving");

    // Let the actor's TabClose handler finish first.
    executeSoon(deferred.resolve);
  }, false);

  gNewWindow = gBrowser.replaceTabWithWindow(gTabC);
  return deferred.promise;
}

function testNewWindow() {
  is(onListChangedCount, 5, "onListChanged handler call count");

  return gTabList.getList().then(aTabActors => {
    let tabActors = new Set(aTabActors);
    is(tabActors.size, 3, "gTabC closed: three tabs in list");
    ok(tabActors.has(gFirstActor), "gTabC closed: initial tab present");

    info("actors: " + [...tabActors].map(a => a.url));
    gActorA = [...tabActors].filter(a => a !== gFirstActor)[0];
    ok(gActorA.url.match(/^data:text\/html;/), "gTabC closed: new tab URL");
    is(gActorA.title, "JS Debugger BrowserTabList test page", "gTabC closed: new tab title");
  });
}

function removeNewWindow() {
  let deferred = promise.defer();

  once(gNewWindow, "unload").then(aEvent => {
    ok(!aEvent.detail, "This was a normal window close");

    // Let the actor's TabClose handler finish first.
    executeSoon(deferred.resolve);
  }, false);

  gNewWindow.close();
  return deferred.promise;
}

function testWindowClosed() {
  is(onListChangedCount, 6, "onListChanged handler call count");

  return gTabList.getList().then(aTabActors => {
    let tabActors = new Set(aTabActors);
    is(tabActors.size, 2, "gNewWindow closed: two tabs in list");
    ok(tabActors.has(gFirstActor), "gNewWindow closed: initial tab present");

    info("actors: " + [...tabActors].map(a => a.url));
    gActorA = [...tabActors].filter(a => a !== gFirstActor)[0];
    ok(gActorA.url.match(/^data:text\/html;/), "gNewWindow closed: new tab URL");
    is(gActorA.title, "JS Debugger BrowserTabList test page", "gNewWindow closed: new tab title");
  });
}

function removeTabB() {
  let deferred = promise.defer();

  once(gBrowser.tabContainer, "TabClose").then(aEvent => {
    ok(!aEvent.detail, "This was a normal tab close");

    // Let the actor's TabClose handler finish first.
    executeSoon(deferred.resolve);
  }, false);

  removeTab(gTabB);
  return deferred.promise;
}

function finishUp() {
  gTabList = gFirstActor = gActorA = gTabA = gTabB = gTabC = gNewWindow = null;
  finish();
}
