/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
function waitForActive() {
    if (!gBrowser.docShell.isActive) {
        executeSoon(waitForActive);
        return;
    }
    is(gBrowser.docShell.isActive, true, "Docshell should be active again");
    finish();
}

function waitForInactive() {
    if (gBrowser.docShell.isActive) {
        executeSoon(waitForInactive);
        return;
    }
    is(gBrowser.docShell.isActive, false, "Docshell should be inactive");
    window.restore();
    waitForActive();
}

function test() {
    registerCleanupFunction(function() {
      window.restore();
    });

    waitForExplicitFinish();
    is(gBrowser.docShell.isActive, true, "Docshell should be active");
    window.minimize();
    // XXX On Linux minimize/restore seem to be very very async, but
    // our window.windowState changes sync.... so we can't rely on the
    // latter correctly reflecting the state of the former.  In
    // particular, a restore() call before minimizing is done will not
    // actually restore the window, but change the window state.  As a
    // result, just poll waiting for our expected isActive values.
    waitForInactive();
}
