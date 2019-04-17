/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/Preferences.jsm", this);

// List of default preferences that can be used for tests, chosen because they
// have little or no side-effects when they are modified for a brief time. If
// any of these preferences are removed or their default state changes, just
// update the constant to point to a different preference with the same default.
const PREF_BOOLEAN_DEFAULT_TRUE = "accessibility.typeaheadfind.manual";
const PREF_BOOLEAN_USERVALUE_TRUE = "browser.dom.window.dump.enabled";
const PREF_NUMBER_DEFAULT_ZERO = "accessibility.typeaheadfind.casesensitive";
const PREF_STRING_DEFAULT_EMPTY = "browser.helperApps.neverAsk.openFile";
const PREF_STRING_DEFAULT_NOTEMPTY = "accessibility.typeaheadfind.soundURL";
const PREF_STRING_DEFAULT_NOTEMPTY_VALUE = "beep";
const PREF_STRING_LOCALIZED_MISSING = "gecko.handlerService.schemes.irc.1.name";

// Other preference names used in tests.
const PREF_NEW = "test.aboutconfig.new";

// These tests can be slow to execute because they show all the preferences
// several times, and each time can require a second on some virtual machines.
requestLongerTimeout(2);

class AboutConfigRowTest {
  constructor(element) {
    this.element = element;
  }

  querySelector(selector) {
    return this.element.querySelector(selector);
  }

  get nameCell() {
    return this.querySelector("th");
  }

  get name() {
    return this.nameCell.textContent;
  }

  get valueCell() {
    return this.querySelector("td.cell-value");
  }

  get value() {
    return this.valueCell.textContent;
  }

  /**
   * Text input field when the row is in edit mode.
   */
  get valueInput() {
    return this.valueCell.querySelector("input");
  }

  /**
   * This is normally "edit" or "toggle" based on the preference type, "save"
   * when the row is in edit mode, or "add" when the preference does not exist.
   */
  get editColumnButton() {
    return this.querySelector("td.cell-edit > button");
  }

  /**
   * This can be "reset" or "delete" based on whether a default exists.
   */
  get resetColumnButton() {
    return this.querySelector("td:last-child > button");
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
    this.browser = browser;
    this.document = browser.contentDocument;
    this.window = browser.contentWindow;
  }

  async setupNewTab(options) {
    await this.document.l10n.ready;
    if (!options.dontBypassWarning) {
      this.bypassWarningButton.click();
      this.showAll();
    }
  }

  get showWarningNextTimeInput() {
    return this.document.getElementById("showWarningNextTime");
  }

  get bypassWarningButton() {
    return this.document.querySelector("button[autofocus]");
  }

  get searchInput() {
    return this.document.getElementById("about-config-search");
  }

  get prefsTable() {
    return this.document.getElementById("prefs");
  }

  /**
   * Array of AboutConfigRowTest objects, one for each row in the main table.
   */
  get rows() {
    let elements = this.prefsTable.querySelectorAll("tr:not(.hidden)");
    return Array.from(elements, element => new AboutConfigRowTest(element));
  }

  /**
   * Returns the AboutConfigRowTest object for the row in the main table which
   * corresponds to the given preference name, or undefined if none is present.
   */
  getRow(name) {
    return this.rows.find(row => row.name == name);
  }

  /**
   * Shows all preferences using the dedicated button.
   */
  showAll() {
    this.search("");
    this.document.getElementById("show-all").click();
  }

  /**
   * Performs a new search using the dedicated textbox. This also makes sure
   * that the list of preferences displayed is up to date.
   */
  search(value) {
    this.searchInput.value = value;
    this.searchInput.focus();
    EventUtils.sendKey("return");
  }

  /**
   * Checks whether or not the initial warning page is displayed.
   */
  assertWarningPage(expected) {
    Assert.equal(!!this.showWarningNextTimeInput, expected);
    Assert.equal(!!this.bypassWarningButton, expected);
    Assert.equal(!this.searchInput, expected);
    Assert.equal(!this.prefsTable, expected);
  }
}
