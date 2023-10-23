/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that the tooltip previews are correct with wrapped editor lines.

"use strict";

const httpServer = createTestHTTPServer();
httpServer.registerContentType("html", "text/html");
httpServer.registerContentType("js", "application/javascript");

httpServer.registerPathHandler(
  "/doc-wrapped-lines.html",
  (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(`<!DOCTYPE html>
    <html>
      <head>
        <script type="text/javascript">
        const cs1 = getComputedStyle(document.documentElement);
        const cs2 = getComputedStyle(document.documentElement);
        // This line generates a very long inline-preview which is loaded a bit later after the
        // initial positions for the page has been calculated
        function add(a,b,k){var result=a+b;return k(result)}function sub(a,b,k){var result=a-b;return k(result)}function mul(a,b,k){var result=a*b;return k(result)}function div(a,b,k){var result=a/b;return k(result)}function arithmetic(){
  add(4,4,function(a){
    sub(a,2,function(b){mul(b,3,function(c){div(c,2,function(d){console.log("arithmetic",d)})})})})};
        isNaN(cs1, cs2);
          const foo = { prop: 0 };
          const bar = Math.min(foo);
          const myVeryLongVariableNameThatMayWrap = 42;
          myVeryLongVariableNameThatMayWrap * 2;
          debugger;
        </script>
      </head>
    </html>
  `);
  }
);

const BASE_URL = `http://localhost:${httpServer.identity.primaryPort}/`;

add_task(async function () {
  await pushPref("devtools.toolbox.footer.height", 500);
  await pushPref("devtools.debugger.ui.editor-wrapping", true);

  // Reset toolbox height and end panel size to avoid impacting other tests
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("debugger.end-panel-size");
  });

  const dbg = await initDebuggerWithAbsoluteURL(
    `${BASE_URL}doc-wrapped-lines.html`
  );
  await waitForSources(dbg, "doc-wrapped-lines.html");

  const onReloaded = reload(dbg);
  await waitForPaused(dbg);

  await assertPreviews(dbg, [
    {
      line: 13,
      column: 18,
      expression: "foo",
      fields: [["prop", "0"]],
    },
    {
      line: 14,
      column: 18,
      result: "NaN",
      expression: "bar",
    },
  ]);

  info("Resize the editor until `myVeryLongVariableNameThatMayWrap` wraps");
  // Use splitter to resize to make sure CodeMirror internal state is refreshed
  // in such case (CodeMirror already handle window resize on its own).
  const splitter = dbg.win.document.querySelectorAll(".splitter")[1];
  const splitterOriginalX = splitter.getBoundingClientRect().left;
  ok(splitter, "Got the splitter");

  let longToken = getTokenElAtLine(
    dbg,
    "myVeryLongVariableNameThatMayWrap",
    16
  );
  const longTokenBoundingClientRect = longToken.getBoundingClientRect();
  await resizeSplitter(
    dbg,
    splitter,
    longTokenBoundingClientRect.left + longTokenBoundingClientRect.width / 2
  );

  info("Wait until the token does wrap");
  longToken = await waitFor(() => {
    const token = getTokenElAtLine(
      dbg,
      "myVeryLongVariableNameThatMayWrap",
      16
    );
    if (token.getBoxQuads().length === 1) {
      return null;
    }
    return token;
  });

  longToken.scrollIntoView();

  await assertPreviews(dbg, [
    {
      line: 16,
      column: 13,
      expression: "myVeryLongVariableNameThatMayWrap",
      result: "42",
    },
  ]);

  // clearing the pref isn't enough to have consistent sizes between runs,
  // so set it back to its original position
  await resizeSplitter(dbg, splitter, splitterOriginalX);

  await resume(dbg);
  await onReloaded;

  Services.prefs.clearUserPref("debugger.end-panel-size");
  await wait(1000);
});

async function resizeSplitter(dbg, splitterEl, x) {
  EventUtils.synthesizeMouse(splitterEl, 0, 0, { type: "mousedown" }, dbg.win);

  // Resizing the editor should cause codeMirror to refresh
  const cm = dbg.getCM();
  const onEditorRefreshed = new Promise(resolve =>
    cm.on("refresh", function onCmRefresh() {
      cm.off("refresh", onCmRefresh);
      resolve();
    })
  );
  // Move the splitter of the secondary pane to the middle of the token,
  // this should cause the token to wrap.
  EventUtils.synthesizeMouseAtPoint(
    x,
    splitterEl.getBoundingClientRect().top + 10,
    { type: "mousemove" },
    dbg.win
  );

  // Stop dragging
  EventUtils.synthesizeMouseAtCenter(splitterEl, { type: "mouseup" }, dbg.win);

  await onEditorRefreshed;
  ok(true, "CodeMirror was refreshed when resizing the editor");
}
