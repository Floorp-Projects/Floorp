/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */  

// only finish() when correct number of tests are done
const expected = 5;
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
  testErrorStatus();
  testRestoreNotFromFile();
  testRestoreFromFileSaved();
  testRestoreFromFileUnsaved();

  content.location = "data:text/html,<p>test star* UI for unsaved file changes";
}

function testListeners()
{
  let win = ScratchpadManager.openScratchpad();

  win.addEventListener("load", function onScratchpadLoad() {
    win.removeEventListener("load", onScratchpadLoad, false);

    win.Scratchpad.addObserver({
      onReady: function (aScratchpad) {
        aScratchpad.removeObserver(this);

        aScratchpad.setText("new text");
        ok(!isStar(win), "no star if scratchpad isn't from a file");

        aScratchpad.onTextSaved();
        ok(!isStar(win), "no star before changing text");

        aScratchpad.setText("new text2");
        ok(isStar(win), "shows star if scratchpad text changes");

        aScratchpad.onTextSaved();
        ok(!isStar(win), "no star if scratchpad was just saved");

        aScratchpad.undo();
        ok(isStar(win), "star if scratchpad undo");

        win.close();
        done();
      }
    });
  }, false);
}

function testErrorStatus()
{
  let win = ScratchpadManager.openScratchpad();

  win.addEventListener("load", function onScratchpadLoad() {
    win.removeEventListener("load", onScratchpadLoad, false);

    win.Scratchpad.addObserver({
      onReady: function (aScratchpad) {
        aScratchpad.removeObserver(this);

        aScratchpad.onTextSaved(Components.results.NS_ERROR_FAILURE);
        aScratchpad.setText("new text");
        ok(!isStar(win), "no star if file save failed");

        win.close();
        done();
      }
    });
  }, false);
}


function testRestoreNotFromFile()
{
  let session = [{
    text: "test1",
    executionContext: 1
  }];

  let [win] = ScratchpadManager.restoreSession(session);
  win.addEventListener("load", function onScratchpadLoad() {
    win.removeEventListener("load", onScratchpadLoad, false);

    win.Scratchpad.addObserver({
      onReady: function (aScratchpad) {
        aScratchpad.removeObserver(this);

        aScratchpad.setText("new text");
        ok(!isStar(win), "no star if restored scratchpad isn't from a file");

        win.close();
        done();
      }
    });
  }, false);
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
  win.addEventListener("load", function onScratchpadLoad() {
    win.removeEventListener("load", onScratchpadLoad, false);

    win.Scratchpad.addObserver({
      onReady: function (aScratchpad) {
        aScratchpad.removeObserver(this);

        ok(!isStar(win), "no star before changing text in scratchpad restored from file");

        aScratchpad.setText("new text");
        ok(isStar(win), "star when text changed from scratchpad restored from file");

        win.close();
        done();
      }
    });
  }, false);
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
  win.addEventListener("load", function onScratchpadLoad() {
    win.removeEventListener("load", onScratchpadLoad, false);

    win.Scratchpad.addObserver({
      onReady: function (aScratchpad) {
        aScratchpad.removeObserver(this);

        ok(isStar(win), "star with scratchpad restored with unsaved text");

        win.close();
        done();
      }
    });
  }, false);
}

function isStar(win)
{
  return win.document.title.match(/^\*[^\*]/);
}
