/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const URL_1 = "about:robots";
const URL_2 =
  "data:text/html;charset=UTF-8," +
  encodeURIComponent('<div id="remote-page">foo</div>');

add_task(async function() {
  // Test twice.
  // Once without target switching, where the toolbox closes and reopens
  // And a second time, with target switching, where the toolbox stays open
  await navigateBetweenProcesses(false);
  await navigateBetweenProcesses(true);
});

async function navigateBetweenProcesses(enableTargetSwitching) {
  info(
    `Testing navigation between processes ${
      enableTargetSwitching ? "with" : "without"
    } target switching`
  );
  await pushPref("devtools.target-switching.enabled", enableTargetSwitching);

  info("Open a tab on a URL supporting only running in parent process");
  const tab = await addTab(URL_1);
  is(
    tab.linkedBrowser.currentURI.spec,
    URL_1,
    "We really are on the expected document"
  );
  is(
    tab.linkedBrowser.getAttribute("remote"),
    "",
    "And running in parent process"
  );

  let toolbox = await openToolboxForTab(tab);

  const onToolboxDestroyed = toolbox.once("destroyed");
  const onToolboxCreated = gDevTools.once("toolbox-created");
  const onToolboxSwitchedToTarget = toolbox.targetList.once("switched-target");

  info("Navigate to a URL supporting remote process");
  if (enableTargetSwitching) {
    await navigateTo(URL_2);
  } else {
    // `navigateTo` except the toolbox to be kept open.
    // So, fallback to BrowserTestUtils helpers in this test when
    // the target-switching preference is turned off.
    const onBrowserLoaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    BrowserTestUtils.loadURI(tab.linkedBrowser, URL_2);
    await onBrowserLoaded;
  }

  is(
    tab.linkedBrowser.getAttribute("remote"),
    "true",
    "Navigated to a data: URI and switching to remote"
  );

  if (enableTargetSwitching) {
    info("Waiting for the toolbox to be switched to the new target");
    await onToolboxSwitchedToTarget;
  } else {
    info("Waiting for the toolbox to be destroyed");
    await onToolboxDestroyed;

    info("Waiting for a new toolbox to be created");
    toolbox = await onToolboxCreated;

    info("Waiting for the new toolbox to be ready");
    await toolbox.once("ready");
  }

  info("Veryify we are inspecting the new document");
  const console = await toolbox.selectTool("webconsole");
  const { ui } = console.hud;
  ui.wrapper.dispatchEvaluateExpression("document.location.href");
  await waitUntil(() => ui.outputNode.querySelector(".result"));
  const url = ui.outputNode.querySelector(".result");

  ok(
    url.textContent.includes(URL_2),
    "The console inspects the second document"
  );

  const { client } = toolbox.target;
  await toolbox.destroy();
  ok(client._closed, "The client is closed after closing the toolbox");
}
