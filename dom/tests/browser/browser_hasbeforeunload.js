"use strict";

const PAGE_URL =
  "http://example.com/browser/dom/tests/browser/beforeunload_test_page.html";

/**
 * Adds 1 or more inert beforeunload event listeners in this browser.
 * By default, will target the top-level content window, but callers
 * can specify the index of a subframe to target. See prepareSubframes
 * for an idea of how the subframes are structured.
 *
 * @param {<xul:browser>} browser
 *        The browser to add the beforeunload event listener in.
 * @param {int} howMany
 *        How many beforeunload event listeners to add. Note that these
 *        beforeunload event listeners are inert and will not actually
 *        prevent the host window from navigating.
 * @param {optional int} frameDepth
 *        The depth of the frame to add the event listener to. Defaults
 *        to 0, which is the top-level content window.
 * @return {Promise}
 */
function addBeforeUnloadListeners(browser, howMany = 1, frameDepth = 0) {
  return controlFrameAt(browser, frameDepth, {
    name: "AddBeforeUnload",
    howMany,
  });
}

/**
 * Adds 1 or more inert beforeunload event listeners in this browser on
 * a particular subframe. By default, this will target the first subframe
 * under the top-level content window, but callers can specify the index
 * of a subframe to target. See prepareSubframes for an idea of how the
 * subframes are structured.
 *
 * Note that this adds the beforeunload event listener on the "outer" window,
 * by doing:
 *
 * iframe.addEventListener("beforeunload", ...);
 *
 * @param {<xul:browser>} browser
 *        The browser to add the beforeunload event listener in.
 * @param {int} howMany
 *        How many beforeunload event listeners to add. Note that these
 *        beforeunload event listeners are inert and will not actually
 *        prevent the host window from navigating.
 * @param {optional int} frameDepth
 *        The depth of the frame to add the event listener to. Defaults
 *        to 1, which is the first subframe inside the top-level content
 *        window. Setting this to 0 will throw.
 * @return {Promise}
 */
function addOuterBeforeUnloadListeners(browser, howMany = 1, frameDepth = 1) {
  if (frameDepth == 0) {
    throw new Error("When adding a beforeunload listener on an outer " +
                    "window, the frame you're targeting needs to be at " +
                    "depth > 0.")
  }

  return controlFrameAt(browser, frameDepth, {
    name: "AddOuterBeforeUnload",
    howMany,
  });
}

/**
 * Removes 1 or more inert beforeunload event listeners in this browser.
 * This assumes that addBeforeUnloadListeners has been called previously
 * for the target frame.
 *
 * By default, will target the top-level content window, but callers
 * can specify the index of a subframe to target. See prepareSubframes
 * for an idea of how the subframes are structured.
 *
 * @param {<xul:browser>} browser
 *        The browser to remove the beforeunload event listener from.
 * @param {int} howMany
 *        How many beforeunload event listeners to remove.
 * @param {optional int} frameDepth
 *        The depth of the frame to remove the event listener from. Defaults
 *        to 0, which is the top-level content window.
 * @return {Promise}
 */
function removeBeforeUnloadListeners(browser, howMany = 1, frameDepth = 0) {
  return controlFrameAt(browser, frameDepth, {
    name: "RemoveBeforeUnload",
    howMany,
  });
}

/**
 * Removes 1 or more inert beforeunload event listeners in this browser on
 * a particular subframe. By default, this will target the first subframe
 * under the top-level content window, but callers can specify the index
 * of a subframe to target. See prepareSubframes for an idea of how the
 * subframes are structured.
 *
 * Note that this removes the beforeunload event listener on the "outer" window,
 * by doing:
 *
 * iframe.removeEventListener("beforeunload", ...);
 *
 * @param {<xul:browser>} browser
 *        The browser to remove the beforeunload event listener from.
 * @param {int} howMany
 *        How many beforeunload event listeners to remove.
 * @param {optional int} frameDepth
 *        The depth of the frame to remove the event listener from. Defaults
 *        to 1, which is the first subframe inside the top-level content
 *        window. Setting this to 0 will throw.
 * @return {Promise}
 */
