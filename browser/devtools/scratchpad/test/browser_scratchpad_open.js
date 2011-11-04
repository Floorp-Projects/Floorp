/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var ScratchpadManager = Scratchpad.ScratchpadManager;

// only finish() when correct number of tests are done
const expected = 3;
var count = 0;

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
}

function testOpen()
{
  let win = ScratchpadManager.openScratchpad();

  win.addEventListener("load", function() {
    is(win.Scratchpad.filename, undefined, "Default filename is undefined");
    is(win.Scratchpad.getText(),
       win.Scratchpad.strings.GetStringFromName("scratchpadIntro"),
       "Default text is loaded")
    is(win.Scratchpad.executionContext, win.SCRATCHPAD_CONTEXT_CONTENT,
      "Default execution context is content");

    win.close();
    done();
  });
}

function testOpenWithState()
{
  let state = {
    filename: "testfile",
    executionContext: 2,
    text: "test text"
  };

  let win = ScratchpadManager.openScratchpad(state);

  win.addEventListener("load", function() {
    is(win.Scratchpad.filename, state.filename, "Filename loaded from state");
    is(win.Scratchpad.executionContext, state.executionContext, "Execution context loaded from state");
    is(win.Scratchpad.getText(), state.text, "Content loaded from state");

    win.close();
    done();
  });
}

function testOpenInvalidState()
{
  let state = 7;

  let win = ScratchpadManager.openScratchpad(state);
  ok(!win, "no scratchpad opened if state is not an object");
  done();
}
