/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/Preferences.jsm", this);

class AboutConfigTest {
  static withNewTab(testFn, options = {}) {
    return BrowserTestUtils.withNewTab({
      gBrowser,
      url: "chrome://browser/content/aboutconfig/aboutconfig.html",
    }, async browser => {
      let scope = new this(browser);
      await scope.setupNewTab(options);
      await testFn.call(scope);
    });
  }

  constructor(browser) {
    this.document = browser.contentDocument;
  }

  async setupNewTab(options) {
    await this.document.l10n.ready;
    if (!options.dontBypassWarning) {
      this.document.querySelector("button").click();
    }
  }
}
