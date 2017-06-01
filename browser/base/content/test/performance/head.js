/**
 * Async utility function for ensuring that no unexpected uninterruptible
 * reflows occur during some period of time in a window.
 *
 * The helper works by running a JS function before each event is
 * dispatched that attempts to dirty the layout tree - the idea being
 * that this puts us in the "worst case scenario" so that any JS
 * that attempts to query for layout or style information will cause
 * a reflow to fire. We also dirty the layout tree after each reflow
 * occurs, for good measure.
 *
 * This sounds good in theory, but it's trickier in practice due to
 * various optimizations in our Layout engine. The default function
 * for dirtying the layout tree adds a margin to the first element
 * child it finds in the window to a maximum of 3px, and then goes
 * back to 0px again and loops.
 *
 * This is not sufficient for reflows that we expect to happen within
 * scrollable frames, as Gecko is able to side-step reflowing the
 * contents of a scrollable frame if outer frames are dirtied. Because
 * of this, it's currently possible to override the default node to
 * dirty with one more appropriate for the test.
 *
 * It is also theoretically possible for enough events to fire between
 * reflows such that the before and after state of the layout tree is
 * exactly the same, meaning that no reflow is required, which opens
 * us up to missing expected reflows. This seems to be possible in
 * theory, but hasn't yet shown up in practice - it's just something
 * to be aware of.
 *
 * Bug 1363361 has been filed for a more reliable way of dirtying layout.
 *
 * @param testFn (async function)
 *        The async function that will exercise the browser activity that is
 *        being tested for reflows.
 * @param expectedStacks (Array, optional)
 *        An Array of Arrays representing stacks.
 *
 *        Example:
 *
 *        [
 *          // This reflow is caused by lorem ipsum
 *          [
 *            "select@chrome://global/content/bindings/textbox.xml",
 *            "focusAndSelectUrlBar@chrome://browser/content/browser.js",
 *            "openLinkIn@chrome://browser/content/utilityOverlay.js",
 *            "openUILinkIn@chrome://browser/content/utilityOverlay.js",
 *            "BrowserOpenTab@chrome://browser/content/browser.js",
 *          ],
 *
 *          // This reflow is caused by lorem ipsum
 *          [
 *            "get_scrollPosition@chrome://global/content/bindings/scrollbox.xml",
 *            "_fillTrailingGap@chrome://browser/content/tabbrowser.xml",
 *            "_handleNewTab@chrome://browser/content/tabbrowser.xml",
 *            "onxbltransitionend@chrome://browser/content/tabbrowser.xml",
 *          ],
 *
 *        ]
 *
 *        Note that line numbers are not included in the stacks.
 *
 *        Order of the reflows doesn't matter. Expected reflows that aren't seen
 *        will cause an assertion failure. When this argument is not passed,
 *        it defaults to the empty Array, meaning no reflows are expected.
 * @param window (browser window, optional)
 *        The browser window to monitor. Defaults to the current window.
 * @param elemToDirty (DOM node, optional)
 *        Callers can provide a custom DOM node to change some layout style
 *        on in the event that the action being tested is occurring within
 *        a scrollable frame. Otherwise, withReflowObserver defaults to dirtying
 *        the first element child of the window.
 */
async function withReflowObserver(testFn, expectedStacks = [], win = window, elemToDirty) {
  if (!elemToDirty) {
    elemToDirty = win.document.firstElementChild;
  }

  let i = 0;
  let dirtyFrameFn = (e) => {
    elemToDirty.style.margin = (++i % 4) + "px";
  };

  let els = Cc["@mozilla.org/eventlistenerservice;1"]
              .getService(Ci.nsIEventListenerService);

  // We're going to remove the stacks one by one as we see them so that
  // we can check for expected, unseen reflows, so let's clone the array.
  expectedStacks = expectedStacks.slice(0);

  let observer = {
    reflow(start, end) {
      // Gather information about the current code path, slicing out the current
      // frame.
      let path = (new Error().stack).split("\n").slice(1).map(line => {
        return line.replace(/:\d+:\d+$/, "");
      }).join("|");

      let pathWithLineNumbers = (new Error().stack).split("\n").slice(1);

      // Just in case, dirty the frame now that we've reflowed.
      dirtyFrameFn();

      // Stack trace is empty. Reflow was triggered by native code, which
      // we ignore.
      if (path === "") {
        return;
      }

      let index = expectedStacks.findIndex(stack => path.startsWith(stack.join("|")));

      if (index != -1) {
        Assert.ok(true, "expected uninterruptible reflow: '" +
                  JSON.stringify(pathWithLineNumbers, null, "\t") + "'");
        expectedStacks.splice(index, 1);
      } else {
        Assert.ok(false, "unexpected uninterruptible reflow \n" +
                         JSON.stringify(pathWithLineNumbers, null, "\t") + "\n");
      }
    },

    reflowInterruptible(start, end) {
      // We're not interested in interruptible reflows.
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIReflowObserver,
                                           Ci.nsISupportsWeakReference])
  };

  let docShell = win.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIWebNavigation)
                    .QueryInterface(Ci.nsIDocShell);
  docShell.addWeakReflowObserver(observer);

  els.addListenerForAllEvents(win, dirtyFrameFn, true);

  try {
    dirtyFrameFn();
    await testFn();
  } finally {
    for (let remainder of expectedStacks) {
      Assert.ok(false,
                `Unused expected reflow: ${JSON.stringify(remainder, null, "\t")}.\n` +
                "This is probably a good thing - just remove it from the " +
                "expected list.");
    }


    els.removeListenerForAllEvents(win, dirtyFrameFn, true);
    docShell.removeWeakReflowObserver(observer);

    elemToDirty.style.margin = "";
  }
}

