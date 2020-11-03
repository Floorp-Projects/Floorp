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

  Services.search.restoreDefaultEngines();
  Services.search.resetToOriginalDefaultEngine();
});

add_task(async function search_engine_match() {
  let engine = await Services.search.getDefault();
  let domain = engine.getResultDomain();
  let token = domain.substr(0, 1);
  let matchedEngine = (
    await UrlbarSearchUtils.enginesForDomainPrefix(token)
  )[0];
  Assert.equal(matchedEngine, engine);
});

add_task(async function no_match() {
  Assert.equal(
    0,
    (await UrlbarSearchUtils.enginesForDomainPrefix("test")).length
  );
});

add_task(async function hide_search_engine_nomatch() {
  let engine = await Services.search.getDefault();
  let domain = engine.getResultDomain();
  let token = domain.substr(0, 1);
  let promiseTopic = promiseSearchTopic("engine-changed");
  await Promise.all([Services.search.removeEngine(engine), promiseTopic]);
  Assert.ok(engine.hidden);
  let matchedEngines = await UrlbarSearchUtils.enginesForDomainPrefix(token);
  Assert.ok(
    !matchedEngines.length || matchedEngines[0].getResultDomain() != domain
  );
  engine.hidden = false;
  await TestUtils.waitForCondition(
    async () => (await UrlbarSearchUtils.enginesForDomainPrefix(token)).length
  );
  let matchedEngine2 = (
    await UrlbarSearchUtils.enginesForDomainPrefix(token)
  )[0];
  Assert.ok(matchedEngine2);
});

add_task(async function add_search_engine_match() {
  let promiseTopic = promiseSearchTopic("engine-added");
  Assert.equal(
    0,
    (await UrlbarSearchUtils.enginesForDomainPrefix("bacon")).length
  );
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
  let matchedEngine = (
    await UrlbarSearchUtils.enginesForDomainPrefix("bacon")
  )[0];
  Assert.ok(matchedEngine);
  Assert.equal(matchedEngine.searchForm, "http://www.bacon.moz");
  Assert.equal(matchedEngine.name, "bacon");
  Assert.equal(matchedEngine.iconURI, null);
  info("also type part of the public suffix");
  matchedEngine = (
    await UrlbarSearchUtils.enginesForDomainPrefix("bacon.m")
  )[0];
  Assert.ok(matchedEngine);
  Assert.equal(matchedEngine.searchForm, "http://www.bacon.moz");
  Assert.equal(matchedEngine.name, "bacon");
  Assert.equal(matchedEngine.iconURI, null);
});

add_task(async function match_multiple_search_engines() {
  let promiseTopic = promiseSearchTopic("engine-added");
  Assert.equal(
    0,
    (await UrlbarSearchUtils.enginesForDomainPrefix("baseball")).length
  );
  await Promise.all([
    Services.search.addEngineWithDetails("baseball", {
      description: "Search Baseball",
      method: "GET",
      template: "http://www.baseball.moz/?search={searchTerms}",
    }),
    promiseTopic,
  ]);
  await promiseTopic;
  let matchedEngines = await UrlbarSearchUtils.enginesForDomainPrefix("ba");
  Assert.equal(
    matchedEngines.length,
    2,
    "enginesForDomainPrefix returned two engines."
  );
  Assert.equal(matchedEngines[0].searchForm, "http://www.bacon.moz");
  Assert.equal(matchedEngines[0].name, "bacon");
  Assert.equal(matchedEngines[1].searchForm, "http://www.baseball.moz");
  Assert.equal(matchedEngines[1].name, "baseball");
});