function removeOuterBeforeUnloadListeners(browser, howMany = 1, frameDepth = 1) {
  if (frameDepth == 0) {
    throw new Error("When removing a beforeunload listener from an outer " +
                    "window, the frame you're targeting needs to be at " +
                    "depth > 0.")
  }

  return controlFrameAt(browser, frameDepth, {
    name: "RemoveOuterBeforeUnload",
    howMany,
  });
}

/**
 * Navigates a content window to a particular URL and waits for it to
 * finish loading that URL.
 *
 * By default, will target the top-level content window, but callers
 * can specify the index of a subframe to target. See prepareSubframes
 * for an idea of how the subframes are structured.
 *
 * @param {<xul:browser>} browser
 *        The browser that will have the navigation occur within it.
 * @param {string} url
 *        The URL to send the content window to.
 * @param {optional int} frameDepth
 *        The depth of the frame to navigate. Defaults to 0, which is
 *        the top-level content window.
 * @return {Promise}
 */
function navigateSubframe(browser, url, frameDepth = 0) {
  let navigatePromise = controlFrameAt(browser, frameDepth, {
    name: "Navigate",
    url,
  });
  let subframeLoad = BrowserTestUtils.browserLoaded(browser, true);
  return Promise.all([navigatePromise, subframeLoad]);
}

/**
 * Removes the <iframe> from a content window pointed at PAGE_URL.
 *
 * By default, will target the top-level content window, but callers
 * can specify the index of a subframe to target. See prepareSubframes
 * for an idea of how the subframes are structured.
 *
 * @param {<xul:browser>} browser
 *        The browser that will have removal occur within it.
 * @param {optional int} frameDepth
 *        The depth of the frame that will have the removal occur within
 *        it. Defaults to 0, which is the top-level content window, meaning
 *        that the first subframe will be removed.
 * @return {Promise}
 */
function removeSubframeFrom(browser, frameDepth = 0) {
  return controlFrameAt(browser, frameDepth, {
    name: "RemoveSubframe",
  });
}

/**
 * Sends a command to a frame pointed at PAGE_URL. There are utility
 * functions defined in this file that call this function. You should
 * use those instead.
 *
 * @param {<xul:browser>} browser
 *        The browser to send the command to.
 * @param {int} frameDepth
 *        The depth of the frame that we'll send the command to. 0 means
 *        sending it to the top-level content window.
 * @param {object} command
 *        An object with the following structure:
 *
 *        {
 *          name: (string),
 *          <arbitrary arguments to send with the command>
 *        }
 *
 *        Here are the commands that can be sent:
 *
 *        AddBeforeUnload
 *          {int} howMany
 *          How many beforeunload event listeners to add.
 *
 *        AddOuterBeforeUnload
 *          {int} howMany
 *          How many beforeunload event listeners to add to
 *          the iframe in the document at this depth.
 *
 *        RemoveBeforeUnload
 *          {int} howMany
 *          How many beforeunload event listeners to remove.
 *
 *        RemoveOuterBeforeUnload
 *          {int} howMany
 *          How many beforeunload event listeners to remove from
 *          the iframe in the document at this depth.
 *
 *        Navigate
 *          {string} url
 *          The URL to send the frame to.
 *
 *        RemoveSubframe
 *
 * @return {Promise}
 */
