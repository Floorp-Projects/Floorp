/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// only finish() when correct number of tests are done
const expected = 4;
var count = 0;
var lastUniqueName = null;

function done()
{
  if (++count == expected) {
    finish();
  }
}

function test()
{
  waitForExplicitFinish();
  testOpen();
  testOpenWithState();
  testOpenInvalidState();
  testOpenTestFile();
}

function testUniqueName(name)
{
  ok(name, "Scratchpad has a uniqueName");

  if (lastUniqueName === null) {
    lastUniqueName = name;
    return;
  }

  ok(name !== lastUniqueName,
      "Unique name for this instance differs from the last one.");
}

function testOpen()
{
  openScratchpad(function(win) {
    is(win.Scratchpad.filename, undefined, "Default filename is undefined");
    isnot(win.Scratchpad.getText(), null, "Default text should not be null");
    is(win.Scratchpad.executionContext, win.SCRATCHPAD_CONTEXT_CONTENT,
      "Default execution context is content");
    testUniqueName(win.Scratchpad.uniqueName);

    win.close();
    done();
  }, {noFocus: true});
}

function testOpenWithState()
{
  let state = {
    filename: "testfile",
    executionContext: 2,
    text: "test text"
  };

  openScratchpad(function(win) {
    is(win.Scratchpad.filename, state.filename, "Filename loaded from state");
    is(win.Scratchpad.executionContext, state.executionContext, "Execution context loaded from state");
    is(win.Scratchpad.getText(), state.text, "Content loaded from state");
    testUniqueName(win.Scratchpad.uniqueName);

    win.close();
    done();
  }, {state: state, noFocus: true});
}

function testOpenInvalidState()
{
  let win = openScratchpad(null, {state: 7});
  ok(!win, "no scratchpad opened if state is not an object");
  done();
}

function testOpenTestFile()
{
  let win = openScratchpad(function(win) {
    ok(win, "scratchpad opened for file open");
    try {
      win.Scratchpad.importFromFile(
        "http://example.com/browser/devtools/client/scratchpad/test/NS_ERROR_ILLEGAL_INPUT.txt",
        "silent",
        function (aStatus, content) {
          let nb = win.document.querySelector('#scratchpad-notificationbox');
          is(nb.querySelectorAll('notification').length, 1, "There is just one notification");
          let cn = nb.currentNotification;
          is(cn.priority, nb.PRIORITY_WARNING_HIGH, "notification priority is correct");
          is(cn.value, "file-import-convert-failed", "notification value is corrent");
          is(cn.type, "warning", "notification type is correct");
          done();
        });
      ok(true, "importFromFile does not cause exception");
    } catch (exception) {
      ok(false, "importFromFile causes exception " + DevToolsUtils.safeErrorString(exception));
    }
  }, {noFocus: true});
}
