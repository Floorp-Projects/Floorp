// This test is useful because mochitest-browser runs as an addon, so we test
// addon-scope paths here.
var ifr;
function test() {
  ifr = document.createElement('iframe');
  document.getElementById('main-window').appendChild(ifr);
  is(ifr.contentDocument.nodePrincipal.origin, "[System Principal]");
  ifr.contentDocument.open();
  ok(true, "Didn't throw");
}
registerCleanupFunction(() => ifr.remove());
