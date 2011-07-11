/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
function test() {
    is(gBrowser.docShellIsActive, true, "Docshell should be active");
    window.minimize();
    is(gBrowser.docShellIsActive, false, "Docshell should be inactive");
    window.restore();
    is(gBrowser.docShellIsActive, true, "Docshell should be active again");
}
