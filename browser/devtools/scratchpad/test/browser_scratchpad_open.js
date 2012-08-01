/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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
  openScratchpad(function(win) {
    is(win.Scratchpad.filename, undefined, "Default filename is undefined");
    isnot(win.Scratchpad.getText(), null, "Default text should not be null");
    is(win.Scratchpad.executionContext, win.SCRATCHPAD_CONTEXT_CONTENT,
      "Default execution context is content");

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
