/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

add_task(async function test() {
  const XUL_NS =
    "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

  await SpecialPowers.pushPrefEnv({
    set: [["dom.require_user_interaction_for_beforeunload", false]],
  });

  let url = TEST_PATH + "file_bug1415918_beforeunload.html";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  let browser = tab.linkedBrowser;
  let stack = browser.parentNode;
  let buttonId;
  let promptShown = false;

  let observer = new MutationObserver(function(mutations) {
    mutations.forEach(function(mutation) {
      if (
        buttonId &&
        mutation.type == "attributes" &&
        browser.hasAttribute("tabmodalPromptShowing")
      ) {
        let prompt = stack.getElementsByTagNameNS(XUL_NS, "tabmodalprompt")[0];
        prompt.querySelector(`.tabmodalprompt-${buttonId}`).click();
        promptShown = true;
      }
    });
  });
  observer.observe(browser, { attributes: true });

  /*
   * Check condition where beforeunload handlers request a prompt.
   */

  // Prompt is shown, user clicks OK.
  buttonId = "button0";
  promptShown = false;
  ok(browser.permitUnload().permitUnload, "permit unload should be true");
  ok(promptShown, "prompt should have been displayed");

  // Check that all beforeunload handlers fired and reset attributes.
  await SpecialPowers.spawn(browser, [], () => {
    ok(
      content.window.document.body.hasAttribute("fired"),
      "parent document beforeunload handler should fire"
    );
    content.window.document.body.removeAttribute("fired");

    for (let frame of Array.from(content.window.frames)) {
      ok(
        frame.document.body.hasAttribute("fired"),
        "frame document beforeunload handler should fire"
      );
      frame.document.body.removeAttribute("fired");
    }
  });

  // Prompt is shown, user clicks CANCEL.
  buttonId = "button1";
  promptShown = false;
  ok(!browser.permitUnload().permitUnload, "permit unload should be false");
  ok(promptShown, "prompt should have been displayed");
  buttonId = "";

  // Check that only the parent beforeunload handler fired, and reset attribute.
  await SpecialPowers.spawn(browser, [], () => {
    ok(
      content.window.document.body.hasAttribute("fired"),
      "parent document beforeunload handler should fire"
    );
    content.window.document.body.removeAttribute("fired");

    for (let frame of Array.from(content.window.frames)) {
      ok(
        !frame.document.body.hasAttribute("fired"),
        "frame document beforeunload handler should not fire"
      );
    }
  });

  // Prompt is not shown, don't permit unload.
  promptShown = false;
  ok(
    !browser.permitUnload(browser.dontPromptAndDontUnload).permitUnload,
    "permit unload should be false"
  );
  ok(!promptShown, "prompt should not have been displayed");

  // Check that only the parent beforeunload handler fired, and reset attribute.
  await SpecialPowers.spawn(browser, [], () => {
    ok(
      content.window.document.body.hasAttribute("fired"),
      "parent document beforeunload handler should fire"
    );
    content.window.document.body.removeAttribute("fired");

    for (let frame of Array.from(content.window.frames)) {
      ok(
        !frame.document.body.hasAttribute("fired"),
        "frame document beforeunload handler should not fire"
      );
    }
  });

  // Prompt is not shown, permit unload.
  promptShown = false;
  ok(
    browser.permitUnload(browser.dontPromptAndUnload).permitUnload,
    "permit unload should be true"
  );
  ok(!promptShown, "prompt should not have been displayed");

  // Check that all beforeunload handlers fired.
  await SpecialPowers.spawn(browser, [], () => {
    ok(
      content.window.document.body.hasAttribute("fired"),
      "parent document beforeunload handler should fire"
    );

    for (let frame of Array.from(content.window.frames)) {
      ok(
        frame.document.body.hasAttribute("fired"),
        "frame document beforeunload handler should fire"
      );
    }
  });

  /*
   * Check condition where no one requests a prompt.  In all cases,
   * permitUnload should be true, and all handlers fired.
   */

  buttonId = "button0";
  url = TEST_PATH + "file_bug1415918_beforeunload_2.html";
  BrowserTestUtils.loadURI(browser, url);
  await BrowserTestUtils.browserLoaded(browser, false, url);
  buttonId = "";

  promptShown = false;
  ok(browser.permitUnload().permitUnload, "permit unload should be true");
  ok(!promptShown, "prompt should not have been displayed");

  // Check that all beforeunload handlers fired and reset attributes.
  await SpecialPowers.spawn(browser, [], () => {
    ok(
      content.window.document.body.hasAttribute("fired"),
      "parent document beforeunload handler should fire"
    );
    content.window.document.body.removeAttribute("fired");

    for (let frame of Array.from(content.window.frames)) {
      ok(
        frame.document.body.hasAttribute("fired"),
        "frame document beforeunload handler should fire"
      );
      frame.document.body.removeAttribute("fired");
    }
  });

  promptShown = false;
  ok(
    browser.permitUnload(browser.dontPromptAndDontUnload).permitUnload,
    "permit unload should be true"
  );
  ok(!promptShown, "prompt should not have been displayed");

  // Check that all beforeunload handlers fired and reset attributes.
  await SpecialPowers.spawn(browser, [], () => {
    ok(
      content.window.document.body.hasAttribute("fired"),
      "parent document beforeunload handler should fire"
    );
    content.window.document.body.removeAttribute("fired");

    for (let frame of Array.from(content.window.frames)) {
      ok(
        frame.document.body.hasAttribute("fired"),
        "frame document beforeunload handler should fire"
      );
      frame.document.body.removeAttribute("fired");
    }
  });

  promptShown = false;
  ok(
    browser.permitUnload(browser.dontPromptAndUnload).permitUnload,
    "permit unload should be true"
  );
  ok(!promptShown, "prompt should not have been displayed");

  // Check that all beforeunload handlers fired.
  await SpecialPowers.spawn(browser, [], () => {
    ok(
      content.window.document.body.hasAttribute("fired"),
      "parent document beforeunload handler should fire"
    );

    for (let frame of Array.from(content.window.frames)) {
      ok(
        frame.document.body.hasAttribute("fired"),
        "frame document beforeunload handler should fire"
      );
    }
  });

  /*
   * Check condition where the parent beforeunload handler does not request a prompt,
   * but a child beforeunload handler does.
   */

  buttonId = "button0";
  url = TEST_PATH + "file_bug1415918_beforeunload_3.html";
  BrowserTestUtils.loadURI(browser, url);
  await BrowserTestUtils.browserLoaded(browser, false, url);

  // Prompt is shown, user clicks OK.
  promptShown = false;
  ok(browser.permitUnload().permitUnload, "permit unload should be true");
  ok(promptShown, "prompt should have been displayed");

  // Check that all beforeunload handlers fired and reset attributes.
  await SpecialPowers.spawn(browser, [], () => {
    ok(
      content.window.document.body.hasAttribute("fired"),
      "parent document beforeunload handler should fire"
    );
    content.window.document.body.removeAttribute("fired");

    for (let frame of Array.from(content.window.frames)) {
      ok(
        frame.document.body.hasAttribute("fired"),
        "frame document beforeunload handler should fire"
      );
      frame.document.body.removeAttribute("fired");
    }
  });

  // Prompt is shown, user clicks CANCEL.
  buttonId = "button1";
  promptShown = false;
  ok(!browser.permitUnload().permitUnload, "permit unload should be false");
  ok(promptShown, "prompt should have been displayed");
  buttonId = "";

  // Check that the parent beforeunload handler fired, and only one child beforeunload
  // handler fired.  Reset attributes.
  await SpecialPowers.spawn(browser, [], () => {
    ok(
      content.window.document.body.hasAttribute("fired"),
      "parent document beforeunload handler should fire"
    );
    content.window.document.body.removeAttribute("fired");

    let count = 0;
    for (let frame of Array.from(content.window.frames)) {
      if (frame.document.body.hasAttribute("fired")) {
        count++;
        frame.document.body.removeAttribute("fired");
      }
    }
    is(count, 1, "only one frame document beforeunload handler should fire");
  });

  // Prompt is not shown, don't permit unload.
  promptShown = false;
  ok(
    !browser.permitUnload(browser.dontPromptAndDontUnload).permitUnload,
    "permit unload should be false"
  );
  ok(!promptShown, "prompt should not have been displayed");

  // Check that the parent beforeunload handler fired, and only one child beforeunload
  // handler fired.  Reset attributes.
  await SpecialPowers.spawn(browser, [], () => {
    ok(
      content.window.document.body.hasAttribute("fired"),
      "parent document beforeunload handler should fire"
    );
    content.window.document.body.removeAttribute("fired");

    let count = 0;
    for (let frame of Array.from(content.window.frames)) {
      if (frame.document.body.hasAttribute("fired")) {
        count++;
        frame.document.body.removeAttribute("fired");
      }
    }
    is(count, 1, "only one frame document beforeunload handler should fire");
  });

  // Prompt is not shown, permit unload.
  promptShown = false;
  ok(
    browser.permitUnload(browser.dontPromptAndUnload).permitUnload,
    "permit unload should be true"
  );
  ok(!promptShown, "prompt should not have been displayed");

  // Check that all beforeunload handlers fired.
  await SpecialPowers.spawn(browser, [], () => {
    ok(
      content.window.document.body.hasAttribute("fired"),
      "parent document beforeunload handler should fire"
    );

    for (let frame of Array.from(content.window.frames)) {
      ok(
        frame.document.body.hasAttribute("fired"),
        "frame document beforeunload handler should fire"
      );
    }
  });

  // Remove tab.
  buttonId = "button0";
  BrowserTestUtils.removeTab(tab);
});
