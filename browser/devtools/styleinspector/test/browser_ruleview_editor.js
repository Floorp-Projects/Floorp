/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let doc = content.document;

function expectDone(aValue, aCommit, aNext)
{
  return function(aDoneValue, aDoneCommit) {
    dump("aDoneValue: " + aDoneValue + " commit: " + aDoneCommit + "\n");

    is(aDoneValue, aValue, "Should get expected value");
    is(aDoneCommit, aDoneCommit, "Should get expected commit value");
    aNext();
  }
}

function clearBody()
{
  doc.body.innerHTML = "";
}

function createSpan()
{
  let span = doc.createElement("span");
  span.setAttribute("tabindex", "0");
  span.textContent = "Edit Me!";
  doc.body.appendChild(span);
  return span;
}

function testReturnCommit()
{
  clearBody();
  let span = createSpan();
  editableField({
    element: span,
    initial: "explicit initial",
    start: function() {
      is(inplaceEditor(span).input.value, "explicit initial", "Explicit initial value should be used.");
      inplaceEditor(span).input.value = "Test Value";
      EventUtils.sendKey("return");
    },
    done: expectDone("Test Value", true, testBlurCommit)
  });
  span.click();
}

function testBlurCommit()
{
  clearBody();
  let span = createSpan();
  editableField({
    element: span,
    start: function() {
      is(inplaceEditor(span).input.value, "Edit Me!", "textContent of the span used.");
      inplaceEditor(span).input.value = "Test Value";
      inplaceEditor(span).input.blur();
    },
    done: expectDone("Test Value", true, testAdvanceCharCommit)
  });
  span.click();
}

function testAdvanceCharCommit()
{
  clearBody();
  let span = createSpan();
  editableField({
    element: span,
    advanceChars: ":",
    start: function() {
      let input = inplaceEditor(span).input;
      for each (let ch in "Test:") {
        EventUtils.sendChar(ch);
      }
    },
    done: expectDone("Test", true, testEscapeCancel)
  });
  span.click();
}

function testEscapeCancel()
{
  clearBody();
  let span = createSpan();
  editableField({
    element: span,
    initial: "initial text",
    start: function() {
      inplaceEditor(span).input.value = "Test Value";
      EventUtils.sendKey("escape");
    },
    done: expectDone("initial text", false, finishTest)
  });
  span.click();
}


function finishTest()
{
  doc = null;
  gBrowser.removeCurrentTab();
  finish();
}


function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee, true);
    doc = content.document;
    waitForFocus(testReturnCommit, content);
  }, true);

  content.location = "data:text/html,inline editor tests";
}
