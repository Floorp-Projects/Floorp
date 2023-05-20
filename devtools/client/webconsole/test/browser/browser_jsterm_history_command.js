/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests if the command history shows a table with the content we expected.

"use strict";

const TEST_URI = "data:text/html;charset=UTF-8,<!DOCTYPE html>test";
const COMMANDS = ["document", "window", "window.location"];

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  jsterm.focus();

  for (const command of COMMANDS) {
    info(`Executing command ${command}`);
    await executeAndWaitForResultMessage(hud, command, "");
  }

  info(`Executing command :history`);
  await executeAndWaitForMessageByType(hud, ":history", "", ".simpleTable");
  const historyTableRows = hud.ui.outputNode.querySelectorAll(
    ".message.simpleTable tbody tr"
  );

  const expectedCommands = [...COMMANDS, ":history"];

  for (let i = 0; i < historyTableRows.length; i++) {
    const cells = historyTableRows[i].querySelectorAll("td");

    is(
      cells[0].textContent,
      String(i),
      "Check the value of the column (index)"
    );
    is(
      cells[1].textContent,
      expectedCommands[i],
      "Check if the value of the column Expressions is the value expected"
    );
  }
});
