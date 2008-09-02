var listener = {
  testFunction : null,
  
  handleEvent : function (e) {
    this.testFunction();
  }
}

var newBrowser;

/* Tests for correct behaviour of getEffectiveHost on identity handler */
function test() {
  
  waitForExplicitFinish();
  
  ok(gIdentityHandler, "gIdentityHandler should exist");
  
  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  newBrowser = gBrowser.getBrowserForTab(newTab);
  
  newBrowser.addEventListener("load", listener, true);
  listener.testFunction = testNormalDomain;  
  newBrowser.contentWindow.location = 'http://test1.example.org/';
}

function testNormalDomain() {
  is(gIdentityHandler._lastLocation.host, 'test1.example.org', "Identity handler is getting the full location");
  is(gIdentityHandler.getEffectiveHost(), 'example.org', "getEffectiveHost should return example.org for test1.example.org");
  
  listener.testFunction = testNormalDomainWithPort;
  newBrowser.contentWindow.location = 'http://sub1.test1.example.org:8000/';
}

function testNormalDomainWithPort() {

  is(gIdentityHandler._lastLocation.host, 'sub1.test1.example.org:8000', "Identity handler is getting port information");
  is(gIdentityHandler.getEffectiveHost(), 'example.org', "getEffectiveHost should return example.org for sub1.test1.example.org:8000");
  
  listener.testFunction = testIPWithPort;
  newBrowser.contentWindow.location = 'http://127.0.0.1:8888/';
}

function testIPWithPort() {
  is(gIdentityHandler.getEffectiveHost(), '127.0.0.1', "getEffectiveHost should return 127.0.0.1 for 127.0.0.1:8888");
  newBrowser.removeEventListener("load", listener, true);
  gBrowser.removeCurrentTab();
  finish();
}
