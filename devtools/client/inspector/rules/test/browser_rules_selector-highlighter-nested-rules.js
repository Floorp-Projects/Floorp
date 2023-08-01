/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the selector highlighter works for nested rules.

const TEST_URI = `
  <style>
    main {
      background: tomato;
      & > h1 {
        color: gold;

        &#title {
          text-decoration: underline;
        }

        &.title {
          outline: 2px solid rebeccapurple;
          & em {
            color: salmon;

            html & {
              padding: 1em;
            }
          }
        }
      }

      .title {
        font-weight: 32px;
      }
    }
  </style>
  <main>
    <h1 class="title" id="title">Selector Highlighter for <em>nested rules</em></h1>
  </main>`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  await selectNode("h1", inspector);

  const activeHighlighter = inspector.highlighters.getActiveHighlighter(
    inspector.highlighters.TYPES.SELECTOR
  );
  ok(!activeHighlighter, "No selector highlighter is active");

  info(`Clicking on "& > h1" selector icon`);
  let highlighterData = await clickSelectorIcon(view, "& > h1");
  is(
    highlighterData.nodeFront.nodeName.toLowerCase(),
    "h1",
    "<h1> is highlighted"
  );

  ok(
    highlighterData.highlighter,
    "The selector highlighter instance was created"
  );
  ok(highlighterData.isShown, "The selector highlighter was shown");

  info(`Clicking on "&#title" selector icon`);
  highlighterData = await clickSelectorIcon(view, "&#title");
  is(
    highlighterData.nodeFront.nodeName.toLowerCase(),
    "h1",
    "<h1> is highlighted"
  );
  ok(
    highlighterData.highlighter,
    "The selector highlighter instance was created"
  );
  ok(highlighterData.isShown, "The selector highlighter was shown");

  info(`Clicking on "&.title" selector icon`);
  highlighterData = await clickSelectorIcon(view, "&.title");
  is(
    highlighterData.nodeFront.nodeName.toLowerCase(),
    "h1",
    "<h1> is highlighted"
  );
  ok(
    highlighterData.highlighter,
    "The selector highlighter instance was created"
  );
  ok(highlighterData.isShown, "The selector highlighter was shown");

  info(`Clicking on ".title" selector icon`);
  highlighterData = await clickSelectorIcon(view, ".title");
  is(
    highlighterData.nodeFront.nodeName.toLowerCase(),
    "h1",
    "<h1> is highlighted"
  );
  ok(
    highlighterData.highlighter,
    "The selector highlighter instance was created"
  );
  ok(highlighterData.isShown, "The selector highlighter was shown");

  await selectNode("h1 em", inspector);
  info(`Clicking on "& em" selector icon`);
  highlighterData = await clickSelectorIcon(view, "& em");
  is(
    highlighterData.nodeFront.nodeName.toLowerCase(),
    "em",
    "<em> is highlighted"
  );
  ok(
    highlighterData.highlighter,
    "The selector highlighter instance was created"
  );
  ok(highlighterData.isShown, "The selector highlighter was shown");

  info(`Clicking on "html &" selector icon`);
  highlighterData = await clickSelectorIcon(view, "html &");
  is(
    highlighterData.nodeFront.nodeName.toLowerCase(),
    "em",
    "<em> is highlighted"
  );
  ok(
    highlighterData.highlighter,
    "The selector highlighter instance was created"
  );
  ok(highlighterData.isShown, "The selector highlighter was shown");
});
