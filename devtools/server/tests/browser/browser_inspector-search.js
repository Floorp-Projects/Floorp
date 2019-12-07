/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/server/tests/browser/inspector-helpers.js",
  this
);

// Test for Bug 835896
// WalkerSearch specific tests.  This is to make sure search results are
// coming back as expected.
// See also test_inspector-search-front.html.

add_task(async function() {
  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-search-data.html"
  );

  await ContentTask.spawn(
    gBrowser.selectedBrowser,
    [walker.actorID],
    async function(actorID) {
      const { require } = ChromeUtils.import(
        "resource://devtools/shared/Loader.jsm"
      );
      const { DebuggerServer } = require("devtools/server/debugger-server");
      const {
        DocumentWalker: _documentWalker,
      } = require("devtools/server/actors/inspector/document-walker");

      // Convert actorID to current compartment string otherwise
      // searchAllConnectionsForActor is confused and won't find the actor.
      actorID = String(actorID);
      const walkerActor = DebuggerServer.searchAllConnectionsForActor(actorID);
      const walkerSearch = walkerActor.walkerSearch;
      const {
        WalkerSearch,
        WalkerIndex,
      } = require("devtools/server/actors/utils/walker-search");

      info("Testing basic index APIs exist.");
      const index = new WalkerIndex(walkerActor);
      ok(index.data.size > 0, "public index is filled after getting");

      index.clearIndex();
      ok(!index._data, "private index is empty after clearing");
      ok(index.data.size > 0, "public index is filled after getting");

      index.destroy();

      info("Testing basic search APIs exist.");

      ok(walkerSearch, "walker search exists on the WalkerActor");
      ok(walkerSearch.search, "walker search has `search` method");
      ok(walkerSearch.index, "walker search has `index` property");
      is(
        walkerSearch.walker,
        walkerActor,
        "referencing the correct WalkerActor"
      );

      const walkerSearch2 = new WalkerSearch(walkerActor);
      ok(walkerSearch2, "a new search instance can be created");
      ok(walkerSearch2.search, "new search instance has `search` method");
      ok(walkerSearch2.index, "new search instance has `index` property");
      isnot(
        walkerSearch2,
        walkerSearch,
        "new search instance differs from the WalkerActor's"
      );

      walkerSearch2.destroy();

      info("Testing search with an empty query.");
      let results = walkerSearch.search("");
      is(results.length, 0, "No results when searching for ''");

      results = walkerSearch.search(null);
      is(results.length, 0, "No results when searching for null");

      results = walkerSearch.search(undefined);
      is(results.length, 0, "No results when searching for undefined");

      results = walkerSearch.search(10);
      is(results.length, 0, "No results when searching for 10");

      const inspectee = content.document;
      const testData = [
        {
          desc: "Search for tag with one result.",
          search: "body",
          expected: [{ node: inspectee.body, type: "tag" }],
        },
        {
          desc: "Search for tag with multiple results",
          search: "h2",
          expected: [
            { node: inspectee.querySelectorAll("h2")[0], type: "tag" },
            { node: inspectee.querySelectorAll("h2")[1], type: "tag" },
            { node: inspectee.querySelectorAll("h2")[2], type: "tag" },
          ],
        },
        {
          desc: "Search for selector with multiple results",
          search: "body > h2",
          expected: [
            { node: inspectee.querySelectorAll("h2")[0], type: "selector" },
            { node: inspectee.querySelectorAll("h2")[1], type: "selector" },
            { node: inspectee.querySelectorAll("h2")[2], type: "selector" },
          ],
        },
        {
          desc: "Search for selector with multiple results",
          search: ":root h2",
          expected: [
            { node: inspectee.querySelectorAll("h2")[0], type: "selector" },
            { node: inspectee.querySelectorAll("h2")[1], type: "selector" },
            { node: inspectee.querySelectorAll("h2")[2], type: "selector" },
          ],
        },
        {
          desc: "Search for selector with multiple results",
          search: "* h2",
          expected: [
            { node: inspectee.querySelectorAll("h2")[0], type: "selector" },
            { node: inspectee.querySelectorAll("h2")[1], type: "selector" },
            { node: inspectee.querySelectorAll("h2")[2], type: "selector" },
          ],
        },
        {
          desc:
            "Search with multiple matches in a single tag expecting a single result",
          search: "💩",
          expected: [
            { node: inspectee.getElementById("💩"), type: "attributeValue" },
          ],
        },
        {
          desc: "Search that has tag and text results",
          search: "h1",
          expected: [
            { node: inspectee.querySelector("h1"), type: "tag" },
            {
              node: inspectee.querySelector("h1 + p").childNodes[0],
              type: "text",
            },
            {
              node: inspectee.querySelector("h1 + p > strong").childNodes[0],
              type: "text",
            },
          ],
        },
      ];

      const isDeeply = (a, b, msg) => {
        return is(JSON.stringify(a), JSON.stringify(b), msg);
      };
      for (const { desc, search, expected } of testData) {
        info("Running test: " + desc);
        results = walkerSearch.search(search);
        isDeeply(
          results,
          expected,
          "Search returns correct results with '" + search + "'"
        );
      }

      info("Testing ::before and ::after element matching");

      const beforeElt = new _documentWalker(
        inspectee.querySelector("#pseudo"),
        inspectee.defaultView
      ).firstChild();
      const afterElt = new _documentWalker(
        inspectee.querySelector("#pseudo"),
        inspectee.defaultView
      ).lastChild();
      const styleText = inspectee.querySelector("style").childNodes[0];

      // ::before
      results = walkerSearch.search("::before");
      isDeeply(
        results,
        [{ node: beforeElt, type: "tag" }],
        "Tag search works for pseudo element"
      );

      results = walkerSearch.search("_moz_generated_content_before");
      is(results.length, 0, "No results for anon tag name");

      results = walkerSearch.search("before element");
      isDeeply(
        results,
        [{ node: styleText, type: "text" }, { node: beforeElt, type: "text" }],
        "Text search works for pseudo element"
      );

      // ::after
      results = walkerSearch.search("::after");
      isDeeply(
        results,
        [{ node: afterElt, type: "tag" }],
        "Tag search works for pseudo element"
      );

      results = walkerSearch.search("_moz_generated_content_after");
      is(results.length, 0, "No results for anon tag name");

      results = walkerSearch.search("after element");
      isDeeply(
        results,
        [{ node: styleText, type: "text" }, { node: afterElt, type: "text" }],
        "Text search works for pseudo element"
      );

      info("Testing search before and after a mutation.");
      const expected = [
        { node: inspectee.querySelectorAll("h3")[0], type: "tag" },
        { node: inspectee.querySelectorAll("h3")[1], type: "tag" },
        { node: inspectee.querySelectorAll("h3")[2], type: "tag" },
      ];

      results = walkerSearch.search("h3");
      isDeeply(results, expected, "Search works with tag results");

      function mutateDocumentAndWaitForMutation(mutationFn) {
        // eslint-disable-next-line new-cap
        return new Promise(resolve => {
          info("Listening to markup mutation on the inspectee");
          const observer = new inspectee.defaultView.MutationObserver(resolve);
          observer.observe(inspectee, { childList: true, subtree: true });
          mutationFn();
        });
      }
      await mutateDocumentAndWaitForMutation(() => {
        expected[0].node.remove();
      });

      results = walkerSearch.search("h3");
      isDeeply(
        results,
        [expected[1], expected[2]],
        "Results are updated after removal"
      );

      // eslint-disable-next-line new-cap
      await new Promise(resolve => {
        info("Waiting for a mutation to happen");
        const observer = new inspectee.defaultView.MutationObserver(() => {
          resolve();
        });
        observer.observe(inspectee, { attributes: true, subtree: true });
        inspectee.body.setAttribute("h3", "true");
      });

      results = walkerSearch.search("h3");
      isDeeply(
        results,
        [
          { node: inspectee.body, type: "attributeName" },
          expected[1],
          expected[2],
        ],
        "Results are updated after addition"
      );

      // eslint-disable-next-line new-cap
      await new Promise(resolve => {
        info("Waiting for a mutation to happen");
        const observer = new inspectee.defaultView.MutationObserver(() => {
          resolve();
        });
        observer.observe(inspectee, {
          attributes: true,
          childList: true,
          subtree: true,
        });
        inspectee.body.removeAttribute("h3");
        expected[1].node.remove();
        expected[2].node.remove();
      });

      results = walkerSearch.search("h3");
      is(results.length, 0, "Results are updated after removal");
    }
  );
});
