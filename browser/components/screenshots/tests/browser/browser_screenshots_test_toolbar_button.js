/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testScreenshotButtonDisabled() {
  info("Test the Screenshots button in the panel");

  let screenshotBtn = document.getElementById("screenshot-button");
  Assert.ok(screenshotBtn, "The screenshots button was added to the nav bar");

  await BrowserTestUtils.withNewTab("https://example.com/", () => {
    Assert.equal(
      screenshotBtn.disabled,
      false,
      "Screenshots button is enabled"
    );
  });
  await BrowserTestUtils.withNewTab("about:home", () => {
    Assert.equal(
      screenshotBtn.disabled,
      true,
      "Screenshots button is now disabled"
    );
  });
});
