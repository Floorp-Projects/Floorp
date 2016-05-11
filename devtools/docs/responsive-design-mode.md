# Responsive Design Mode Architecture

## Context

You have a single browser tab that has visited several pages, and now has a
history that looks like, in oldest to newest order:

1. https://newsblur.com
2. https://mozilla.org (← current page)
3. https://convolv.es

## Opening RDM During Current Firefox Session

When opening RDM, the browser tab's history must preserved.  Additionally, we
strive to preserve the exact state of the currently displayed page (effectively
any in-page state, which is important for single page apps where data can be
lost if they are reloaded).

This seems a bit convoluted, but one advantage of this technique is that it
preserves tab state since the same tab is reused.  This helps to maintain any
extra state that may be set on tab by add-ons or others.

1. Create a temporary, hidden tab to load the tool UI.
2. Mark the tool tab browser's docshell as active so the viewport frame is
   created eagerly and will be ready to swap.
3. Create the initial viewport inside the tool UI.
4. Swap tab content from the regular browser tab to the browser within the
   viewport in the tool UI, preserving all state via
   `gBrowser._swapBrowserDocShells`.
5. Force the original browser tab to be non-remote since the tool UI must be
   loaded in the parent process, and we're about to swap the tool UI into
   this tab.
6. Swap the tool UI (with viewport showing the content) into the original
   browser tab and close the temporary tab used to load the tool via
   `swapBrowsersAndCloseOther`.

## Closing RDM During Current Firefox Session

To close RDM, we follow a similar process to the one from opening RDM so we can
restore the content back to a normal tab.

1. Create a temporary, hidden tab to hold the content.
2. Mark the content tab browser's docshell as active so the frame is created
   eagerly and will be ready to swap.
3. Swap tab content from the browser within the viewport in the tool UI to the
   regular browser tab, preserving all state via
   `gBrowser._swapBrowserDocShells`.
4. Force the original browser tab to be remote since web content is loaded in
   the child process, and we're about to swap the content into this tab.
5. Swap the content into the original browser tab and close the temporary tab
   used to hold the content via `swapBrowsersAndCloseOther`.

## Session Restore

When restarting Firefox and restoring a user's browsing session, we must
correctly restore the tab history.  If the RDM tool was opened when the session
was captured, then it would be acceptable to either:

* A: Restore the tab content without any RDM tool displayed **OR**
* B: Restore the RDM tool the tab content inside, just as before the restart

We currently follow path A (no RDM after session restore), which seems more in
line with how the rest of DevTools currently functions after restore.  To do so,
we watch for `beforeunload` events on the tab at shutdown and quickly exit RDM
so that session restore records only the original page content during its final
write at shutdown.