function controlFrameAt(browser, frameDepth, command) {
  return ContentTask.spawn(browser, { frameDepth, command }, async function(args) {
    Cu.import("resource://testing-common/TestUtils.jsm", this);

    let { command: contentCommand, frameDepth: contentFrameDepth } = args;

    let targetContent = content;
    let targetSubframe = content.document.getElementById("subframe");

    // We want to not only find the frame that maps to the
    // target frame depth that we've been given, but we also want
    // to count the total depth so that if a middle frame is removed
    // or navigated, then we know how many outer-window-destroyed
    // observer notifications to expect.
    let currentContent = targetContent;
    let currentSubframe = targetSubframe;

    let depth = 0;

    do {
      currentContent = currentSubframe.contentWindow;
      currentSubframe = currentContent.document.getElementById("subframe");
      depth++;
      if (depth == contentFrameDepth) {
        targetContent = currentContent;
        targetSubframe = currentSubframe;
      }
    } while (currentSubframe);

    switch (contentCommand.name) {
      case "AddBeforeUnload": {
        let BeforeUnloader = targetContent.wrappedJSObject.BeforeUnloader;
        Assert.ok(BeforeUnloader, "Found BeforeUnloader in the test page.");
        BeforeUnloader.pushInner(contentCommand.howMany);
        break;
      }
      case "AddOuterBeforeUnload": {
        let BeforeUnloader = targetContent.wrappedJSObject.BeforeUnloader;
        Assert.ok(BeforeUnloader, "Found BeforeUnloader in the test page.");
        BeforeUnloader.pushOuter(contentCommand.howMany);
        break;
      }
      case "RemoveBeforeUnload": {
        let BeforeUnloader = targetContent.wrappedJSObject.BeforeUnloader;
        Assert.ok(BeforeUnloader, "Found BeforeUnloader in the test page.");
        BeforeUnloader.popInner(contentCommand.howMany);
        break;
      }
      case "RemoveOuterBeforeUnload": {
        let BeforeUnloader = targetContent.wrappedJSObject.BeforeUnloader;
        Assert.ok(BeforeUnloader, "Found BeforeUnloader in the test page.");
        BeforeUnloader.popOuter(contentCommand.howMany);
        break;
      }
      case "Navigate": {
        // How many frames are going to be destroyed when we do this? We
        // need to wait for that many window destroyed notifications.
        targetContent.location = contentCommand.url;

        let destroyedOuterWindows = depth - contentFrameDepth;
        if (destroyedOuterWindows) {
          await TestUtils.topicObserved("outer-window-destroyed", () => {
            destroyedOuterWindows--;
            return !destroyedOuterWindows;
          });
        }
        break;
      }
      case "RemoveSubframe": {
        let subframe = targetContent.document.getElementById("subframe");
        Assert.ok(subframe, "Found subframe at frame depth of " + contentFrameDepth);
        subframe.remove();

        let destroyedOuterWindows = depth - contentFrameDepth;
        if (destroyedOuterWindows) {
          await TestUtils.topicObserved("outer-window-destroyed", () => {
            destroyedOuterWindows--;
            return !destroyedOuterWindows;
          });
        }
        break;
      }
    }
  });
}

/**
 * Sets up a structure where a page at PAGE_URL will host an
 * <iframe> also pointed at PAGE_URL, and does this repeatedly
 * until we've achieved the desired frame depth. Note that this
 * will cause the top-level browser to reload, and wipe out any
 * previous changes to the DOM under it.
 *
 * @param {<xul:browser>} browser
 *        The browser in which we'll load our structure at the
 *        top level.
 * @param {Array<object>} options
 *        Set-up options for each subframe. The following properties
 *        are accepted:
 *
 *        {string} sandboxAttributes
 *          The value to set the sandbox attribute to. If null, no sandbox
 *          attribute will be set (and any pre-existing sandbox attributes)
 *          on the <iframe> will be removed.
 *
 *        The number of entries on the options Array corresponds to how many
 *        subframes are under the top-level content window.
 *
 *        Example:
 *
 *        yield prepareSubframes(browser, [
 *          { sandboxAttributes: null },
 *          { sandboxAttributes: "allow-modals" },
 *        ]);
 *
 *        This would create the following structure:
 *
 *        <top-level content window at PAGE_URL>
 *        |
 *        |--> <iframe at PAGE_URL, no sandbox attributes>
 *             |
 *             |--> <iframe at PAGE_URL, sandbox="allow-modals">
 *
 * @return {Promise}
 */
