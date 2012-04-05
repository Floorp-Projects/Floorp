/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */  

// only finish() when correct number of tests are done
const expected = 4;
var count = 0;
function done()
{
  if (++count == expected) {
    finish();
  }
}

var ScratchpadManager = Scratchpad.ScratchpadManager;


function test()
{
  waitForExplicitFinish();
  
  testListeners();
  testRestoreNotFromFile();
  testRestoreFromFileSaved();
  testRestoreFromFileUnsaved();

  gBrowser.selectedTab = gBrowser.addTab();
  content.location = "data:text/html,<p>test star* UI for unsaved file changes";
}

function testListeners()
{
  openScratchpad(function(aWin, aScratchpad) {
    aScratchpad.setText("new text");
    ok(!isStar(aWin), "no star if scratchpad isn't from a file");

    aScratchpad.editor.dirty = false;
    ok(!isStar(aWin), "no star before changing text");

    aScratchpad.setFilename("foo.js");
    aScratchpad.setText("new text2");
    ok(isStar(aWin), "shows star if scratchpad text changes");

    aScratchpad.editor.dirty = false;
    ok(!isStar(aWin), "no star if scratchpad was just saved");

    aScratchpad.setText("new text3");
    ok(isStar(aWin), "shows star if scratchpad has more changes");

    aScratchpad.undo();
    ok(!isStar(aWin), "no star if scratchpad undo to save point");

    aScratchpad.undo();
    ok(isStar(aWin), "star if scratchpad undo past save point");

    aWin.close();
    done();
  }, {noFocus: true});
}

function testRestoreNotFromFile()
{
  let session = [{
    text: "test1",
    executionContext: 1
  }];

  let [win] = ScratchpadManager.restoreSession(session);
  openScratchpad(function(aWin, aScratchpad) {
    aScratchpad.setText("new text");
    ok(!isStar(win), "no star if restored scratchpad isn't from a file");

    win.close();
    done();
  }, {window: win, noFocus: true});
}

function testRestoreFromFileSaved()
{
  let session = [{
    filename: "test.js",
    text: "test1",
    executionContext: 1,
    saved: true
  }];

  let [win] = ScratchpadManager.restoreSession(session);
  openScratchpad(function(aWin, aScratchpad) {
    ok(!isStar(win), "no star before changing text in scratchpad restored from file");

    aScratchpad.setText("new text");
    ok(isStar(win), "star when text changed from scratchpad restored from file");

    win.close();
    done();
  }, {window: win, noFocus: true});
}

function testRestoreFromFileUnsaved()
{
  let session = [{
    filename: "test.js",
    text: "test1",
    executionContext: 1,
    saved: false
  }];

  let [win] = ScratchpadManager.restoreSession(session);
  openScratchpad(function() {
    ok(isStar(win), "star with scratchpad restored with unsaved text");

    win.close();
    done();
  }, {window: win, noFocus: true});
}

function isStar(win)
{
  return win.document.title.match(/^\*[^\*]/);
}
