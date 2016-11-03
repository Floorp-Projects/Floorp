/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// See browser/components/search/test/browser_*_behavior.js for tests of actual
// searches.

Cu.import("resource://gre/modules/Task.jsm");

const ENGINE_NO_LOGO = {
  name: "searchEngineNoLogo.xml",
  numLogos: 0,
};

const ENGINE_FAVICON = {
  name: "searchEngineFavicon.xml",
  logoPrefix1x: "data:image/png;base64,AAABAAIAICAAAAEAIACoEAAAJgAAABAQAAABACAAaAQAAM4QAAAoAAAAIAAAAEAAAAABACAAAAAAAAAQAAATCwAAEwsA",
  numLogos: 1,
};
ENGINE_FAVICON.logoPrefix2x = ENGINE_FAVICON.logoPrefix1x;

const ENGINE_1X_LOGO = {
  name: "searchEngine1xLogo.xml",
  logoPrefix1x: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEEAAAAaCAIAAABn3KYmAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3gkTADEw",
  numLogos: 1,
};
ENGINE_1X_LOGO.logoPrefix2x = ENGINE_1X_LOGO.logoPrefix1x;

const ENGINE_2X_LOGO = {
  name: "searchEngine2xLogo.xml",
  logoPrefix2x: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAIIAAAA0CAIAAADJ8nfCAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3gkTADMU",
  numLogos: 1,
};
ENGINE_2X_LOGO.logoPrefix1x = ENGINE_2X_LOGO.logoPrefix2x;

const ENGINE_1X_2X_LOGO = {
  name: "searchEngine1x2xLogo.xml",
  logoPrefix1x: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEEAAAAaCAIAAABn3KYmAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3gkTADIG",
  logoPrefix2x: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAIIAAAA0CAIAAADJ8nfCAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3gkTADMo",
  numLogos: 2,
};

const ENGINE_SUGGESTIONS = {
  name: "searchSuggestionEngine.xml",
  numLogos: 0,
};

// The test has an expected search event queue and a search event listener.
// Search events that are expected to happen are added to the queue, and the
// listener consumes the queue and ensures that each event it receives is at
// the head of the queue.
let gExpectedSearchEventQueue = [];
let gExpectedSearchEventResolver = null;

let gNewEngines = [];

