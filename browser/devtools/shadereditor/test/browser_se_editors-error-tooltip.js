/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if error tooltips can be opened from the editor's gutter when there's
 * a shader compilation error.
 */

function ifWebGLSupported() {
  let [target, debuggee, panel] = yield initShaderEditor(SIMPLE_CANVAS_URL);
  let { gFront, EVENTS, ShadersEditorsView } = panel.panelWin;

  reload(target);
  yield once(gFront, "program-linked");

  let vsEditor = yield ShadersEditorsView._getEditor("vs");
  let fsEditor = yield ShadersEditorsView._getEditor("fs");

  vsEditor.replaceText("vec3", { line: 7, ch: 22 }, { line: 7, ch: 26 });
  yield once(panel.panelWin, EVENTS.SHADER_COMPILED);

  // Synthesizing 'mouseenter' events doesn't work, hack around this by
  // manually calling the event listener with the expected arguments.
  let editorDocument = vsEditor.container.contentDocument;
  let marker = editorDocument.querySelector(".error");
  let parsed = ShadersEditorsView._errors.vs[0].messages;
  ShadersEditorsView._onMarkerMouseEnter(7, marker, parsed);

  let tooltip = marker._markerErrorsTooltip;
  ok(tooltip, "A tooltip was created successfully.");

  let content = tooltip.content;
  ok(tooltip.content,
    "Some tooltip's content was set.");
  is(tooltip.content.className, "devtools-tooltip-simple-text-container",
    "The tooltip's content container was created correctly.");

  let messages = content.childNodes;
  is(messages.length, 2,
    "There are two messages displayed in the tooltip.");
  is(messages[0].className, "devtools-tooltip-simple-text",
    "The first message was created correctly.");
  is(messages[1].className, "devtools-tooltip-simple-text",
    "The second message was created correctly.");

  ok(messages[0].textContent.contains("'constructor' : too many arguments"),
    "The first message contains the correct text.");
  ok(messages[1].textContent.contains("'assign' : cannot convert"),
    "The second message contains the correct text.");

  yield teardown(panel);
  finish();
}

function once(aTarget, aEvent) {
  let deferred = promise.defer();
  aTarget.once(aEvent, (aName, aData) => deferred.resolve(aData));
  return deferred.promise;
}
