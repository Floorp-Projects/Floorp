const URL = "http://mochi.test:8888/browser/browser/base/content/test/general/file_trackingUI_6.html";

function waitForSecurityChange(numChanges = 1) {
  return new Promise(resolve => {
    let n = 0;
    let listener = {
      onSecurityChange() {
        n = n + 1;
        info("Received onSecurityChange event " + n + " of " + numChanges);
        if (n >= numChanges) {
          gBrowser.removeProgressListener(listener);
          resolve();
        }
      }
    };
    gBrowser.addProgressListener(listener);
  });
}

add_task(async function test_fetch() {
  await SpecialPowers.pushPrefEnv({ set: [["privacy.trackingprotection.enabled", true]] });

  await BrowserTestUtils.withNewTab({ gBrowser, url: URL }, async function(newTabBrowser) {
    let securityChange = waitForSecurityChange();
    await ContentTask.spawn(newTabBrowser, null, async function() {
      await content.wrappedJSObject.test_fetch()
                   .then(response => Assert.ok(false, "should have denied the request"))
                   .catch(e => Assert.ok(true, `Caught exception: ${e}`));
    });
    await securityChange;

    var TrackingProtection = newTabBrowser.ownerGlobal.TrackingProtection;
    ok(TrackingProtection, "got TP object");
    ok(TrackingProtection.enabled, "TP is enabled");

    is(TrackingProtection.content.getAttribute("state"), "blocked-tracking-content",
        'content: state="blocked-tracking-content"');
    is(TrackingProtection.icon.getAttribute("state"), "blocked-tracking-content",
        'icon: state="blocked-tracking-content"');
    is(TrackingProtection.icon.getAttribute("tooltiptext"),
       gNavigatorBundle.getString("trackingProtection.icon.activeTooltip"), "correct tooltip");
  });
});
