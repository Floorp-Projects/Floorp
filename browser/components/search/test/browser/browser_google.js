/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test Google search plugin URLs
 */

"use strict";

let expectedEngine = {
  name: "Google",
  alias: null,
  description: "Google Search",
  hidden: false,
};

add_task(async function test() {
  await Services.search.init();

  let engine = Services.search.getEngineByName("Google");
  ok(engine, "Found Google search engine");

  // Check search suggestion URL.
  let url = engine.getSubmission("foo", "application/x-suggestions+json").uri.spec;
  is(url, "https://www.google.com/complete/search?client=firefox&q=foo", "Check search suggestion URL for 'foo'");

  // Check result parsing and alternate domains.
  let base = "https://www.google.com/search?q=foo";
  is(Services.search.parseSubmissionURL(base).terms, "foo",
     "Check result parsing");
  let alternateBase = base.replace("www.google.com", "www.google.fr");
  is(Services.search.parseSubmissionURL(alternateBase).terms, "foo",
     "Check alternate domain");

  // Check all other engine properties.
  isSubObjectOf(expectedEngine, engine, "Google");
});
