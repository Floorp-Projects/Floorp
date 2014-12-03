/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure the listTabs request works as specified.
 */

const TAB1_URL = EXAMPLE_URL + "doc_empty-tab-01.html";

let gTab1, gTab1Actor, gTab2, gTab2Actor, gClient;

function listTabs() {
  let deferred = promise.defer();

  gClient.listTabs(aResponse => {
    deferred.resolve(aResponse.tabs);
  });

  return deferred.promise;
}

function request(params) {
  let deferred = promise.defer();
  gClient.request(params, deferred.resolve);
  return deferred.promise;
}

function test() {
  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }

  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect(Task.async(function*(aType, aTraits) {
    is(aType, "browser",
      "Root actor should identify itself as a browser.");
    let tab = yield addTab(TAB1_URL);

    let tabs = yield listTabs();
    is(tabs.length, 2, "Should be two tabs");
    let tabGrip = tabs.filter(a => a.url ==TAB1_URL).pop();
    ok(tabGrip, "Should have an actor for the tab");

    let response = yield request({ to: tabGrip.actor, type: "attach" });
    is(response.type, "tabAttached", "Should have attached");

    tabs = yield listTabs();

    response = yield request({ to: tabGrip.actor, type: "detach" });
    is(response.type, "detached", "Should have detached");

    let newGrip = tabs.filter(a => a.url ==TAB1_URL).pop();
    is(newGrip.actor, tabGrip.actor, "Should have the same actor for the same tab");

    response = yield request({ to: tabGrip.actor, type: "attach" });
    is(response.type, "tabAttached", "Should have attached");
    response = yield request({ to: tabGrip.actor, type: "detach" });
    is(response.type, "detached", "Should have detached");

    yield removeTab(tab);
    yield closeConnection();
    finish();
  }));
}

function closeConnection() {
  let deferred = promise.defer();
  gClient.close(deferred.resolve);
  return deferred.promise;
}

registerCleanupFunction(function() {
  gTab1 = null;
  gTab1Actor = null;
  gTab2 = null;
  gTab2Actor = null;
  gClient = null;
});
