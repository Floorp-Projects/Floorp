"use strict";

function waitForClickEvent(aTarget) {
  return new Promise(resolve => {
    aTarget.addEventListener(
      "click",
      e => {
        resolve({ screenX: e.screenX, screenY: e.screenY });
      },
      { once: true }
    );
  });
}

function waitForRemoteClickEvent(aRemote) {
  return SpecialPowers.spawn(aRemote, [], () => {
    return new Promise(resolve => {
      content.document.addEventListener(
        "click",
        e => {
          resolve({ screenX: e.screenX, screenY: e.screenY });
        },
        { once: true }
      );
    });
  });
}

function executeSoonRemote(aRemote) {
  return SpecialPowers.spawn(aRemote, [], () => {
    return new Promise(resolve => {
      SpecialPowers.executeSoon(resolve);
    });
  });
}

add_task(async function testClickScreenXY() {
  await BrowserTestUtils.withNewTab(
    "https://example.com/browser/browser/base/content/test/general/dummy_page.html",
    async browser => {
      let parentPromise = waitForClickEvent(document);
      let contentPromise = waitForRemoteClickEvent(browser);
      // Ensure the event listener has registered on the remote.
      await executeSoonRemote(browser);

      EventUtils.synthesizeMouseAtCenter(browser, {});

      let parent = await parentPromise;
      let content = await contentPromise;

      is(parent.screenX, content.screenX, "check screenX");
      is(parent.screenY, content.screenY, "check screenY");
    }
  );
});
