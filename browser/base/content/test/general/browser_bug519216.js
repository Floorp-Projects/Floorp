function test() {
  waitForExplicitFinish();
  gBrowser.addProgressListener(progressListener1);
  gBrowser.addProgressListener(progressListener2);
  gBrowser.addProgressListener(progressListener3);
  BrowserTestUtils.startLoadingURIString(gBrowser, "data:text/plain,bug519216");
}

var calledListener1 = false;
var progressListener1 = {
  onLocationChange: function onLocationChange() {
    calledListener1 = true;
    gBrowser.removeProgressListener(this);
  },
};

var calledListener2 = false;
var progressListener2 = {
  onLocationChange: function onLocationChange() {
    ok(calledListener1, "called progressListener1 before progressListener2");
    calledListener2 = true;
    gBrowser.removeProgressListener(this);
  },
};

var progressListener3 = {
  onLocationChange: function onLocationChange() {
    ok(calledListener2, "called progressListener2 before progressListener3");
    gBrowser.removeProgressListener(this);
    gBrowser.addProgressListener(progressListener4);
    executeSoon(function () {
      expectListener4 = true;
      gBrowser.reload();
    });
  },
};

var expectListener4 = false;
var progressListener4 = {
  onLocationChange: function onLocationChange() {
    ok(
      expectListener4,
      "didn't call progressListener4 for the first location change"
    );
    gBrowser.removeProgressListener(this);
    executeSoon(finish);
  },
};
