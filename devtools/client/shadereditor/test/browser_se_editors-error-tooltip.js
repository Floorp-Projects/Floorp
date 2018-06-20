/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if error tooltips can be opened from the editor's gutter when there's
 * a shader compilation error.
 */

async function ifWebGLSupported() {
  const { target, panel } = await initShaderEditor(SIMPLE_CANVAS_URL);
  const { front, EVENTS, shadersEditorsView } = panel;

  reload(target);
  await promise.all([
    once(front, "program-linked"),
    once(panel, EVENTS.SOURCES_SHOWN)
  ]);

  const vsEditor = await shadersEditorsView._getEditor("vs");
  const fsEditor = await shadersEditorsView._getEditor("fs");

  vsEditor.replaceText("vec3", { line: 7, ch: 22 }, { line: 7, ch: 26 });
  await once(panel, EVENTS.SHADER_COMPILED);

  // Synthesizing 'mouseover' events doesn't work, hack around this by
  // manually calling the event listener with the expected arguments.
  const editorDocument = vsEditor.container.contentDocument;
  const marker = editorDocument.querySelector(".error");
  const parsed = shadersEditorsView._errors.vs[0].messages;
  shadersEditorsView._onMarkerMouseOver(7, marker, parsed);

  const tooltip = marker._markerErrorsTooltip;
  ok(tooltip, "A tooltip was created successfully.");

  const content = tooltip.content;
  ok(tooltip.content,
    "Some tooltip's content was set.");
  ok(tooltip.content.className.includes("devtools-tooltip-simple-text-container"),
    "The tooltip's content container was created correctly.");

  const messages = content.childNodes;
  is(messages.length, 3,
    "There are three messages displayed in the tooltip.");
  ok(messages[0].className.includes("devtools-tooltip-simple-text"),
    "The first message was created correctly.");
  ok(messages[1].className.includes("devtools-tooltip-simple-text"),
    "The second message was created correctly.");
  ok(messages[2].className.includes("devtools-tooltip-simple-text"),
    "The third message was created correctly.");

  ok(messages[0].textContent.includes("'constructor' : too many arguments"),
    "The first message contains the correct text.");
  ok(messages[1].textContent.includes("'=' : dimension mismatch"),
    "The second message contains the correct text.");
  ok(messages[2].textContent.includes("'assign' : cannot convert"),
    "The third message contains the correct text.");

  await teardown(panel);
  finish();
}
