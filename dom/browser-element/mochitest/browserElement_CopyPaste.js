/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that "cut, copy, paste, selectall" and selectionstatechanged event works from inside an <iframe mozbrowser>.
"use strict";

SimpleTest.waitForExplicitFinish();
SimpleTest.requestFlakyTimeout("untriaged");
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.setSelectionChangeEnabledPref(true);
browserElementTestHelpers.addPermission();
const { Services } = SpecialPowers.Cu.import('resource://gre/modules/Services.jsm');
var gTextarea = null;
var mm;
var iframe;
var state = 0;
var stateMeaning;
var defaultData;
var pasteData;
var focusScript;

function copyToClipboard(str) {
  gTextarea.value = str;
  SpecialPowers.wrap(gTextarea).editor.selectAll();
  SpecialPowers.wrap(gTextarea).editor.copy();
}

function getScriptForGetContent() {
  var script = 'data:,\
    var elt = content.document.getElementById("text"); \
    var txt = ""; \
    if (elt) { \
      if (elt.tagName === "DIV" || elt.tagName === "BODY") { \
        txt = elt.textContent; \
      } else { \
        txt = elt.value; \
      } \
    } \
    sendAsyncMessage("content-text", txt);';
  return script;
}

function getScriptForSetFocus() {
  var script = 'data:,' + focusScript + 'sendAsyncMessage("content-focus")';
  return script;
}

function runTest() {
  iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');
  document.body.appendChild(iframe);

  gTextarea = document.createElement('textarea');
  document.body.appendChild(gTextarea);

  mm = SpecialPowers.getBrowserFrameMessageManager(iframe);

  iframe.addEventListener("mozbrowserloadend", function onloadend(e) {
    iframe.removeEventListener("mozbrowserloadend", onloadend);
    dispatchTest(e);
  });
}

function doCommand(cmd) {
  Services.obs.notifyObservers({wrappedJSObject: iframe},
                               'copypaste-docommand', cmd);
}

function dispatchTest(e) {
  iframe.addEventListener("mozbrowserloadend", function onloadend2(e) {
    iframe.removeEventListener("mozbrowserloadend", onloadend2);
    iframe.focus();
    SimpleTest.executeSoon(function() { testSelectAll(e); });
  });

  switch (state) {
    case 0: // test for textarea
      defaultData = "Test for selection change event";
      pasteData = "from parent ";
      iframe.src = "data:text/html,<html><body>" +
                   "<textarea id='text'>" + defaultData + "</textarea>" +
                   "</body>" +
                   "</html>";
      stateMeaning = " (test: textarea)";
      focusScript = "var elt=content.document.getElementById('text');elt.focus();elt.select();";
      break;
    case 1: // test for input text
      defaultData = "Test for selection change event";
      pasteData = "from parent ";
      iframe.src = "data:text/html,<html><body>" +
                   "<input type='text' id='text' value='" + defaultData + "'>" +
                   "</body>" +
                   "</html>";
      stateMeaning = " (test: <input type=text>)";
      focusScript = "var elt=content.document.getElementById('text');elt.focus();elt.select();";
      break;
    case 2: // test for input number
      defaultData = "12345";
      pasteData = "67890";
      iframe.src = "data:text/html,<html><body>" +
                   "<input type='number' id='text' value='" + defaultData + "'>" +
                   "</body>" +
                   "</html>";
      stateMeaning = " (test: <input type=number>)";
      focusScript = "var elt=content.document.getElementById('text');elt.focus();elt.select();";
      break;
    case 3: // test for div contenteditable
      defaultData = "Test for selection change event";
      pasteData = "from parent ";
      iframe.src = "data:text/html,<html><body>" +
                   "<div contenteditable='true' id='text'>" + defaultData + "</div>" +
                   "</body>" +
                   "</html>";
      stateMeaning = " (test: content editable div)";
      focusScript = "var elt=content.document.getElementById('text');elt.focus();";
      break;
    case 4: // test for normal div
      SimpleTest.finish();
      return;
      defaultData = "Test for selection change event";
      pasteData = "from parent ";
      iframe.src = "data:text/html,<html><body>" +
                   "<div id='text'>" + defaultData + "</div>" +
                   "</body>" +
                   "</html>";
      stateMeaning = " (test: normal div)";
      focusScript = "var elt=content.document.getElementById('text');elt.focus();";
      break;
    case 5: // test for normal div with designMode:on
      defaultData = "Test for selection change event";
      pasteData = "from parent ";
      iframe.src = "data:text/html,<html><body id='text'>" +
                   defaultData +
                   "</body>" +
                   "<script>document.designMode='on';</script>" +
                   "</html>";
      stateMeaning = " (test: normal div with designMode:on)";
      focusScript = "var elt=content.document.getElementById('text');elt.focus();";
      break;
    default:
      SimpleTest.finish();
      break;
  }
}

