/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that Toolbox#viewSourceInScratchpad works.
 */

var URL = `${URL_ROOT}doc_viewsource.html`;

function *viewSource() {
  let toolbox = yield openNewTabAndToolbox(URL);
  let win = yield openScratchpadWindow();
  let { Scratchpad: scratchpad } = win;

  // Brahm's Cello Sonata No.1, Op.38 now in the scratchpad
  scratchpad.setText("E G B C B\nA B A G A B\nG E");
  let scratchpadURL = scratchpad.uniqueName;

  // Now select another tool for focus
  yield toolbox.selectTool("webconsole");

  yield toolbox.viewSourceInScratchpad(scratchpadURL, 2);

  is(scratchpad.editor.getCursor().line, 2,
    "The correct line is highlighted in scratchpad's editor.");

  win.close();
  yield closeToolboxAndTab(toolbox);
  finish();
}

function test () {
  Task.spawn(viewSource).then(finish, (aError) => {
    ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
    finish();
  });
}
