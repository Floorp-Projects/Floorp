/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that custom properties are only displayed when they are unregistered,
// or when their property definition indicate that they should inherit.

const TEST_URI = `
  <style>
    @property --inherit {
      syntax: "<color>";
      inherits: true;
      initial-value: gold;
    }

    @property --no-inherit {
      syntax: "<color>";
      inherits: false;
      initial-value: tomato;
    }

    main, [test="no-inherit"] {
      --no-inherit: blue;
    }

    main, [test="inherit"] {
      --inherit: red;
    }

    main, [test="unregistered"] {
      --myvar: brown;
    }

    h1 {
      background-color: var(--no-inherit);
      color: var(--inherit);
      outline-color: var(--myvar);
    }
  </style>
  <main>
    <h1>Hello world</h1>
  </main>
`;

add_task(async function () {
  await pushPref("layout.css.properties-and-values.enabled", true);
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("h1", inspector);

  const gutters = view.element.querySelectorAll(".ruleview-header");
  is(gutters.length, 1, "There's one inherited section header");
  is(
    gutters[0].textContent,
    "Inherited from main",
    "The header is the expected inherited one"
  );

  const inheritedRules = view.element.querySelectorAll(
    ".ruleview-header ~ .ruleview-rule"
  );
  is(inheritedRules.length, 2, "There are 2 inherited rules displayed");

  info("Check that registered inherits property is visible");
  is(
    getRuleViewPropertyValue(view, `main, [test="inherit"]`, "--inherit"),
    "red",
    "--inherit definition on main is visible"
  );

  info("Check that unregistered property is visible");
  is(
    getRuleViewPropertyValue(view, `main, [test="unregistered"]`, "--myvar"),
    "brown",
    "--myvar definition on main is displayed"
  );

  info("Check that registered non-inherits property is not visible");
  // The no-inherit rule only has 1 definition that should be hidden, which means
  // that the whole rule should be hidden
  ok(
    !getRuleViewRule(view, `main, [test="no-inherit"]`),
    "The rule with the not inherited registered property is not displayed"
  );
});
