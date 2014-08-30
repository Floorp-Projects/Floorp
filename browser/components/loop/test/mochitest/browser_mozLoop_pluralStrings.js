/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This is an integration test from navigator.mozLoop through to the end
 * effects - rather than just testing MozLoopAPI alone.
 */

add_task(loadLoopPanel);

add_task(function* test_mozLoop_pluralStrings() {
  Assert.ok(gMozLoopAPI, "mozLoop should exist");

  var strings = JSON.parse(gMozLoopAPI.getStrings("feedback_window_will_close_in2"));
  Assert.equal(gMozLoopAPI.getPluralForm(0, strings.textContent),
               "This window will close in {{countdown}} seconds");
  Assert.equal(gMozLoopAPI.getPluralForm(1, strings.textContent),
               "This window will close in {{countdown}} second");
});