async function prepareSubframes(browser, options) {
  browser.reload();
  await BrowserTestUtils.browserLoaded(browser);

  await ContentTask.spawn(browser, { options, PAGE_URL }, async function(args) {
    let { options: allSubframeOptions, PAGE_URL: contentPageURL } = args;
    function loadBeforeUnloadHelper(doc, subframeOptions) {
      let subframe = doc.getElementById("subframe");
      subframe.remove();
      if (subframeOptions.sandboxAttributes === null) {
        subframe.removeAttribute("sandbox");
      } else {
        subframe.setAttribute("sandbox", subframeOptions.sandboxAttributes);
      }
      doc.body.appendChild(subframe);
      subframe.contentWindow.location = contentPageURL;
      return ContentTaskUtils.waitForEvent(subframe, "load").then(() => {
        return subframe.contentDocument;
      });
    }

    let currentDoc = content.document;
    for (let subframeOptions of allSubframeOptions) {
      currentDoc = await loadBeforeUnloadHelper(currentDoc, subframeOptions);
    }
  });
}

/**
 * Ensures that a browser's nsITabParent hasBeforeUnload attribute
 * is set to the expected value.
 *
 * @param {<xul:browser>} browser
 *        The browser whose nsITabParent we will check.
 * @param {bool} expected
 *        True if hasBeforeUnload is expected to be true.
 */
function assertHasBeforeUnload(browser, expected) {
  Assert.equal(browser.frameLoader.tabParent.hasBeforeUnload,
               expected);
}

/**
 * Tests that the nsITabParent hasBeforeUnload attribute works under
 * a number of different scenarios on inner windows. At a high-level,
 * we test that hasBeforeUnload works properly during page / iframe
 * navigation, or when an <iframe> with a beforeunload listener on its
 * inner window is removed from the DOM.
 */
