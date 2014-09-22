/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that program and shader actors are cached in the frontend.
 */

function ifWebGLSupported() {
  let { target, debuggee, panel } = yield initShaderEditor(MULTIPLE_CONTEXTS_URL);
  let { EVENTS, gFront, ShadersListView, ShadersEditorsView } = panel.panelWin;

  reload(target);
  let [[programActor]] = yield promise.all([
    getPrograms(gFront, 1),
    once(panel.panelWin, EVENTS.SOURCES_SHOWN)
  ]);

  let programItem = ShadersListView.selectedItem;

  is(programItem.attachment.programActor, programActor,
    "The correct program actor is cached for the selected item.");

  is((yield programActor.getVertexShader()),
     (yield programItem.attachment.vs),
    "The cached vertex shader promise returns the correct actor.");

  is((yield programActor.getFragmentShader()),
     (yield programItem.attachment.fs),
    "The cached fragment shader promise returns the correct actor.");

  is((yield (yield programActor.getVertexShader()).getText()),
     (yield (yield ShadersEditorsView._getEditor("vs")).getText()),
    "The cached vertex shader promise returns the correct text.");

  is((yield (yield programActor.getFragmentShader()).getText()),
     (yield (yield ShadersEditorsView._getEditor("fs")).getText()),
    "The cached fragment shader promise returns the correct text.");

  yield teardown(panel);
  finish();
}
