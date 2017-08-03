// The purpose of this test is to ensure that by default, BrowserUITelemetry
// isn't reporting any UI customizations. This is primarily so changes to
// customizableUI (eg, new buttons, button location changes) also have a
// corresponding BrowserUITelemetry change.

function test() {
  let s = {};
  Cu.import("resource:///modules/CustomizableUI.jsm", s);
  Cu.import("resource:///modules/BrowserUITelemetry.jsm", s);

  let { CustomizableUI, BrowserUITelemetry } = s;

  // Bug 1278176 - DevEdition never has the UI in a default state by default.
  if (!AppConstants.MOZ_DEV_EDITION) {
    Assert.ok(CustomizableUI.inDefaultState,
              "No other test should have left CUI in a dirty state.");
  }

  let result = BrowserUITelemetry._getWindowMeasurements(window, 0);

  // Bug 1278176 - DevEdition always reports the developer-button is moved.
  if (!AppConstants.MOZ_DEV_EDITION) {
    Assert.deepEqual(result.defaultMoved, []);
  }
  Assert.deepEqual(result.nondefaultAdded, []);

  Assert.deepEqual(result.defaultRemoved, []);

  // And mochi insists there's only a single window with a single tab when
  // starting a test, so check that for good measure.
  Assert.deepEqual(result.visibleTabs, [1]);
}
