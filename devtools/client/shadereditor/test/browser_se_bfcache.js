/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the shader editor works with bfcache.
 */
async function ifWebGLSupported() {
  const { target, panel } = await initShaderEditor(SIMPLE_CANVAS_URL);
  const { front, $, EVENTS, shadersListView, shadersEditorsView } = panel;

  // Attach frame scripts if in e10s to perform
  // history navigation via the content
  loadFrameScripts();

  const reloaded = reload(target);
  const firstProgram = await once(front, "program-linked");
  await reloaded;

  const navigated = navigate(target, MULTIPLE_CONTEXTS_URL);
  const [secondProgram, thirdProgram] = await getPrograms(front, 2);
  await navigated;

  const vsEditor = await shadersEditorsView._getEditor("vs");
  const fsEditor = await shadersEditorsView._getEditor("fs");

  await navigateInHistory(target, "back", "will-navigate");
  await once(panel, EVENTS.PROGRAMS_ADDED);
  await once(panel, EVENTS.SOURCES_SHOWN);

  is($("#content").hidden, false,
    "The tool's content should not be hidden.");
  is(shadersListView.itemCount, 1,
    "The shaders list contains one entry after navigating back.");
  is(shadersListView.selectedIndex, 0,
    "The shaders list has a correct selection after navigating back.");

  is(vsEditor.getText().indexOf("gl_Position"), 170,
    "The vertex shader editor contains the correct text.");
  is(fsEditor.getText().indexOf("gl_FragColor"), 97,
    "The fragment shader editor contains the correct text.");

  await navigateInHistory(target, "forward", "will-navigate");
  await once(panel, EVENTS.PROGRAMS_ADDED);
  await once(panel, EVENTS.SOURCES_SHOWN);

  is($("#content").hidden, false,
    "The tool's content should not be hidden.");
  is(shadersListView.itemCount, 2,
    "The shaders list contains two entries after navigating forward.");
  is(shadersListView.selectedIndex, 0,
    "The shaders list has a correct selection after navigating forward.");

  is(vsEditor.getText().indexOf("gl_Position"), 100,
    "The vertex shader editor contains the correct text.");
  is(fsEditor.getText().indexOf("gl_FragColor"), 89,
    "The fragment shader editor contains the correct text.");

  await teardown(panel);
  finish();
}