add_task(function* () {
  let oldCurrentEngine = Services.search.currentEngine;

  yield* addNewTabPageTab();

  // The tab is removed at the end of the test, so there's no need to remove
  // this listener at the end of the test.
  info("Adding search event listener");
  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    const SERVICE_EVENT_NAME = "ContentSearchService";
    content.addEventListener(SERVICE_EVENT_NAME, function (event) {
      sendAsyncMessage("test:search-event", { eventType: event.detail.type });
    });
  });

  let mm = gBrowser.selectedBrowser.messageManager;
  mm.addMessageListener("test:search-event", function (message) {
    let eventType = message.data.eventType;
    if (!gExpectedSearchEventResolver) {
      ok(false, "Got search event " + eventType + " with no promise assigned");
    }

    let expectedEventType = gExpectedSearchEventQueue.shift();
    is(eventType, expectedEventType, "Got expected search event " + expectedEventType);
    if (!gExpectedSearchEventQueue.length) {
      gExpectedSearchEventResolver();
      gExpectedSearchEventResolver = null;
    }
  });

  // Add the engine without any logos and switch to it.
  let noLogoEngine = yield promiseNewSearchEngine(ENGINE_NO_LOGO);
  let searchEventsPromise = promiseSearchEvents(["CurrentEngine"]);
  Services.search.currentEngine = noLogoEngine;
  yield searchEventsPromise;
  yield* checkCurrentEngine(ENGINE_NO_LOGO);

  // Add the engine with favicon and switch to it.
  let faviconEngine = yield promiseNewSearchEngine(ENGINE_FAVICON);
  searchEventsPromise = promiseSearchEvents(["CurrentEngine"]);
  Services.search.currentEngine = faviconEngine;
  yield searchEventsPromise;
  yield* checkCurrentEngine(ENGINE_FAVICON);

  // Add the engine with a 1x-DPI logo and switch to it.
  let logo1xEngine = yield promiseNewSearchEngine(ENGINE_1X_LOGO);
  searchEventsPromise = promiseSearchEvents(["CurrentEngine"]);
  Services.search.currentEngine = logo1xEngine;
  yield searchEventsPromise;
  yield* checkCurrentEngine(ENGINE_1X_LOGO);

  // Add the engine with a 2x-DPI logo and switch to it.
  let logo2xEngine = yield promiseNewSearchEngine(ENGINE_2X_LOGO);
  searchEventsPromise = promiseSearchEvents(["CurrentEngine"]);
  Services.search.currentEngine = logo2xEngine;
  yield searchEventsPromise;
  yield* checkCurrentEngine(ENGINE_2X_LOGO);

  // Add the engine with 1x- and 2x-DPI logos and switch to it.
  let logo1x2xEngine = yield promiseNewSearchEngine(ENGINE_1X_2X_LOGO);
  searchEventsPromise = promiseSearchEvents(["CurrentEngine"]);
  Services.search.currentEngine = logo1x2xEngine;
  yield searchEventsPromise;
  yield* checkCurrentEngine(ENGINE_1X_2X_LOGO);

  // Add the engine that provides search suggestions and switch to it.
  let suggestionEngine = yield promiseNewSearchEngine(ENGINE_SUGGESTIONS);
  searchEventsPromise = promiseSearchEvents(["CurrentEngine"]);
  Services.search.currentEngine = suggestionEngine;
  yield searchEventsPromise;
  yield* checkCurrentEngine(ENGINE_SUGGESTIONS);

  // Avoid intermittent failures.
  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    content.gSearch._contentSearchController.remoteTimeout = 5000;
  });

  // Type an X in the search input.  This is only a smoke test.  See
  // browser_searchSuggestionUI.js for comprehensive content search suggestion
  // UI tests.
  let suggestionsOpenPromise = new Promise(resolve => {
    mm.addMessageListener("test:newtab-suggestions-open", function onResponse(message) {
      mm.removeMessageListener("test:newtab-suggestions-open", onResponse);
      resolve();
    });
  });

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    let table = content.document.getElementById("searchSuggestionTable");

    let input = content.document.getElementById("newtab-search-text");
    input.focus();

    info("Waiting for suggestions table to open");
    let observer = new content.MutationObserver(() => {
      if (input.getAttribute("aria-expanded") == "true") {
        observer.disconnect();
        Assert.ok(!table.hidden, "Search suggestion table unhidden");
        sendAsyncMessage("test:newtab-suggestions-open", {});
      }
    });
    observer.observe(input, {
      attributes: true,
      attributeFilter: ["aria-expanded"],
    });
  });

  let suggestionsPromise = promiseSearchEvents(["Suggestions"]);

  EventUtils.synthesizeKey("x", {});

  // Wait for the search suggestions to become visible and for the Suggestions
  // message.
  yield suggestionsOpenPromise;
  yield suggestionsPromise;

  // Empty the search input, causing the suggestions to be hidden.
  EventUtils.synthesizeKey("a", { accelKey: true });
  EventUtils.synthesizeKey("VK_DELETE", {});

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    Assert.ok(content.document.getElementById("searchSuggestionTable").hidden,
      "Search suggestion table hidden");
  });

  // Done.  Revert the current engine and remove the new engines.
  searchEventsPromise = promiseSearchEvents(["CurrentEngine"]);
  Services.search.currentEngine = oldCurrentEngine;
  yield searchEventsPromise;

  let events = Array(gNewEngines.length).fill("CurrentState", 0, gNewEngines.length);
  searchEventsPromise = promiseSearchEvents(events);

  for (let engine of gNewEngines) {
    Services.search.removeEngine(engine);
  }
  yield searchEventsPromise;
});

function promiseSearchEvents(events) {
  info("Expecting search events: " + events);
  return new Promise(resolve => {
    gExpectedSearchEventQueue.push(...events);
    gExpectedSearchEventResolver = resolve;
  });
}

function promiseNewSearchEngine({name: basename, numLogos}) {
  info("Waiting for engine to be added: " + basename);

  // Wait for the search events triggered by adding the new engine.
  // engine-added engine-loaded
  let expectedSearchEvents = ["CurrentState", "CurrentState"];
  // engine-changed for each of the logos
  for (let i = 0; i < numLogos; i++) {
    expectedSearchEvents.push("CurrentState");
  }
  let eventPromise = promiseSearchEvents(expectedSearchEvents);

  // Wait for addEngine().
  let addEnginePromise = new Promise((resolve, reject) => {
    let url = getRootDirectory(gTestPath) + basename;
    Services.search.addEngine(url, null, "", false, {
      onSuccess: function (engine) {
        info("Search engine added: " + basename);
        gNewEngines.push(engine);
        resolve(engine);
      },
      onError: function (errCode) {
        ok(false, "addEngine failed with error code " + errCode);
        reject();
      },
    });
  });

  return Promise.all([addEnginePromise, eventPromise]).then(([newEngine, _]) => {
    return newEngine;
  });
}

function* checkCurrentEngine(engineInfo)
{
  let engine = Services.search.currentEngine;
  ok(engine.name.includes(engineInfo.name),
     "Sanity check: current engine: engine.name=" + engine.name +
     " basename=" + engineInfo.name);

  yield ContentTask.spawn(gBrowser.selectedBrowser, { name: engine.name }, function* (args) {
    Assert.equal(content.gSearch._contentSearchController.defaultEngine.name,
      args.name, "currentEngineName: " + args.name);
  });
}
