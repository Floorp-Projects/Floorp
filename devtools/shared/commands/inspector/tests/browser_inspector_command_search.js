/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing basic inspector search

add_task(async () => {
  const html = `<div>
                  <div>
                    <p>This is the paragraph node down in the tree</p>
                  </div>
                  <div class="child"></div>
                  <div class="child"></div>
                  <iframe src="data:text/html,${encodeURIComponent(
                    "<html><body><div class='frame-child'>foo</div></body></html>"
                  )}">
                  </iframe>
                </div>`;

  const tab = await addTab("data:text/html," + encodeURIComponent(html));

  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();

  info("Search using text");
  await searchAndAssert(
    commands,
    { query: "paragraph", reverse: false },
    { resultsLength: 1, resultsIndex: 0 }
  );

  info("Search using class selector");
  info(" > Get first result ");
  await searchAndAssert(
    commands,
    { query: ".child", reverse: false },
    { resultsLength: 2, resultsIndex: 0 }
  );

  info(" > Get next result ");
  await searchAndAssert(
    commands,
    { query: ".child", reverse: false },
    { resultsLength: 2, resultsIndex: 1 }
  );

  info("Search using el selector with reverse option");
  info(" > Get first result ");
  await searchAndAssert(
    commands,
    { query: "div", reverse: true },
    { resultsLength: 6, resultsIndex: 5 }
  );

  info(" > Get next result ");
  await searchAndAssert(
    commands,
    { query: "div", reverse: true },
    { resultsLength: 6, resultsIndex: 4 }
  );

  info("Search for foo in remote frame");
  await searchAndAssert(
    commands,
    { query: ".frame-child", reverse: false },
    { resultsLength: 1, resultsIndex: 0 }
  );

  await commands.destroy();
});
/**
 * Does an inspector search to find the next node and assert the results
 *
 * @param {Object} commands
 * @param {Object} options
 *            options.query - search query
 *            options.reverse - search in reverse
 * @param {Object} expected
 *         Holds the expected values
 */
async function searchAndAssert(commands, { query, reverse }, expected) {
  const response = await commands.inspectorCommand.findNextNode(query, {
    reverse,
  });

  is(
    response.resultsLength,
    expected.resultsLength,
    "Got the expected no of results"
  );

  is(
    response.resultsIndex,
    expected.resultsIndex,
    "Got the expected currently selected node index"
  );
}
