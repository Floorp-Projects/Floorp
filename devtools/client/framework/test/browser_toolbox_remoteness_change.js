/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const URL_1 = "about:robots";
const URL_2 =
  "data:text/html;charset=UTF-8," +
  encodeURIComponent('<div id="remote-page">foo</div>');

// Testing navigation between processes
add_task(async function() {
  info(`Testing navigation between processes`);

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

  const toolbox = await openToolboxForTab(tab);

  info("Navigate to a URL supporting remote process");
  await navigateTo(URL_2);

  is(
    tab.linkedBrowser.getAttribute("remote"),
    "true",
    "Navigated to a data: URI and switching to remote"
  );

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
  ok(client._transportClosed, "The client is closed after closing the toolbox");
});
