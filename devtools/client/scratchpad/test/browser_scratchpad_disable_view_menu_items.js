/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test if the view menu items "Larger Font" and "Smaller Font" are disabled
// when the font size reaches the maximum/minimum values.

function test() {
  const options = {
    tabContent: 'test if view menu items "Larger Font" and "Smaller Font" are enabled/disabled.'
  };
  openTabAndScratchpad(options)
    .then(runTests)
    .then(finish, console.error);
}

async function runTests([win, sp]) {
  await testMaximumFontSize(win, sp);

  await testMinimumFontSize(win, sp);
}

const MAXIMUM_FONT_SIZE = 96;
const MINIMUM_FONT_SIZE = 6;
const NORMAL_FONT_SIZE = 12;

var testMaximumFontSize = async function(win, sp) {
  let doc = win.document;

  Services.prefs.clearUserPref("devtools.scratchpad.editorFontSize");

  let menu = doc.getElementById("sp-menu-larger-font");

  for (let i = NORMAL_FONT_SIZE; i <= MAXIMUM_FONT_SIZE; i++) {
    menu.doCommand();
  }

  let cmd = doc.getElementById("sp-cmd-larger-font");
  ok(cmd.getAttribute("disabled") === "true", 'Command "sp-cmd-larger-font" is disabled.');

  menu = doc.getElementById("sp-menu-smaller-font");
  menu.doCommand();

  ok(cmd.hasAttribute("disabled") === false, 'Command "sp-cmd-larger-font" is enabled.');
};

var testMinimumFontSize = async function(win, sp) {
  let doc = win.document;

  let menu = doc.getElementById("sp-menu-smaller-font");

  for (let i = MAXIMUM_FONT_SIZE; i >= MINIMUM_FONT_SIZE; i--) {
    menu.doCommand();
  }

  let cmd = doc.getElementById("sp-cmd-smaller-font");
  ok(cmd.getAttribute("disabled") === "true", 'Command "sp-cmd-smaller-font" is disabled.');

  menu = doc.getElementById("sp-menu-larger-font");
  menu.doCommand();

  ok(cmd.hasAttribute("disabled") === false, 'Command "sp-cmd-smaller-font" is enabled.');

  Services.prefs.clearUserPref("devtools.scratchpad.editorFontSize");
};