function testSelectAll(e) {
  iframe.addEventListener("mozbrowserselectionstatechanged", function selectchangeforselectall(e) {
    iframe.removeEventListener("mozbrowserselectionstatechanged", selectchangeforselectall, true);
    ok(true, "got mozbrowserselectionstatechanged event." + stateMeaning);
    ok(e.detail, "event.detail is not null." + stateMeaning);
    ok(e.detail.width != 0, "event.detail.width is not zero" + stateMeaning);
    ok(e.detail.height != 0, "event.detail.height is not zero" + stateMeaning);
    SimpleTest.executeSoon(function() { testCopy1(e); });
  }, true);

  mm.addMessageListener('content-focus', function messageforfocus(msg) {
    mm.removeMessageListener('content-focus', messageforfocus);
    // test selectall command, after calling this the selectionstatechanged event should be fired.
    doCommand('selectall');
  });

  mm.loadFrameScript(getScriptForSetFocus(), false);
}

function testCopy1(e) {
  // Right now we're at "selectall" state, so we can test copy commnad by
  // calling doCommand
  copyToClipboard("");
  let setup = function() {
    doCommand("copy");
  };

  let nextTest = function(success) {
    ok(success, "copy command works" + stateMeaning);
    SimpleTest.executeSoon(function() { testPaste1(e); });
  };

  let success = function() {
    nextTest(true);
  }

  let fail = function() {
    nextTest(false);
  }

  let compareData = defaultData;
  SimpleTest.waitForClipboard(compareData, setup, success, fail);
}

function testPaste1(e) {
  // Next test paste command, first we copy to global clipboard in parent side.
  // Then paste it to child side.
  copyToClipboard(pasteData);

  doCommand('selectall');
  doCommand("paste");
  SimpleTest.executeSoon(function() { testPaste2(e); });
}

function testPaste2(e) {
  mm.addMessageListener('content-text', function messageforpaste(msg) {
    mm.removeMessageListener('content-text', messageforpaste);
    if (state == 4) {
      // normal div cannot paste, so the content remain unchange
      ok(SpecialPowers.wrap(msg).json === defaultData, "paste command works" + stateMeaning);
    } else if (state == 3 && browserElementTestHelpers.getOOPByDefaultPref()) {
      // Something weird when we doCommand with content editable element in OOP. Mark this case as todo
      todo(false, "paste command works" + stateMeaning);
    } else {
      ok(SpecialPowers.wrap(msg).json === pasteData, "paste command works" + stateMeaning);
    }
    SimpleTest.executeSoon(function() { testCut1(e); });
  });

  mm.loadFrameScript(getScriptForGetContent(), false);
}

function testCut1(e) {
  // Clean clipboard first
  copyToClipboard("");
  let setup = function() {
    doCommand("selectall");
    doCommand("cut");
  };

  let nextTest = function(success) {
    if (state == 3 && browserElementTestHelpers.getOOPByDefaultPref()) {
      // Something weird when we doCommand with content editable element in OOP.
      todo(false, "cut function works" + stateMeaning);
    } else {
      ok(success, "cut function works" + stateMeaning);
    }
    SimpleTest.executeSoon(function() { testCut2(e); });
  };

  let success = function() {
    nextTest(true);
  }

  let fail = function() {
    nextTest(false);
  }

  let compareData = pasteData;
  if (state == 3 && browserElementTestHelpers.getOOPByDefaultPref()) {
    // Something weird when we doCommand with content editable element in OOP.
    // Always true in this case
    compareData = function() { return true; }
  }

  SimpleTest.waitForClipboard(compareData, setup, success, fail);
}

function testCut2(e) {
  mm.addMessageListener('content-text', function messageforcut(msg) {
    mm.removeMessageListener('content-text', messageforcut);
    // normal div cannot cut
    if (state == 4) {
      ok(SpecialPowers.wrap(msg).json !== "", "cut command works" + stateMeaning);
    } else if (state == 3 && browserElementTestHelpers.getOOPByDefaultPref()) {
      // Something weird when we doCommand with content editable element in OOP. Mark this case as todo
      todo(false, "cut command works" + stateMeaning);
    } else {
      ok(SpecialPowers.wrap(msg).json === "", "cut command works" + stateMeaning);
    }

    state++;
    dispatchTest(e);
  });

  mm.loadFrameScript(getScriptForGetContent(), false);
}

addEventListener('testready', runTest);
