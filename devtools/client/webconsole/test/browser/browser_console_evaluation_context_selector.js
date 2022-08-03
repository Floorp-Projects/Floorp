/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function() {
  await pushPref("devtools.webconsole.input.context", true);
  await pushPref("devtools.chrome.enabled", true);
  await pushPref("devtools.every-frame-target.enabled", true);
  await pushPref("devtools.browsertoolbox.fission", true);
  await pushPref("devtools.browsertoolbox.scope", "everything");

  const hud = await BrowserConsoleManager.toggleBrowserConsole();

  const evaluationContextSelectorButton = await waitFor(() =>
    hud.ui.outputNode.querySelector(".webconsole-evaluation-selector-button")
  );

  ok(
    evaluationContextSelectorButton,
    "The evaluation context selector is visible"
  );
  is(
    evaluationContextSelectorButton.innerText,
    "Top",
    "The button has the expected 'Top' text"
  );
  is(
    evaluationContextSelectorButton.classList.contains("checked"),
    false,
    "The checked class isn't applied"
  );

  await executeAndWaitForResultMessage(
    hud,
    "document.location",
    "chrome://browser/content/browser.xhtml"
  );
  ok(true, "The evaluation was done in the top context");

  setInputValue(hud, "document.location.host");
  await waitForEagerEvaluationResult(hud, `"browser"`);

  info("Check the context selector menu");
  checkContextSelectorMenuItemAt(hud, 0, {
    label: "Top",
    tooltip: "chrome://browser/content/browser.xhtml",
    checked: true,
  });
  checkContextSelectorMenuItemAt(hud, 1, {
    separator: true,
  });

  info(
    "Add a tab with a worker and check both the document and the worker are displayed in the context selector"
  );
  const documentFile = "test-evaluate-worker.html";
  const documentWithWorkerUrl =
    "https://example.com/browser/devtools/client/webconsole/test/browser/" +
    documentFile;
  const tab = await addTab(documentWithWorkerUrl);

  const documentIndex = await waitFor(() => {
    const i = getContextSelectorItems(hud).findIndex(el =>
      el.querySelector(".label")?.innerText?.endsWith(documentFile)
    );
    return i == -1 ? null : i;
  });

  info("Select the document target");
  selectTargetInContextSelector(hud, documentWithWorkerUrl);

  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes(documentFile)
  );
  ok(true, "The context was set to the selected document");
  is(
    evaluationContextSelectorButton.classList.contains("checked"),
    true,
    "The checked class is applied"
  );

  checkContextSelectorMenuItemAt(hud, documentIndex, {
    label: documentWithWorkerUrl,
    tooltip: documentWithWorkerUrl,
    checked: true,
  });

  await waitForEagerEvaluationResult(hud, `"example.com"`);
  ok(true, "The instant evaluation result is updated in the document context");

  await executeAndWaitForResultMessage(
    hud,
    "document.location",
    documentWithWorkerUrl
  );
  ok(true, "The evaluation is done in the document context");

  // set input text so we can watch for instant evaluation result update
  setInputValue(hud, "globalThis.location.href");
  await waitForEagerEvaluationResult(hud, `"${documentWithWorkerUrl}"`);

  info("Select the worker target");
  const workerFile = "test-evaluate-worker.js";
  const workerUrl =
    "https://example.com/browser/devtools/client/webconsole/test/browser/" +
    workerFile;
  // Wait for the worker target to be displayed
  await waitFor(() =>
    getContextSelectorItems(hud).find(el =>
      el.querySelector(".label")?.innerText?.endsWith(workerFile)
    )
  );
  selectTargetInContextSelector(hud, workerFile);

  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes(workerFile)
  );
  ok(true, "The context was set to the selected worker");

  await waitForEagerEvaluationResult(hud, `"${workerUrl}"`);
  ok(true, "The instant evaluation result is updated in the worker context");

  const workerIndex = await waitFor(() => {
    const i = getContextSelectorItems(hud).findIndex(el =>
      el.querySelector(".label")?.innerText?.endsWith(workerFile)
    );
    return i == -1 ? null : i;
  });
  checkContextSelectorMenuItemAt(hud, workerIndex, {
    label: workerFile,
    tooltip: workerFile,
    checked: true,
  });

  await executeAndWaitForResultMessage(
    hud,
    "globalThis.location",
    `WorkerLocation`
  );
  ok(true, "The evaluation is done in the worker context");

  // set input text so we can watch for instant evaluation result update
  setInputValue(hud, "document.location.host");
  await waitForEagerEvaluationResult(
    hud,
    `ReferenceError: document is not defined`
  );

  info(
    "Remove the tab and make sure both the document and worker target are removed from the context selector"
  );
  await removeTab(tab);

  await waitFor(() => evaluationContextSelectorButton.innerText == "Top");
  ok(true, "The context is set back to Top");

  checkContextSelectorMenuItemAt(hud, 0, {
    label: "Top",
    tooltip: "chrome://browser/content/browser.xhtml",
    checked: true,
  });

  is(
    getContextSelectorItems(hud).every(el => {
      const label = el.querySelector(".label")?.innerText;
      return (
        !label ||
        (label !== "test-evaluate-worker.html" && label !== workerFile)
      );
    }),
    true,
    "the document and worker targets were removed"
  );

  await waitForEagerEvaluationResult(hud, `"browser"`);
  ok(true, "The instant evaluation was done in the top context");

  await executeAndWaitForResultMessage(
    hud,
    "document.location",
    "chrome://browser/content/browser.xhtml"
  );
  ok(true, "The evaluation was done in the top context");
});
