/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Check that when the debugger pauses in a frame which is in a different target, the
// context selector is updated, and evaluating in the console is done in the paused
// frame context.

const TEST_URI = `${URL_ROOT_COM_SSL}test-console-evaluation-context-selector.html`;
const IFRAME_FILE = `test-console-evaluation-context-selector-child.html`;

add_task(async function () {
  await pushPref("devtools.webconsole.input.context", true);

  const tab = await addTab(TEST_URI);

  info("Create new iframes and add them to the page.");
  await addIFrameAndWaitForLoad(
    `${URL_ROOT_ORG_SSL}${IFRAME_FILE}?id=iframe_org`
  );
  await addIFrameAndWaitForLoad(
    `${URL_ROOT_NET_SSL}${IFRAME_FILE}?id=iframe_net`
  );

  const toolbox = await openToolboxForTab(tab, "webconsole");

  info("Open Debugger");
  await openDebugger();
  const dbg = createDebuggerContext(toolbox);

  info("Hit the debugger statement on first iframe");
  clickOnIframeStopMeButton(".iframe-1");

  info("Wait for the debugger to pause");
  await waitForPaused(dbg);

  info("Open the split Console");
  await toolbox.openSplitConsole();
  const { hud } = toolbox.getPanel("webconsole");

  const evaluationContextSelectorButton = hud.ui.outputNode.querySelector(
    ".webconsole-evaluation-selector-button"
  );

  if (!isFissionEnabled() && !isEveryFrameTargetEnabled()) {
    is(
      evaluationContextSelectorButton,
      null,
      "context selector is only displayed when Fission or EFT is enabled"
    );
    return;
  }

  await waitFor(
    () => evaluationContextSelectorButton.innerText.includes("example.org"),
    "The context selector wasn't updated"
  );
  ok(true, "The context was set to the first iframe document");

  // localVar is defined in the event listener, and was assigned the `document` value.
  setInputValue(hud, "localVar");
  await waitForEagerEvaluationResult(hud, /example\.org/);
  ok(true, "Instant evaluation has the expected result");

  await keyboardExecuteAndWaitForResultMessage(hud, `localVar`, "example.org");
  ok(true, "Evaluation result is the expected one");

  // Cleanup
  await clearOutput(hud);
  setInputValue(hud, "");

  info("Resume the debugger");
  await resume(dbg);

  info("Hit the debugger statement on second iframe");
  clickOnIframeStopMeButton(".iframe-2");

  info("Wait for the debugger to pause");
  await waitForPaused(dbg);

  await waitFor(
    () => evaluationContextSelectorButton.innerText.includes("example.net"),
    "The context selector wasn't updated"
  );
  ok(true, "The context was set to the second iframe document");

  // localVar is defined in the event listener, and was assigned the `document` value.
  setInputValue(hud, "localVar");
  await waitForEagerEvaluationResult(hud, /example\.net/);
  ok(true, "Instant evaluation has the expected result");

  await keyboardExecuteAndWaitForResultMessage(hud, `localVar`, "example.net");
  ok(true, "Evaluation result is the expected one");

  info("Resume the debugger");
  await resume(dbg);
});

async function addIFrameAndWaitForLoad(url) {
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [url], async innerUrl => {
    const iframe = content.document.createElement("iframe");
    const iframeCount = content.document.querySelectorAll("iframe").length;
    iframe.classList.add(`iframe-${iframeCount + 1}`);
    content.document.body.append(iframe);

    const onLoadIframe = new Promise(resolve => {
      iframe.addEventListener("load", resolve, { once: true });
    });

    iframe.src = innerUrl;
    await onLoadIframe;
  });
}

function clickOnIframeStopMeButton(iframeClassName) {
  SpecialPowers.spawn(gBrowser.selectedBrowser, [iframeClassName], cls => {
    const iframe = content.document.querySelector(cls);
    SpecialPowers.spawn(iframe, [], () => {
      content.document.querySelector(".stop-me").click();
    });
  });
}
