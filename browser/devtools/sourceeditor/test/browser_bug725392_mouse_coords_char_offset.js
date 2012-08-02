/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test()
{
  let testWin;
  let editor;
  let mousePos = { x: 36, y: 4 };
  let expectedOffset = 5;
  let maxDiff = 10;

  waitForExplicitFinish();

  function editorLoaded(aEditor, aWindow)
  {
    editor = aEditor;
    testWin = aWindow;

    let text = fillEditor(editor, 3);
    editor.setText(text);
    editor.setCaretOffset(0);

    doMouseMove(testPage1);
  }

  function doMouseMove(aCallback)
  {
    function mouseEventHandler(aEvent)
    {
      editor.removeEventListener(editor.EVENTS.MOUSE_OUT, mouseEventHandler);
      editor.removeEventListener(editor.EVENTS.MOUSE_OVER, mouseEventHandler);
      editor.removeEventListener(editor.EVENTS.MOUSE_MOVE, mouseEventHandler);

      executeSoon(aCallback.bind(null, aEvent));
    }

    editor.addEventListener(editor.EVENTS.MOUSE_MOVE, mouseEventHandler);
    editor.addEventListener(editor.EVENTS.MOUSE_OUT, mouseEventHandler);
    editor.addEventListener(editor.EVENTS.MOUSE_OVER, mouseEventHandler);

    let target = editor.editorElement;
    let targetWin = testWin;

    EventUtils.synthesizeMouse(target, mousePos.x, mousePos.y,
                               {type: "mousemove"}, targetWin);
    EventUtils.synthesizeMouse(target, mousePos.x, mousePos.y,
                               {type: "mouseout"}, targetWin);
    EventUtils.synthesizeMouse(target, mousePos.x, mousePos.y,
                               {type: "mouseover"}, targetWin);
  }

  function checkValue(aValue, aExpectedValue)
  {
    let result = Math.abs(aValue - aExpectedValue) <= maxDiff;
    if (!result) {
      info("checkValue() given " + aValue + " expected " + aExpectedValue);
    }
    return result;
  }

  function testPage1(aEvent)
  {
    let {event: { clientX: clientX, clientY: clientY }, x: x, y: y} = aEvent;

    info("testPage1 " + aEvent.type +
         " clientX " + clientX + " clientY " + clientY +
         " x " + x + " y " + y);

    // x and y are in document coordinates.
    // clientX and clientY are in view coordinates.
    // since we are scrolled at the top, both are expected to be approximately
    // the same.
    ok(checkValue(x, mousePos.x), "x is in range");
    ok(checkValue(y, mousePos.y), "y is in range");

    ok(checkValue(clientX, mousePos.x), "clientX is in range");
    ok(checkValue(clientY, mousePos.y), "clientY is in range");

    // we give document-relative coordinates here.
    let offset = editor.getOffsetAtLocation(x, y);
    ok(checkValue(offset, expectedOffset), "character offset is correct");

    let rect = {x: x, y: y};
    let viewCoords = editor.convertCoordinates(rect, "document", "view");
    ok(checkValue(viewCoords.x, clientX), "viewCoords.x is in range");
    ok(checkValue(viewCoords.y, clientY), "viewCoords.y is in range");

    rect = {x: clientX, y: clientY};
    let docCoords = editor.convertCoordinates(rect, "view", "document");
    ok(checkValue(docCoords.x, x), "docCoords.x is in range");
    ok(checkValue(docCoords.y, y), "docCoords.y is in range");

    // we are given document-relative coordinates.
    let offsetPos = editor.getLocationAtOffset(expectedOffset);
    ok(checkValue(offsetPos.x, x), "offsetPos.x is in range");
    ok(checkValue(offsetPos.y, y), "offsetPos.y is in range");

    // Scroll the view and test again.
    let topIndex = Math.round(editor.getLineCount() / 2);
    editor.setTopIndex(topIndex);
    expectedOffset += editor.getLineStart(topIndex);

    executeSoon(doMouseMove.bind(null, testPage2));
  }

  function testPage2(aEvent)
  {
    let {event: { clientX: clientX, clientY: clientY }, x: x, y: y} = aEvent;

    info("testPage2 " + aEvent.type +
         " clientX " + clientX + " clientY " + clientY +
         " x " + x + " y " + y);

    // after page scroll document coordinates need to be different from view
    // coordinates.
    ok(checkValue(x, mousePos.x), "x is not different from clientX");
    ok(!checkValue(y, mousePos.y), "y is different from clientY");

    ok(checkValue(clientX, mousePos.x), "clientX is in range");
    ok(checkValue(clientY, mousePos.y), "clientY is in range");

    // we give document-relative coordinates here.
    let offset = editor.getOffsetAtLocation(x, y);
    ok(checkValue(offset, expectedOffset), "character offset is correct");

    let rect = {x: x, y: y};
    let viewCoords = editor.convertCoordinates(rect, "document", "view");
    ok(checkValue(viewCoords.x, clientX), "viewCoords.x is in range");
    ok(checkValue(viewCoords.y, clientY), "viewCoords.y is in range");

    rect = {x: clientX, y: clientY};
    let docCoords = editor.convertCoordinates(rect, "view", "document");
    ok(checkValue(docCoords.x, x), "docCoords.x is in range");
    ok(checkValue(docCoords.y, y), "docCoords.y is in range");

    // we are given document-relative coordinates.
    let offsetPos = editor.getLocationAtOffset(expectedOffset);
    ok(checkValue(offsetPos.x, x), "offsetPos.x is in range");
    ok(checkValue(offsetPos.y, y), "offsetPos.y is in range");

    executeSoon(testEnd);
  }

  function testEnd()
  {
    if (editor) {
      editor.destroy();
    }
    if (testWin) {
      testWin.close();
    }

    waitForFocus(finish, window);
  }

  openSourceEditorWindow(editorLoaded);
}
