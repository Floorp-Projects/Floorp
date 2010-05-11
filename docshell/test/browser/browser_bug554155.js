function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab("http://example.com");

  tab.linkedBrowser.addEventListener('load', function() {
    let numLocationChanges = 0;

    let listener = {
      onStateChange:    function() {},
      onProgressChange: function() {},
      onStatusChange:   function() {},
      onSecurityChange: function() {},
      onLocationChange: function() {
        numLocationChanges++;
      }
    };

    gBrowser.addTabsProgressListener(listener, Components.interfaces.nsIWebProgress.NOTIFY_ALL);

    // pushState to a new URL (http://example.com/foo").  This should trigger
    // exactly one LocationChange event.
    tab.linkedBrowser.contentWindow.history.pushState(null, null, "foo");

    executeSoon(function() {
      is(numLocationChanges, 1,
         "pushState should cause exactly one LocationChange event.");
      finish();
    });

  }, true);
}
