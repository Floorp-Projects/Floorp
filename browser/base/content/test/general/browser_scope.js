///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("TypeError: this.docShell is null");

function test() {
  ok(!!gBrowser, "gBrowser exists");
  is(gBrowser, getBrowser(), "both ways of getting tabbrowser work");
}
