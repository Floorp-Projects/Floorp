function test() {
  ok(!!gBrowser, "gBrowser exists");
  is(gBrowser, getBrowser(), "both ways of getting tabbrowser work");
}
