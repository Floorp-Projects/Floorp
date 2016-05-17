/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that Toolbox#viewSourceInStyleEditor works when style editor is not
 * yet opened.
 */

var URL = `${URL_ROOT}doc_viewsource.html`;
var CSS_URL = `${URL_ROOT}doc_theme.css`;

function* viewSource() {
  let toolbox = yield openNewTabAndToolbox(URL);

  let fileFound = yield toolbox.viewSourceInStyleEditor(CSS_URL, 2);
  ok(fileFound, "viewSourceInStyleEditor should resolve to true if source found.");

  let stylePanel = toolbox.getPanel("styleeditor");
  ok(stylePanel, "The style editor panel was opened.");
  is(toolbox.currentToolId, "styleeditor", "The style editor panel was selected.");

  let { UI } = stylePanel;

  is(UI.selectedEditor.styleSheet.href, CSS_URL,
    "The correct source is shown in the style editor.");
  is(UI.selectedEditor.sourceEditor.getCursor().line + 1, 2,
    "The correct line is highlighted in the style editor's source editor.");

  yield closeToolboxAndTab(toolbox);
  finish();
}

function test() {
  Task.spawn(viewSource).then(finish, (aError) => {
    ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
    finish();
  });
}
