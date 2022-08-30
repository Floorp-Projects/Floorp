/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

requestLongerTimeout(5);

var { Toolbox } = require("devtools/client/framework/toolbox");

const NAME_1 = "";
const NAME_2 = "Toolbox test for title update";
const NAME_3 = NAME_2;
const NAME_4 = "Toolbox test for another title update";

const URL_1 = "data:text/plain;charset=UTF-8,abcde";
const URL_2 =
  URL_ROOT_ORG_SSL + "browser_toolbox_window_title_changes_page.html";
const URL_3 =
  URL_ROOT_COM_SSL + "browser_toolbox_window_title_changes_page.html";
const URL_4 = `https://example.com/document-builder.sjs?html=<head><title>${NAME_4}</title></head><h1>Hello`;

add_task(async function test() {
  await addTab(URL_1);

  const tab = gBrowser.selectedTab;
  let toolbox = await gDevTools.showToolboxForTab(tab, {
    hostType: Toolbox.HostType.BOTTOM,
  });
  await toolbox.selectTool("webconsole");

  info("Undock toolbox and check title");
  // We have to first switch the host in order to spawn the new top level window
  // on which we are going to listen from title change event
  await toolbox.switchHost(Toolbox.HostType.WINDOW);
  await checkTitle(NAME_1, URL_1, "toolbox undocked");

  info("switch to different tool and check title again");
  await toolbox.selectTool("jsdebugger");
  await checkTitle(NAME_1, URL_1, "tool changed");

  info("navigate to different local url and check title");

  await navigateTo(URL_2);
  info("wait for title change");
  await checkTitle(NAME_2, URL_2, "url changed");

  info("navigate to a real url and check title");
  await navigateTo(URL_3);

  info("wait for title change");
  await checkTitle(NAME_3, URL_3, "url changed");

  info("navigate to another page on the same domain");
  await navigateTo(URL_4);
  await checkTitle(NAME_4, URL_4, "title changed");

  info(
    "destroy toolbox, create new one hosted in a window (with a different tool id), and check title"
  );
  // Give the tools a chance to handle the navigation event before
  // destroying the toolbox.
  await new Promise(resolve => executeSoon(resolve));
  await toolbox.destroy();

  // After destroying the toolbox, open a new one.
  toolbox = await gDevTools.showToolboxForTab(tab, {
    hostType: Toolbox.HostType.WINDOW,
  });
  toolbox.selectTool("webconsole");
  await checkTitle(NAME_4, URL_4, "toolbox destroyed and recreated");

  info("clean up");
  await toolbox.destroy();
  gBrowser.removeCurrentTab();
  Services.prefs.clearUserPref("devtools.toolbox.host");
  Services.prefs.clearUserPref("devtools.toolbox.selectedTool");
});

function getExpectedTitle(name, url) {
  if (name) {
    return `Developer Tools — ${name} — ${url}`;
  }
  return `Developer Tools — ${url}`;
}

async function checkTitle(name, url, context) {
  info("Check title - " + context);
  await waitFor(
    () => getToolboxWindowTitle() === getExpectedTitle(name, url),
    `Didn't get the expected title ("${getExpectedTitle(name, url)}"`,
    200,
    50
  );
  const expectedTitle = getExpectedTitle(name, url);
  is(getToolboxWindowTitle(), expectedTitle, context);
}

function getToolboxWindowTitle() {
  return Services.wm.getMostRecentWindow("devtools:toolbox").document.title;
}
