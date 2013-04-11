/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;

  function observer(subject, topic, data) {
    Services.obs.removeObserver(observer, "keyword-search");
    is(topic, "keyword-search", "Got keyword-search notification");

    let engine = Services.search.defaultEngine;
    ok(engine, "Have default search engine.");
    is(engine.name, data, "Notification data is engine name.");

    executeSoon(function cleanup() {
      gBrowser.removeTab(tab);
      finish();
    });
  }

  Services.obs.addObserver(observer, "keyword-search", false);

  gURLBar.value = "firefox health report";
  gURLBar.handleCommand();
}