add_task(async function test_inner_window_scenarios() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE_URL,
  }, async function(browser) {
    Assert.ok(browser.isRemoteBrowser,
              "This test only makes sense with out of process browsers.");
    assertHasBeforeUnload(browser, false);

    // Test the simple case on the top-level window by adding a single
    // beforeunload event listener on the inner window and then removing
    // it.
    await addBeforeUnloadListeners(browser);
    assertHasBeforeUnload(browser, true);
    await removeBeforeUnloadListeners(browser);
    assertHasBeforeUnload(browser, false);

    // Now let's add several beforeunload listeners, and
    // ensure that we only set hasBeforeUnload to false once
    // the last listener is removed.
    await addBeforeUnloadListeners(browser, 3);
    assertHasBeforeUnload(browser, true);
    await removeBeforeUnloadListeners(browser); // 2 left...
    assertHasBeforeUnload(browser, true);
    await removeBeforeUnloadListeners(browser); // 1 left...
    assertHasBeforeUnload(browser, true);
    await removeBeforeUnloadListeners(browser); // None left!

    assertHasBeforeUnload(browser, false);

    // Now let's have the top-level content window navigate
    // away with a beforeunload listener set, and ensure
    // that we clear the hasBeforeUnload value.
    await addBeforeUnloadListeners(browser, 5);
    await navigateSubframe(browser, "http://example.com");
    assertHasBeforeUnload(browser, false);

    // Now send the page back to the test page for
    // the next few tests.
    browser.loadURI(PAGE_URL);
    await BrowserTestUtils.browserLoaded(browser);

    // We want to test hasBeforeUnload works properly with
    // beforeunload event listeners in <iframe> elements too.
    // We prepare a structure like this with 3 content windows
    // to exercise:
    //
    // <top-level content window at PAGE_URL> (TOP)
    // |
    // |--> <iframe at PAGE_URL> (MIDDLE)
    //      |
    //      |--> <iframe at PAGE_URL> (BOTTOM)
    //
    await prepareSubframes(browser, [
      { sandboxAttributes: null, },
      { sandboxAttributes: null, },
    ]);
    // These constants are just to make it easier to know which
    // frame we're referring to without having to remember the
    // exact indices.
    const TOP = 0;
    const MIDDLE = 1;
    const BOTTOM = 2;

    // We should initially start with hasBeforeUnload set to false.
    assertHasBeforeUnload(browser, false);

    // Tests that if there are beforeunload event listeners on
    // all levels of our window structure, that we only set
    // hasBeforeUnload to false once the last beforeunload
    // listener has been unset.
    await addBeforeUnloadListeners(browser, 2, MIDDLE);
    assertHasBeforeUnload(browser, true);
    await addBeforeUnloadListeners(browser, 1, TOP);
    assertHasBeforeUnload(browser, true);
    await addBeforeUnloadListeners(browser, 5, BOTTOM);
    assertHasBeforeUnload(browser, true);

    await removeBeforeUnloadListeners(browser, 1, TOP);
    assertHasBeforeUnload(browser, true);
    await removeBeforeUnloadListeners(browser, 5, BOTTOM);
    assertHasBeforeUnload(browser, true);
    await removeBeforeUnloadListeners(browser, 2, MIDDLE);
    assertHasBeforeUnload(browser, false);

    // Tests that if a beforeunload event listener is set on
    // an iframe that navigates away to a page without a
    // beforeunload listener, that hasBeforeUnload is set
    // to false.
    await addBeforeUnloadListeners(browser, 5, BOTTOM);
    assertHasBeforeUnload(browser, true);

    await navigateSubframe(browser, "http://example.com", BOTTOM);
    assertHasBeforeUnload(browser, false);

    // Reset our window structure now.
    await prepareSubframes(browser, [
      { sandboxAttributes: null, },
      { sandboxAttributes: null, },
    ]);

    // This time, add beforeunload event listeners to both the
    // MIDDLE and BOTTOM frame, and then navigate the MIDDLE
    // away. This should set hasBeforeUnload to false.
    await addBeforeUnloadListeners(browser, 3, MIDDLE);
    await addBeforeUnloadListeners(browser, 1, BOTTOM);
    assertHasBeforeUnload(browser, true);
    await navigateSubframe(browser, "http://example.com", MIDDLE);
    assertHasBeforeUnload(browser, false);

    // Tests that if the MIDDLE and BOTTOM frames have beforeunload
    // event listeners, and if we remove the BOTTOM <iframe> and the
    // MIDDLE <iframe>, that hasBeforeUnload is set to false.
    await prepareSubframes(browser, [
      { sandboxAttributes: null, },
      { sandboxAttributes: null, },
    ]);
    await addBeforeUnloadListeners(browser, 3, MIDDLE);
    await addBeforeUnloadListeners(browser, 1, BOTTOM);
    assertHasBeforeUnload(browser, true);
    await removeSubframeFrom(browser, MIDDLE);
    assertHasBeforeUnload(browser, true);
    await removeSubframeFrom(browser, TOP);
    assertHasBeforeUnload(browser, false);

    // Tests that if the MIDDLE and BOTTOM frames have beforeunload
    // event listeners, and if we remove just the MIDDLE <iframe>, that
    // hasBeforeUnload is set to false.
    await prepareSubframes(browser, [
      { sandboxAttributes: null, },
      { sandboxAttributes: null, },
    ]);
    await addBeforeUnloadListeners(browser, 3, MIDDLE);
    await addBeforeUnloadListeners(browser, 1, BOTTOM);
    assertHasBeforeUnload(browser, true);
    await removeSubframeFrom(browser, TOP);
    assertHasBeforeUnload(browser, false);

    // Test that two sandboxed iframes, _without_ the allow-modals
    // permission, do not result in the hasBeforeUnload attribute
    // being set to true when beforeunload event listeners are added.
    await prepareSubframes(browser, [
      { sandboxAttributes: "allow-scripts", },
      { sandboxAttributes: "allow-scripts", },
    ]);

    await addBeforeUnloadListeners(browser, 3, MIDDLE);
    await addBeforeUnloadListeners(browser, 1, BOTTOM);
    assertHasBeforeUnload(browser, false);

    await removeBeforeUnloadListeners(browser, 3, MIDDLE);
    await removeBeforeUnloadListeners(browser, 1, BOTTOM);
    assertHasBeforeUnload(browser, false);

    // Test that two sandboxed iframes, both with the allow-modals
    // permission, cause the hasBeforeUnload attribute to be set
    // to true when beforeunload event listeners are added.
    await prepareSubframes(browser, [
      { sandboxAttributes: "allow-scripts allow-modals", },
      { sandboxAttributes: "allow-scripts allow-modals", },
    ]);

    await addBeforeUnloadListeners(browser, 3, MIDDLE);
    await addBeforeUnloadListeners(browser, 1, BOTTOM);
    assertHasBeforeUnload(browser, true);

    await removeBeforeUnloadListeners(browser, 1, BOTTOM);
    assertHasBeforeUnload(browser, true);
    await removeBeforeUnloadListeners(browser, 3, MIDDLE);
    assertHasBeforeUnload(browser, false);
  });
});

