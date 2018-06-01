/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that Toolbox#viewSourceInScratchpad works.
 */

var URL = `${URL_ROOT}doc_viewsource.html`;

async function viewSource() {
  const toolbox = await openNewTabAndToolbox(URL);
  const win = await openScratchpadWindow();
  const { Scratchpad: scratchpad } = win;

  // Brahm's Cello Sonata No.1, Op.38 now in the scratchpad
  scratchpad.setText("E G B C B\nA B A G A B\nG E");
  const scratchpadURL = scratchpad.uniqueName;

  // Now select another tool for focus
  await toolbox.selectTool("webconsole");

  await toolbox.viewSourceInScratchpad(scratchpadURL, 2);

  is(scratchpad.editor.getCursor().line, 2,
    "The correct line is highlighted in scratchpad's editor.");

  win.close();
  await closeToolboxAndTab(toolbox);
  finish();
}

function test() {
  viewSource().then(finish, (aError) => {
    ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
    finish();
  });
}
