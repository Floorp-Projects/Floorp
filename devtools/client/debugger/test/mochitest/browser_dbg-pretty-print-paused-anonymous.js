/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that pretty-printing a file with no URL works while paused.

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-minified.html");

  info("Evaluate an expression with scriptCommand.execute");
  const debuggerDone = dbg.commands.scriptCommand.execute(
    `debugger; var foo; document.addEventListener("click", e => { debugger; }, {once: true})`
  );
  await waitForPaused(dbg);
  const evaluatedSourceId = dbg.selectors.getSelectedSourceId();

  // This will throw if things fail to pretty-print and render properly.
  info("Pretty print the source created by the evaluated expression");
  await prettyPrint(dbg);

  const prettyEvaluatedSourceFilename =
    evaluatedSourceId.split("/").at(-1) + ":formatted";
  await waitForSource(dbg, prettyEvaluatedSourceFilename);
  const prettySource = findSource(dbg, prettyEvaluatedSourceFilename);

  info("Check that the script was pretty-printed as expected");
  const { value: prettySourceValue } = findSourceContent(dbg, prettySource);

  is(
    prettySourceValue.trim(),
    `debugger;
var foo;
document.addEventListener('click', e => {
  debugger;
}, {
  once: true
})
`.trim(),
    "script was pretty printed as expected"
  );

  await resume(dbg);
  await debuggerDone;

  info("Check if we can pause inside the pretty-printed source");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.document.body.click();
  });
  await waitForPaused(dbg);
  await assertPausedAtSourceAndLine(dbg, prettySource.id, 4);
  await resume(dbg);

  info("Check that pretty printing works in `eval`'d source");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.eval(
      `setTimeout(() => {debugger;document.addEventListener("click", e => { debugger; }, {once: true})}, 100)`
    );
  });
  await waitForPaused(dbg);
  const evalSourceId = dbg.selectors.getSelectedSourceId();

  // This will throw if things fail to pretty-print and render properly.
  info("Pretty print the source created by the `eval` expression");
  await prettyPrint(dbg);

  const prettyEvalSourceFilename =
    evalSourceId.split("/").at(-1) + ":formatted";
  await waitForSource(dbg, prettyEvalSourceFilename);
  const prettyEvalSource = findSource(dbg, prettyEvalSourceFilename);

  info("Check that the script was pretty-printed as expected");
  const { value: prettyEvalSourceValue } = findSourceContent(
    dbg,
    prettyEvalSource
  );

  is(
    prettyEvalSourceValue.trim(),
    `
setTimeout(
  () => {
    debugger;
    document.addEventListener('click', e => {
      debugger;
    }, {
      once: true
    })
  },
  100
)`.trim(),
    "script was pretty printed as expected"
  );
  await resume(dbg);

  info("Check if we can pause inside the pretty-printed eval source");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.document.body.click();
  });
  await waitForPaused(dbg);
  await assertPausedAtSourceAndLine(dbg, prettyEvalSource.id, 5);
  await resume(dbg);

  info("Check that pretty printing works in `new Function` source");
  invokeInTab("breakInNewFunction");
  await waitForPaused(dbg);
  const newFunctionSourceId = dbg.selectors.getSelectedSourceId();

  // This will throw if things fail to pretty-print and render properly.
  info("Pretty print the source created with `new Function`");
  await prettyPrint(dbg);

  const prettyNewFunctionSourceFilename =
    newFunctionSourceId.split("/").at(-1) + ":formatted";
  await waitForSource(dbg, prettyNewFunctionSourceFilename);
  const prettyNewFunctionSource = findSource(
    dbg,
    prettyNewFunctionSourceFilename
  );

  info("Check that the script was pretty-printed as expected");
  const { value: prettyNewFunctionSourceValue } = findSourceContent(
    dbg,
    prettyNewFunctionSource
  );

  is(
    prettyNewFunctionSourceValue.trim(),
    `function anonymous() {
  debugger;
  document.addEventListener('click', function () {
    debugger;
  })
}
`.trim(),
    "script was pretty printed as expected"
  );
  await resume(dbg);

  info("Check if we can pause inside the pretty-printed eval source");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.document.body.click();
  });
  await waitForPaused(dbg);
  await assertPausedAtSourceAndLine(dbg, prettyNewFunctionSource.id, 4);
  await resume(dbg);
});
