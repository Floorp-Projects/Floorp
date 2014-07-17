/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* Bug 968896 */

// Test the completions using numbers.
const source = "0x1.";
const completions = ["toExponential", "toFixed", "toString"];
Cu.import("resource://gre/modules/Task.jsm");

function test() {
  const options = { tabContent: "test scratchpad autocomplete" };
  openTabAndScratchpad(options)
    .then(Task.async(runTests))
    .then(finish, console.error);
}


function* runTests([win, sp]) {
  const {editor} = sp;
  const editorWin = editor.container.contentWindow;

  // Show the completions popup.
  sp.setText(source);
  sp.editor.setCursor({ line: 0, ch: source.length });
  yield keyOnce("suggestion-entered", " ", { ctrlKey: true });

  // Get the hints popup container.
  const hints = editorWin.document.querySelector(".CodeMirror-hints");

  ok(hints,
     "The hint container should exist.")
  is(hints.childNodes.length, 3,
     "The hint container should have the completions.");

  let i = 0;
  for (let completion of completions) {
    let active = hints.querySelector(".CodeMirror-hint-active");
    is(active.textContent, completion,
       "Check that completion " + i++ + " is what is expected.");
    yield keyOnce("suggestion-entered", "VK_DOWN");
  }

  // We should have looped around to the first suggestion again. Accept it.
  yield keyOnce("after-suggest", "VK_RETURN");

  is(sp.getText(), source + completions[0],
     "Autocompletion should work and select the right element.");

  // Check that the information tooltips work.
  sp.setText("5");
  yield keyOnce("show-information", " ", { ctrlKey: true, shiftKey: true });

  // Get the information container.
  const info = editorWin.document.querySelector(".CodeMirror-Tern-information");
  ok(info,
     "Info tooltip should appear.");
  is(info.textContent.slice(0, 6), "number",
     "Info tooltip should have expected contents.");

  function keyOnce(event, key, opts = {}) {
    const p = editor.once(event);
    EventUtils.synthesizeKey(key, opts, editorWin);
    return p;
  }
}
