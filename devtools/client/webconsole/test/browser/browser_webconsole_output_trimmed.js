/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that we trim start and end whitespace in user input
// in the messages list

"use strict";

const TEST_URI = `http://example.com/browser/devtools/client/webconsole/test/browser/test-console.html`;

const TEST_ITEMS = [
  {
    name: "Commands without whitespace are not affected by trimming",
    command: "Math.PI==='3.14159'",
    expected: "Math.PI==='3.14159'",
  },
  {
    name: "Trims whitespace before and after a command (single line case)",
    command: "\t\t    (window.o_O || {})   ['  O_o  ']    ",
    expected: "(window.o_O || {})   ['  O_o  ']",
  },
  {
    name:
      "When trimming a whitespace before and after a command, " +
      "it keeps indentation for each contentful line",
    command: "  \n  \n  1,\n  2,\n  3\n  \n  ",
    expected: "  1,\n  2,\n  3",
  },
  {
    name:
      "When trimming a whitespace before and after a command, " +
      "it keeps trailing whitespace for all lines except the last",
    command:
      "\n" +
      "    let numbers = [1,\n" +
      "                   2,  \n" +
      "                   3];\n" +
      "    \n" +
      "    \n" +
      "    function addNumber() {        \n" +
      "      numbers.push(numbers.length + 1);\n" +
      "    }    \n" +
      "    ",
    expected:
      "    let numbers = [1,\n" +
      "                   2,  \n" +
      "                   3];\n" +
      "    \n" +
      "    \n" +
      "    function addNumber() {        \n" +
      "      numbers.push(numbers.length + 1);\n" +
      "    }",
  },
];

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  // Check that expected output and actual trimmed output match
  for (const { name, command, expected } of TEST_ITEMS) {
    hud.ui.clearOutput();
    await executeAndWaitForMessage(hud, command, "", ".result");

    const result = await waitFor(() => getDisplayedInput(hud));

    if (result === expected) {
      ok(true, name);
    } else {
      ok(false, formatError(name, result, expected));
    }
  }
});

/**
 * Get the text content of the latest command logged in the console
 * @param {WebConsole} hud: The webconsole
 * @return {string|null}
 */
function getDisplayedInput(hud) {
  const message = Array.from(
    hud.ui.outputNode.querySelectorAll(".message.command")
  ).pop();

  if (message) {
    return message.querySelector("syntax-highlighted").textContent;
  }

  return null;
}

/**
 * Format a "Got vs Expected" error message on multiple lines,
 * making whitespace more visible in console output.
 */
function formatError(name, result, expected) {
  const quote = str =>
    typeof str === "string"
      ? "> " + str.replace(/ /g, "\u{B7}").replace(/\n/g, "\n> ")
      : str;

  return `${name}\nGot:\n${quote(result)}\nExpected:\n${quote(expected)}\n`;
}
