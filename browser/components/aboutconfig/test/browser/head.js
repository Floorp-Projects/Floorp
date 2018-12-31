/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/Preferences.jsm", this);

class AboutConfigRowTest {
  constructor(element) {
    this.element = element;
  }

  querySelector(selector) {
    return this.element.querySelector(selector);
  }

  get name() {
    return this.querySelector("td").textContent;
  }

  get value() {
    return this.querySelector("td.cell-value").textContent;
  }

  get firstButton() {
    return this.querySelector("button");
  }

  hasClass(className) {
    return this.element.classList.contains(className);
  }
}

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

  /**
   * Array of AboutConfigRowTest objects, one for each row in the main table.
   */
  get rows() {
    let elements = this.document.getElementById("prefs")
                                .getElementsByTagName("tr");
    return Array.map(elements, element => new AboutConfigRowTest(element));
  }

  /**
   * Returns the AboutConfigRowTest object for the row in the main table which
   * corresponds to the given preference name, or undefined if none is present.
   */
  getRow(name) {
    return this.rows.find(row => row.name == name);
  }
}
