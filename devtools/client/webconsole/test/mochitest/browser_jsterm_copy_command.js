/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the `copy` console helper works as intended.

"use strict";

const text =
  "Lorem ipsum dolor sit amet, consectetur adipisicing " +
  "elit, sed do eiusmod tempor incididunt ut labore et dolore magna " +
  "aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco " +
  "laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure " +
  "dolor in reprehenderit in voluptate velit esse cillum dolore eu " +
  "fugiat nulla pariatur. Excepteur sint occaecat cupidatat non " +
  "proident, sunt in culpa qui officia deserunt mollit anim id est laborum." +
  new Date();

const id = "select-me";
const TEST_URI = `data:text/html;charset=utf-8,
<body>
  <div>
    <h1>Testing copy command</h1>
    <p>This is some example text</p>
    <p id="${id}">${text}</p>
  </div>
  <div><p></p></div>
</body>`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const random = Math.random();
  const string = "Text: " + random;
  const obj = { a: 1, b: "foo", c: random };

  await testCopy(hud, random, random.toString());
  await testCopy(hud, JSON.stringify(string), string);
  await testCopy(hud, obj.toSource(), JSON.stringify(obj, null, "  "));

  const outerHTML = await ContentTask.spawn(
    gBrowser.selectedBrowser,
    id,
    function(elementId) {
      return content.document.getElementById(elementId).outerHTML;
    }
  );
  await testCopy(hud, `$("#${id}")`, outerHTML);
});

function testCopy(hud, stringToCopy, expectedResult) {
  return waitForClipboardPromise(() => {
    info(`Attempting to copy: "${stringToCopy}"`);
    const command = `copy(${stringToCopy})`;
    info(`Executing command: "${command}"`);
    execute(hud, command);
  }, expectedResult);
}
