/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TAB_URL = "data:text/html,<title>foo</title>";

add_task(async function() {
  const { tab, document } = await openAboutDebugging("tabs");

  // Wait for initial tabs list which may be empty
  let tabsElement = getTabList(document);
  await waitUntilElement(".target-name", tabsElement);

  // Refresh tabsElement to get the .target-list element
  tabsElement = getTabList(document);

  let names = [...tabsElement.querySelectorAll(".target-name")];
  const initialTabCount = names.length;

  info("Open a new background tab");
  const newTab = await addTab(TAB_URL, { background: true });

  info("Wait for the tab to appear in the list with the correct name");
  const container = await waitUntilTabContainer("foo", document);

  info("Wait until the title to update");
  await waitUntil(() => {
    return container.querySelector(".target-name").title === TAB_URL;
  }, 100);

  const icon = container.querySelector(".target-icon");
  ok(icon && icon.src, "Tab icon found and src attribute is not empty");

  info("Check if the tab icon is a valid image");
  await new Promise(r => {
    const image = new Image();
    image.onload = () => {
      ok(true, "Favicon is not a broken image");
      r();
    };
    image.onerror = () => {
      ok(false, "Favicon is a broken image");
      r();
    };
    image.src = icon.src;
  });

  // Finally, close the tab
  await removeTab(newTab);

  info("Wait until the tab container is removed");
  await waitUntil(() => !getTabContainer("foo", document), 100);

  // Check that the tab disappeared from the UI
  names = [...tabsElement.querySelectorAll("#tabs .target-name")];
  is(names.length, initialTabCount, "The tab disappeared from the UI");

  await closeAboutDebugging(tab);
});

function getTabContainer(name, document) {
  const nameElements = [...document.querySelectorAll("#tabs .target-name")];
  const nameElement = nameElements.filter(element => element.textContent === name)[0];
  if (nameElement) {
    return nameElement.closest(".target-container");
  }

  return null;
}

async function waitUntilTabContainer(name, document) {
  await waitUntil(() => {
    return getTabContainer(name, document);
  });
  return getTabContainer(name, document);
}
