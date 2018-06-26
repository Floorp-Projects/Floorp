/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the programs list contains an entry after vertex and fragment
 * shaders are linked.
 */

async function ifWebGLSupported() {
  const { target, panel } = await initShaderEditor(MULTIPLE_CONTEXTS_URL);
  const { front, EVENTS, L10N, shadersListView, shadersEditorsView } = panel;

  is(shadersListView.itemCount, 0,
    "The shaders list should initially be empty.");
  is(shadersListView.selectedItem, null,
    "The shaders list has no selected item.");
  is(shadersListView.selectedIndex, -1,
    "The shaders list has a negative index.");

  reload(target);

  const [firstProgramActor, secondProgramActor] = await promise.all([
    getPrograms(front, 2, (actors) => {
      // Fired upon each actor addition, we want to check only
      // after the first actor has been added so we can test state
      if (actors.length === 1) {
        checkFirstProgram();
      }
      if (actors.length === 2) {
        checkSecondProgram();
      }
    }),
    once(panel, EVENTS.SOURCES_SHOWN)
  ]).then(([programs, ]) => programs);

  is(shadersListView.attachments[0].label, L10N.getFormatStr("shadersList.programLabel", 0),
    "The correct first label is shown in the shaders list.");
  is(shadersListView.attachments[1].label, L10N.getFormatStr("shadersList.programLabel", 1),
    "The correct second label is shown in the shaders list.");

  const vertexShader = await firstProgramActor.getVertexShader();
  const fragmentShader = await firstProgramActor.getFragmentShader();
  const vertSource = await vertexShader.getText();
  const fragSource = await fragmentShader.getText();

  const vsEditor = await shadersEditorsView._getEditor("vs");
  const fsEditor = await shadersEditorsView._getEditor("fs");

  is(vertSource, vsEditor.getText(),
    "The vertex shader editor contains the correct text.");
  is(fragSource, fsEditor.getText(),
    "The vertex shader editor contains the correct text.");

  const compiled = once(panel, EVENTS.SHADER_COMPILED).then(() => {
    ok(false, "Selecting a different program shouldn't recompile its shaders.");
  });

  const shown = once(panel, EVENTS.SOURCES_SHOWN).then(() => {
    ok(true, "The vertex and fragment sources have changed in the editors.");
  });

  EventUtils.sendMouseEvent({ type: "mousedown" }, shadersListView.items[1].target);
  await shown;

  is(shadersListView.selectedItem, shadersListView.items[1],
    "The shaders list has a correct item selected.");
  is(shadersListView.selectedIndex, 1,
    "The shaders list has a correct index selected.");

  await teardown(panel);
  finish();

  function checkFirstProgram() {
    is(shadersListView.itemCount, 1,
      "The shaders list contains one entry.");
    is(shadersListView.selectedItem, shadersListView.items[0],
      "The shaders list has a correct item selected.");
    is(shadersListView.selectedIndex, 0,
      "The shaders list has a correct index selected.");
  }
  function checkSecondProgram() {
    is(shadersListView.itemCount, 2,
      "The shaders list contains two entries.");
    is(shadersListView.selectedItem, shadersListView.items[0],
      "The shaders list has a correct item selected.");
    is(shadersListView.selectedIndex, 0,
      "The shaders list has a correct index selected.");
  }
}