add_task(async function test_aliased_search_engine_match() {
  Assert.equal(null, await UrlbarSearchUtils.engineForAlias("sober"));
  // Lower case
  let matchedEngine = await UrlbarSearchUtils.engineForAlias("pork");
  Assert.ok(matchedEngine);
  Assert.equal(matchedEngine.name, "bacon");
  Assert.ok(matchedEngine.aliases.includes("pork"));
  Assert.equal(matchedEngine.iconURI, null);
  // Upper case
  matchedEngine = await UrlbarSearchUtils.engineForAlias("PORK");
  Assert.ok(matchedEngine);
  Assert.equal(matchedEngine.name, "bacon");
  Assert.ok(matchedEngine.aliases.includes("pork"));
  Assert.equal(matchedEngine.iconURI, null);
  // Cap case
  matchedEngine = await UrlbarSearchUtils.engineForAlias("Pork");
  Assert.ok(matchedEngine);
  Assert.equal(matchedEngine.name, "bacon");
  Assert.ok(matchedEngine.aliases.includes("pork"));
  Assert.equal(matchedEngine.iconURI, null);
});

add_task(async function test_aliased_search_engine_match_upper_case_alias() {
  let promiseTopic = promiseSearchTopic("engine-added");
  Assert.equal(
    0,
    (await UrlbarSearchUtils.enginesForDomainPrefix("patch")).length
  );
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
  Assert.ok(matchedEngine.aliases.includes("PR"));
  Assert.equal(matchedEngine.iconURI, null);
  // Upper case
  matchedEngine = await UrlbarSearchUtils.engineForAlias("PR");
  Assert.ok(matchedEngine);
  Assert.equal(matchedEngine.name, "patch");
  Assert.ok(matchedEngine.aliases.includes("PR"));
  Assert.equal(matchedEngine.iconURI, null);
  // Cap case
  matchedEngine = await UrlbarSearchUtils.engineForAlias("Pr");
  Assert.ok(matchedEngine);
  Assert.equal(matchedEngine.name, "patch");
  Assert.ok(matchedEngine.aliases.includes("PR"));
  Assert.equal(matchedEngine.iconURI, null);
});

add_task(async function remove_search_engine_nomatch() {
  let engine = Services.search.getEngineByName("bacon");
  let promiseTopic = promiseSearchTopic("engine-removed");
  await Promise.all([Services.search.removeEngine(engine), promiseTopic]);
  Assert.equal(
    0,
    (await UrlbarSearchUtils.enginesForDomainPrefix("bacon")).length
  );
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

add_task(async function test_serps_are_equivalent() {
  info("Subset URL has extraneous parameters.");
  let url1 = "https://example.com/search?q=test&type=images";
  let url2 = "https://example.com/search?q=test";
  Assert.ok(!UrlbarSearchUtils.serpsAreEquivalent(url1, url2));
  info("Superset URL has extraneous parameters.");
  Assert.ok(UrlbarSearchUtils.serpsAreEquivalent(url2, url1));

  info("Same keys, different values.");
  url1 = "https://example.com/search?q=test&type=images";
  url2 = "https://example.com/search?q=test123&type=maps";
  Assert.ok(!UrlbarSearchUtils.serpsAreEquivalent(url1, url2));
  Assert.ok(!UrlbarSearchUtils.serpsAreEquivalent(url2, url1));

  info("Subset matching isn't strict (URL is subset of itself).");
  Assert.ok(UrlbarSearchUtils.serpsAreEquivalent(url1, url1));

  info("Origin and pathname are ignored.");
  url1 = "https://example.com/search?q=test";
  url2 = "https://example-1.com/maps?q=test";
  Assert.ok(UrlbarSearchUtils.serpsAreEquivalent(url1, url2));
  Assert.ok(UrlbarSearchUtils.serpsAreEquivalent(url2, url1));

  info("Params can be optionally ignored");
  url1 = "https://example.com/search?q=test&abc=123&foo=bar";
  url2 = "https://example.com/search?q=test";
  Assert.ok(!UrlbarSearchUtils.serpsAreEquivalent(url1, url2));
  Assert.ok(UrlbarSearchUtils.serpsAreEquivalent(url1, url2, ["abc", "foo"]));
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
