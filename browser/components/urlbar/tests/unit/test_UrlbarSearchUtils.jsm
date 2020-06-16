/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { UrlbarSearchUtils } = ChromeUtils.import(
  "resource:///modules/UrlbarSearchUtils.jsm"
);

add_task(async function() {
  await UrlbarSearchUtils.init();
  // Tell the search service we are running in the US.  This also has the
  // desired side-effect of preventing our geoip lookup.
  Services.prefs.setCharPref("browser.search.region", "US");
  Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", false);

  Services.search.restoreDefaultEngines();
  Services.search.resetToOriginalDefaultEngine();
});

add_task(async function search_engine_match() {
  let engine = await Services.search.getDefault();
  let domain = engine.getResultDomain();
  let token = domain.substr(0, 1);
  let matchedEngine = await UrlbarSearchUtils.engineForDomainPrefix(token);
  Assert.equal(matchedEngine, engine);
});

add_task(async function no_match() {
  Assert.equal(null, await UrlbarSearchUtils.engineForDomainPrefix("test"));
});

add_task(async function hide_search_engine_nomatch() {
  let engine = await Services.search.getDefault();
  let domain = engine.getResultDomain();
  let token = domain.substr(0, 1);
  let promiseTopic = promiseSearchTopic("engine-changed");
  await Promise.all([Services.search.removeEngine(engine), promiseTopic]);
  Assert.ok(engine.hidden);
  let matchedEngine = await UrlbarSearchUtils.engineForDomainPrefix(token);
  Assert.ok(!matchedEngine || matchedEngine.getResultDomain() != domain);
  engine.hidden = false;
  await TestUtils.waitForCondition(() =>
    UrlbarSearchUtils.engineForDomainPrefix(token)
  );
  let matchedEngine2 = await UrlbarSearchUtils.engineForDomainPrefix(token);
  Assert.ok(matchedEngine2);
});

add_task(async function add_search_engine_match() {
  let promiseTopic = promiseSearchTopic("engine-added");
  Assert.equal(null, await UrlbarSearchUtils.engineForDomainPrefix("bacon"));
  await Promise.all([
    Services.search.addEngineWithDetails("bacon", {
      alias: "pork",
      description: "Search Bacon",
      method: "GET",
      template: "http://www.bacon.moz/?search={searchTerms}",
    }),
    promiseTopic,
  ]);
  await promiseTopic;
  let matchedEngine = await UrlbarSearchUtils.engineForDomainPrefix("bacon");
  Assert.ok(matchedEngine);
  Assert.equal(matchedEngine.searchForm, "http://www.bacon.moz");
  Assert.equal(matchedEngine.name, "bacon");
  Assert.equal(matchedEngine.iconURI, null);
});

add_task(async function test_aliased_search_engine_match() {
  Assert.equal(null, await UrlbarSearchUtils.engineForAlias("sober"));
  // Lower case
  let matchedEngine = await UrlbarSearchUtils.engineForAlias("pork");
  Assert.ok(matchedEngine);
  Assert.equal(matchedEngine.name, "bacon");
  Assert.equal(matchedEngine.alias, "pork");
  Assert.equal(matchedEngine.iconURI, null);
  // Upper case
  matchedEngine = await UrlbarSearchUtils.engineForAlias("PORK");
  Assert.ok(matchedEngine);
  Assert.equal(matchedEngine.name, "bacon");
  Assert.equal(matchedEngine.alias, "pork");
  Assert.equal(matchedEngine.iconURI, null);
  // Cap case
  matchedEngine = await UrlbarSearchUtils.engineForAlias("Pork");
  Assert.ok(matchedEngine);
  Assert.equal(matchedEngine.name, "bacon");
  Assert.equal(matchedEngine.alias, "pork");
  Assert.equal(matchedEngine.iconURI, null);
});

add_task(async function test_aliased_search_engine_match_upper_case_alias() {
  let promiseTopic = promiseSearchTopic("engine-added");
  Assert.equal(null, await UrlbarSearchUtils.engineForDomainPrefix("patch"));
  await Promise.all([
    Services.search.addEngineWithDetails("patch", {
      alias: "PR",
      description: "Search Patch",
      method: "GET",
      template: "http://www.patch.moz/?search={searchTerms}",
    }),
    promiseTopic,
  ]);
  // lower case
  let matchedEngine = await UrlbarSearchUtils.engineForAlias("pr");
  Assert.ok(matchedEngine);
  Assert.equal(matchedEngine.name, "patch");
  Assert.equal(matchedEngine.alias, "PR");
  Assert.equal(matchedEngine.iconURI, null);
  // Upper case
  matchedEngine = await UrlbarSearchUtils.engineForAlias("PR");
  Assert.ok(matchedEngine);
  Assert.equal(matchedEngine.name, "patch");
  Assert.equal(matchedEngine.alias, "PR");
  Assert.equal(matchedEngine.iconURI, null);
  // Cap case
  matchedEngine = await UrlbarSearchUtils.engineForAlias("Pr");
  Assert.ok(matchedEngine);
  Assert.equal(matchedEngine.name, "patch");
  Assert.equal(matchedEngine.alias, "PR");
  Assert.equal(matchedEngine.iconURI, null);
});

add_task(async function remove_search_engine_nomatch() {
  let engine = Services.search.getEngineByName("bacon");
  let promiseTopic = promiseSearchTopic("engine-removed");
  await Promise.all([Services.search.removeEngine(engine), promiseTopic]);
  Assert.equal(null, await UrlbarSearchUtils.engineForDomainPrefix("bacon"));
});

add_task(async function test_builtin_aliased_search_engine_match() {
  let engine = await UrlbarSearchUtils.engineForAlias("@google");
  Assert.ok(engine);
  Assert.equal(engine.name, "Google");
  let promiseTopic = promiseSearchTopic("engine-changed");
  await Promise.all([Services.search.removeEngine(engine), promiseTopic]);
  let matchedEngine = await UrlbarSearchUtils.engineForAlias("@google");
  Assert.ok(!matchedEngine);
  engine.hidden = false;
  await TestUtils.waitForCondition(() =>
    UrlbarSearchUtils.engineForAlias("@google")
  );
  engine = await UrlbarSearchUtils.engineForAlias("@google");
  Assert.ok(engine);
});

function promiseSearchTopic(expectedVerb) {
  return new Promise(resolve => {
    Services.obs.addObserver(function observe(subject, topic, verb) {
      info("browser-search-engine-modified: " + verb);
      if (verb == expectedVerb) {
        Services.obs.removeObserver(observe, "browser-search-engine-modified");
        resolve();
      }
    }, "browser-search-engine-modified");
  });
}