/**
 * Tests that the nsITabParent hasBeforeUnload attribute works under
 * a number of different scenarios on outer windows. Very similar to
 * the above set of tests, except that we add the beforeunload listeners
 * to the iframe DOM nodes instead of the inner windows.
 */
add_task(async function test_outer_window_scenarios() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE_URL,
  }, async function(browser) {
    Assert.ok(browser.isRemoteBrowser,
              "This test only makes sense with out of process browsers.");
    assertHasBeforeUnload(browser, false);

    // We want to test hasBeforeUnload works properly with
    // beforeunload event listeners in <iframe> elements.
    // We prepare a structure like this with 3 content windows
    // to exercise:
    //
    // <top-level content window at PAGE_URL> (TOP)
    // |
    // |--> <iframe at PAGE_URL> (MIDDLE)
    //      |
    //      |--> <iframe at PAGE_URL> (BOTTOM)
    //
    await prepareSubframes(browser, [
      { sandboxAttributes: null, },
      { sandboxAttributes: null, },
    ]);

    // These constants are just to make it easier to know which
    // frame we're referring to without having to remember the
    // exact indices.
    const TOP = 0;
    const MIDDLE = 1;
    const BOTTOM = 2;

    // Test the simple case on the top-level window by adding a single
    // beforeunload event listener on the outer window of the iframe
    // in the TOP document.
    await addOuterBeforeUnloadListeners(browser);
    assertHasBeforeUnload(browser, true);

    await removeOuterBeforeUnloadListeners(browser);
    assertHasBeforeUnload(browser, false);

    // Now let's add several beforeunload listeners, and
    // ensure that we only set hasBeforeUnload to false once
    // the last listener is removed.
    await addOuterBeforeUnloadListeners(browser, 3);
    assertHasBeforeUnload(browser, true);
    await removeOuterBeforeUnloadListeners(browser); // 2 left...
    assertHasBeforeUnload(browser, true);
    await removeOuterBeforeUnloadListeners(browser); // 1 left...
    assertHasBeforeUnload(browser, true);
    await removeOuterBeforeUnloadListeners(browser); // None left!

    assertHasBeforeUnload(browser, false);

    // Now let's have the top-level content window navigate away
    // with a beforeunload listener set on the outer window of the
    // iframe inside it, and ensure that we clear the hasBeforeUnload
    // value.
    await addOuterBeforeUnloadListeners(browser, 5);
    await navigateSubframe(browser, "http://example.com", TOP);
    assertHasBeforeUnload(browser, false);

    // Now send the page back to the test page for
    // the next few tests.
    browser.loadURI(PAGE_URL);
    await BrowserTestUtils.browserLoaded(browser);

    // We should initially start with hasBeforeUnload set to false.
    assertHasBeforeUnload(browser, false);

    await prepareSubframes(browser, [
      { sandboxAttributes: null, },
      { sandboxAttributes: null, },
    ]);

    // Tests that if there are beforeunload event listeners on
    // all levels of our window structure, that we only set
    // hasBeforeUnload to false once the last beforeunload
    // listener has been unset.
    await addOuterBeforeUnloadListeners(browser, 3, MIDDLE);
    assertHasBeforeUnload(browser, true);
    await addOuterBeforeUnloadListeners(browser, 7, BOTTOM);
    assertHasBeforeUnload(browser, true);

    await removeOuterBeforeUnloadListeners(browser, 7, BOTTOM);
    assertHasBeforeUnload(browser, true);
    await removeOuterBeforeUnloadListeners(browser, 3, MIDDLE);
    assertHasBeforeUnload(browser, false);

    // Tests that if a beforeunload event listener is set on
    // an iframe that navigates away to a page without a
    // beforeunload listener, that hasBeforeUnload is set
    // to false. We're setting the event listener on the
    // outer window on the <iframe> in the MIDDLE, which
    // itself contains the BOTTOM frame it our structure.
    await addOuterBeforeUnloadListeners(browser, 5, BOTTOM);
    assertHasBeforeUnload(browser, true);

    // Now navigate that BOTTOM frame.
    await navigateSubframe(browser, "http://example.com", BOTTOM);
    assertHasBeforeUnload(browser, false);

    // Reset our window structure now.
    await prepareSubframes(browser, [
      { sandboxAttributes: null, },
      { sandboxAttributes: null, },
    ]);

    // This time, add beforeunload event listeners to the outer
    // windows for MIDDLE and BOTTOM. Then navigate the MIDDLE
    // frame. This should set hasBeforeUnload to false.
    await addOuterBeforeUnloadListeners(browser, 3, MIDDLE);
    await addOuterBeforeUnloadListeners(browser, 1, BOTTOM);
    assertHasBeforeUnload(browser, true);
    await navigateSubframe(browser, "http://example.com", MIDDLE);
    assertHasBeforeUnload(browser, false);

    // Adds beforeunload event listeners to the outer windows of
    // MIDDLE and BOTOTM, and then removes those iframes. Removing
    // both iframes should set hasBeforeUnload to false.
    await prepareSubframes(browser, [
      { sandboxAttributes: null, },
      { sandboxAttributes: null, },
    ]);
    await addOuterBeforeUnloadListeners(browser, 3, MIDDLE);
    await addOuterBeforeUnloadListeners(browser, 1, BOTTOM);
    assertHasBeforeUnload(browser, true);
    await removeSubframeFrom(browser, BOTTOM);
    assertHasBeforeUnload(browser, true);
    await removeSubframeFrom(browser, MIDDLE);
    assertHasBeforeUnload(browser, false);

    // Adds beforeunload event listeners to the outer windows of MIDDLE
    // and BOTTOM, and then removes just the MIDDLE iframe (which will
    // take the bottom one with it). This should set hasBeforeUnload to
    // false.
    await prepareSubframes(browser, [
      { sandboxAttributes: null, },
      { sandboxAttributes: null, },
    ]);
    await addOuterBeforeUnloadListeners(browser, 3, MIDDLE);
    await addOuterBeforeUnloadListeners(browser, 1, BOTTOM);
    assertHasBeforeUnload(browser, true);
    await removeSubframeFrom(browser, TOP);
    assertHasBeforeUnload(browser, false);

    // Test that two sandboxed iframes, _without_ the allow-modals
    // permission, do not result in the hasBeforeUnload attribute
    // being set to true when beforeunload event listeners are added
    // to the outer windows. Note that this requires the
    // allow-same-origin permission, otherwise a cross-origin
    // security exception is thrown.
    await prepareSubframes(browser, [
      { sandboxAttributes: "allow-same-origin allow-scripts", },
      { sandboxAttributes: "allow-same-origin allow-scripts", },
    ]);

    await addOuterBeforeUnloadListeners(browser, 3, MIDDLE);
    await addOuterBeforeUnloadListeners(browser, 1, BOTTOM);
    assertHasBeforeUnload(browser, false);

    await removeOuterBeforeUnloadListeners(browser, 3, MIDDLE);
    await removeOuterBeforeUnloadListeners(browser, 1, BOTTOM);
    assertHasBeforeUnload(browser, false);

    // Test that two sandboxed iframes, both with the allow-modals
    // permission, cause the hasBeforeUnload attribute to be set
    // to true when beforeunload event listeners are added. Note
    // that this requires the allow-same-origin permission,
    // otherwise a cross-origin security exception is thrown.
    await prepareSubframes(browser, [
      { sandboxAttributes: "allow-same-origin allow-scripts allow-modals", },
      { sandboxAttributes: "allow-same-origin allow-scripts allow-modals", },
    ]);

    await addOuterBeforeUnloadListeners(browser, 3, MIDDLE);
    await addOuterBeforeUnloadListeners(browser, 1, BOTTOM);
    assertHasBeforeUnload(browser, true);

    await removeOuterBeforeUnloadListeners(browser, 1, BOTTOM);
    assertHasBeforeUnload(browser, true);
    await removeOuterBeforeUnloadListeners(browser, 3, MIDDLE);
    assertHasBeforeUnload(browser, false);
  });
});

