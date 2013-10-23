/* vim:set ts=2 sw=2 sts=2 et tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

(function () {
  "use strict";

  const WRAP_CLASS = "CodeMirror-activeline";
  const BACK_CLASS = "CodeMirror-activeline-background";

  CodeMirror.defineOption("styleActiveLine", false, function(cm, val, old) {
    var prev = old && old != CodeMirror.Init;

    if (val && !prev) {
      updateActiveLine(cm);
      cm.on("cursorActivity", updateActiveLine);
    } else if (!val && prev) {
      cm.off("cursorActivity", updateActiveLine);
      clearActiveLine(cm);
      delete cm.state.activeLine;
    }
  });

  function clearActiveLine(cm) {
    if ("activeLine" in cm.state) {
      cm.removeLineClass(cm.state.activeLine, "wrap", WRAP_CLASS);
      cm.removeLineClass(cm.state.activeLine, "background", BACK_CLASS);
    }
  }

  function updateActiveLine(cm) {
    var line = cm.getLineHandleVisualStart(cm.getCursor().line);
    if (cm.state.activeLine == line) return;
    clearActiveLine(cm);
    cm.addLineClass(line, "wrap", WRAP_CLASS);
    cm.addLineClass(line, "background", BACK_CLASS);
    cm.state.activeLine = line;
  }
})();