/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter works properly for @media / @layer rules.
// The document uses selectors so we can identify rule more easily
const TEST_URI = `
  <!DOCTYPE html>
    <style type='text/css'>
      h1, simple {
        color: tomato;
      }
      @layer {
        h1, anonymous {
          color: tomato;
        }
      }
      @layer myLayer {
        h1, named {
          color: tomato;
        }
      }
      @media screen {
        h1, skreen {
          color: tomato;
        }
      }
      @layer {
        @layer myLayer {
          @media (min-width: 1px) {
            @media (min-height: 1px) {
              h1, nested {
                color: tomato;
              }
            }
          }
        }
      }
    </style>
    <h1>Hello Mochi</h1>`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("h1", inspector);

  info(`Check initial state and rules order`);
  await checkRuleView(view, {
    rules: [
      { selector: "h1, skreen", highlighted: [] },
      { selector: "h1, simple", highlighted: [] },
      { selector: "h1, nested", highlighted: [] },
      { selector: "h1, named", highlighted: [] },
      { selector: "h1, anonymous", highlighted: [] },
    ],
  });

  info(`Check filtering on "layer"`);
  await setSearchFilter(view, `layer`);
  await checkRuleView(view, {
    rules: [
      { selector: "h1, nested", highlighted: ["@layer", "@layer myLayer"] },
      { selector: "h1, named", highlighted: ["@layer myLayer"] },
      { selector: "h1, anonymous", highlighted: ["@layer"] },
    ],
  });

  info(`Check filtering on "@layer"`);
  await setNewSearchFilter(view, `@layer`);
  await checkRuleView(view, {
    rules: [
      { selector: "h1, nested", highlighted: ["@layer", "@layer myLayer"] },
      { selector: "h1, named", highlighted: ["@layer myLayer"] },
      { selector: "h1, anonymous", highlighted: ["@layer"] },
    ],
  });

  info("Check filtering on exact `@layer`");
  await setNewSearchFilter(view, "`@layer`");
  await checkRuleView(view, {
    rules: [
      { selector: "h1, nested", highlighted: ["@layer"] },
      { selector: "h1, anonymous", highlighted: ["@layer"] },
    ],
  });

  info(`Check filtering on layer name "myLayer"`);
  await setNewSearchFilter(view, `myLayer`);
  await checkRuleView(view, {
    rules: [
      { selector: "h1, nested", highlighted: ["@layer myLayer"] },
      { selector: "h1, named", highlighted: ["@layer myLayer"] },
    ],
  });

  info(`Check filtering on "@layer myLayer"`);
  await setNewSearchFilter(view, `@layer myLayer`);
  await checkRuleView(view, {
    rules: [
      { selector: "h1, nested", highlighted: ["@layer myLayer"] },
      { selector: "h1, named", highlighted: ["@layer myLayer"] },
    ],
  });

  info(`Check filtering on "media"`);
  await setNewSearchFilter(view, `media`);
  await checkRuleView(view, {
    rules: [
      { selector: "h1, skreen", highlighted: ["@media screen"] },
      {
        selector: "h1, nested",
        highlighted: ["@media (min-width: 1px)", "@media (min-height: 1px)"],
      },
    ],
  });

  info(`Check filtering on "@media"`);
  await setNewSearchFilter(view, `@media`);
  await checkRuleView(view, {
    rules: [
      { selector: "h1, skreen", highlighted: ["@media screen"] },
      {
        selector: "h1, nested",
        highlighted: ["@media (min-width: 1px)", "@media (min-height: 1px)"],
      },
    ],
  });

  info(`Check filtering on media query content "1px"`);
  await setNewSearchFilter(view, `1px`);
  await checkRuleView(view, {
    rules: [
      {
        selector: "h1, nested",
        highlighted: ["@media (min-width: 1px)", "@media (min-height: 1px)"],
      },
    ],
  });

  info(`Check filtering on media query content "height"`);
  await setNewSearchFilter(view, `height`);
  await checkRuleView(view, {
    rules: [
      {
        selector: "h1, nested",
        highlighted: ["@media (min-height: 1px)"],
      },
    ],
  });

  info("Check filtering on exact `@media`");
  await setNewSearchFilter(view, "`@media`");
  await checkRuleView(view, {
    rules: [],
  });
});

async function checkRuleView(view, { rules }) {
  info("Check that the correct rules are visible");

  const rulesInView = Array.from(view.element.children);
  // The `element` "rule" is never filtered, so remove it from the list of element we check.
  rulesInView.shift();

  is(rulesInView.length, rules.length, "All expected rules are displayed");

  for (let i = 0; i < rulesInView.length; i++) {
    const rule = rulesInView[i];
    const selector = rule.querySelector(
      ".ruleview-selectors-container"
    ).innerText;
    is(selector, rules[i]?.selector, `Expected selector at index ${i}`);

    const highlightedElements = Array.from(
      rule.querySelectorAll(".ruleview-highlight")
    ).map(el => el.innerText);
    Assert.deepEqual(
      highlightedElements,
      rules[i]?.highlighted,
      "The expected ancestor rules information element are highlighted"
    );
  }
}

async function setNewSearchFilter(view, newSearchText) {
  const win = view.styleWindow;
  const searchClearButton = view.searchClearButton;

  const onRuleViewCleared = view.inspector.once("ruleview-filtered");
  EventUtils.synthesizeMouseAtCenter(searchClearButton, {}, win);
  await onRuleViewCleared;

  await setSearchFilter(view, newSearchText);
}