/**
 * Tests hasBeforeUnload behaviour when beforeunload event listeners
 * are added on both inner and outer windows.
 */
add_task(async function test_mixed_inner_and_outer_window_scenarios() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE_URL,
  }, async function(browser) {
    Assert.ok(browser.isRemoteBrowser,
              "This test only makes sense with out of process browsers.");
    assertHasBeforeUnload(browser, false);

    // We want to test hasBeforeUnload works properly with
    // beforeunload event listeners in <iframe> elements.
    // We prepare a structure like this with 3 content windows
    // to exercise:
    //
    // <top-level content window at PAGE_URL> (TOP)
    // |
    // |--> <iframe at PAGE_URL> (MIDDLE)
    //      |
    //      |--> <iframe at PAGE_URL> (BOTTOM)
    //
    await prepareSubframes(browser, [
      { sandboxAttributes: null, },
      { sandboxAttributes: null, },
    ]);

    // These constants are just to make it easier to know which
    // frame we're referring to without having to remember the
    // exact indices.
    const TOP = 0;
    const MIDDLE = 1;
    const BOTTOM = 2;

    await addBeforeUnloadListeners(browser, 1, TOP);
    assertHasBeforeUnload(browser, true);
    await addBeforeUnloadListeners(browser, 2, MIDDLE);
    assertHasBeforeUnload(browser, true);
    await addBeforeUnloadListeners(browser, 5, BOTTOM);
    assertHasBeforeUnload(browser, true);

    await addOuterBeforeUnloadListeners(browser, 3, MIDDLE);
    assertHasBeforeUnload(browser, true);
    await addOuterBeforeUnloadListeners(browser, 7, BOTTOM);
    assertHasBeforeUnload(browser, true);

    await removeBeforeUnloadListeners(browser, 5, BOTTOM);
    assertHasBeforeUnload(browser, true);

    await removeBeforeUnloadListeners(browser, 2, MIDDLE);
    assertHasBeforeUnload(browser, true);

    await removeOuterBeforeUnloadListeners(browser, 3, MIDDLE);
    assertHasBeforeUnload(browser, true);

    await removeBeforeUnloadListeners(browser, 1, TOP);
    assertHasBeforeUnload(browser, true);

    await removeOuterBeforeUnloadListeners(browser, 7, BOTTOM);
    assertHasBeforeUnload(browser, false);
  });
});
