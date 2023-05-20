/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests filters.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console-filter-groups.html";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  await waitFor(
    () => findConsoleAPIMessage(hud, "[a]") && findConsoleAPIMessage(hud, "[j]")
  );

  /*
   * The output looks like the following:
   *   ▼[a]
   *   |   [b]
   *   |   [c]
   *   | ▼[d]
   *   | |   [e]
   *   |   [f]
   *   |   [g]
   *   [h]
   *   [i]
   *   ▶︎[j]
   *   ▼[group]
   *   | ▼[subgroup]
   *   | |  [subitem]
   */

  await setFilterState(hud, {
    text: "[",
  });

  checkMessages(
    hud,
    `
    ▼[a]
    |  [b]
    |  [c]
    | ▼[d]
    | |  [e]
    |  [f]
    |  [g]
    [h]
    [i]
    ▶︎[j]
    ▼[group]
    | ▼[subgroup]
    | |    [subitem]
    `,
    `Got all expected messages when filtering on common character "["`
  );

  info("Check that filtering on a group substring shows all children");
  await setFilterState(hud, {
    text: "[a]",
  });

  checkMessages(
    hud,
    `
    ▼[a]
    |  [b]
    |  [c]
    | ▼[d]
    | |  [e]
    |  [f]
    |  [g]
    `,
    `Got all children of group when filtering on "[a]"`
  );

  info(
    "Check that filtering on a group child substring shows the parent group message"
  );
  await setFilterState(hud, {
    text: "[b]",
  });

  checkMessages(
    hud,
    `
    ▼[a]
    |  [b]
    `,
    `Got matching message and parent group when filtering on "[b]"`
  );

  info(
    "Check that filtering on a sub-group substring shows subgroup children and parent group"
  );
  await setFilterState(hud, {
    text: "[d]",
  });

  checkMessages(
    hud,
    `
    ▼[a]
    | ▼[d]
    | |  [e]
    `,
    `Got matching message, subgroup children and parent group when filtering on "[d]"`
  );

  info("Check that filtering on a sub-group child shows all parent groups");
  await setFilterState(hud, {
    text: "[e]",
  });

  checkMessages(
    hud,
    `
    ▼[a]
    | ▼[d]
    | |  [e]
    `,
    `Got matching message and parent groups when filtering on "[e]"`
  );

  info(
    "Check that filtering a message in a collapsed group shows the parent group"
  );
  await setFilterState(hud, {
    text: "[k]",
  });

  checkMessages(
    hud,
    `
    [j]
    `,
    `Got collapsed group when filtering on "[k]"`
  );

  info("Check that filtering a message not in a group hides all groups");
  await setFilterState(hud, {
    text: "[h]",
  });

  checkMessages(
    hud,
    `
    [h]
    `,
    `Got only matching message when filtering on "[h]"`
  );

  await setFilterState(hud, {
    text: "",
  });

  const groupA = await findMessageVirtualizedByType({
    hud,
    text: "[a]",
    typeSelector: ".console-api",
  });
  const groupJ = await findMessageVirtualizedByType({
    hud,
    text: "[j]",
    typeSelector: ".console-api",
  });

  toggleGroup(groupA);
  toggleGroup(groupJ);

  checkMessages(
    hud,
    `▶︎[a]
    [h]
    [i]
    ▼[j]
    |  [k]
    ▼[group]
    | ▼[subgroup]
    | |    [subitem]`,
    `Got matching messages after collapsing and expanding group messages`
  );

  info(
    "Check that filtering on expanded groupCollapsed messages does not hide children"
  );
  await setFilterState(hud, {
    text: "[k]",
  });

  checkMessages(
    hud,
    `
    ▼[j]
    |  [k]
    `,
    `Got only matching message when filtering on "[k]"`
  );

  info(
    "Check that filtering on collapsed group messages shows only the group parent"
  );

  await setFilterState(hud, {
    text: "[e]",
  });

  checkMessages(
    hud,
    `
    [a]
    `,
    `Got only matching message when filtering on "[e]"`
  );

  info(
    "Check that filtering on collapsed, nested group messages shows only expaded ancestor"
  );

  // We clear the filter so subgroup is visible and can be toggled
  await setFilterState(hud, {
    text: "",
  });

  const subGroup = findConsoleAPIMessage(hud, "[subgroup]");
  toggleGroup(subGroup);

  await setFilterState(hud, {
    text: "[subitem",
  });

  checkMessages(
    hud,
    `
    ▼[group]
    |  ▶︎[subgroup]
    `,
    `Got only visible ancestors when filtering on "[subitem"`
  );
});

/**
 *
 * @param {WebConsole} hud
 * @param {String} expected
 * @param {String} assertionMessage
 */
function checkMessages(hud, expected, assertionMessage) {
  const expectedMessages = expected
    .split("\n")
    .filter(line => line.trim())
    .map(line => line.replace(/(▶︎|▼|\|)/g, "").trim());

  const messages = Array.from(
    hud.ui.outputNode.querySelectorAll(".message .message-body")
  ).map(el => el.innerText.trim());

  const formatMessages = arr => `\n${arr.join("\n")}\n`;

  is(
    formatMessages(messages),
    formatMessages(expectedMessages),
    assertionMessage
  );
}

function toggleGroup(node) {
  const toggleArrow = node.querySelector(".collapse-button");
  toggleArrow.click();
}
