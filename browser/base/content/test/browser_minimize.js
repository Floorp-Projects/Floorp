/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
function test() {
    is(gBrowser.docShell.isActive, true, "Docshell should be active");
    window.minimize();
    is(gBrowser.docShell.isActive, false, "Docshell should be inactive");
    window.restore();
    is(gBrowser.docShell.isActive, true, "Docshell should be active again");
}
