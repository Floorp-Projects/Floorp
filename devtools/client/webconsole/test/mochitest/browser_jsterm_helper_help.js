/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html,Test <code>help()</code> jsterm helper";
const HELP_URL = "https://developer.mozilla.org/docs/Tools/Web_Console/Helpers";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const jsterm = hud.jsterm;

  let openedLinks = 0;
  const oldOpenLink = hud.openLink;
  hud.openLink = (url) => {
    if (url == HELP_URL) {
      openedLinks++;
    }
  };

  hud.ui.clearOutput();
  await jsterm.execute("help()");
  await jsterm.execute("help");
  await jsterm.execute("?");

  const messages = Array.from(jsterm.outputNode.querySelectorAll(".message"));
  ok(messages.every(msg => msg.classList.contains("command")),
    "There is no results shown for the help commands");
  is(openedLinks, 3, "correct number of pages opened by the help calls");
  hud.openLink = oldOpenLink;
});
