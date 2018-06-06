/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { Localization } = ChromeUtils.import("resource://gre/modules/Localization.jsm", {});
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm", {});
const { L10nRegistry, FileSource } =
  ChromeUtils.import("resource://gre/modules/L10nRegistry.jsm", {});

const originalValues = {};

function addMockFileSource() {

  const fs = {
    "/localization/de/browser/menu.ftl": `
key = This is a single message
    .tooltip = This is a tooltip
    .accesskey = f`,
  };
  originalValues.load = L10nRegistry.load;
  originalValues.requested = Services.locale.getRequestedLocales();

  L10nRegistry.load = async function(url) {
    return fs[url];
  };

  const source = new FileSource("test", ["de"], "/localization/{locale}");
  L10nRegistry.registerSource(source);

  return async function* generateMessages(resIds) {
    yield * await L10nRegistry.generateContexts(["de"], resIds);
  };
}

/**
 * This test verifies that as we switching between
 * different pseudo strategies the Localization object
 * follows and formats using the given strategy.
 *
 * We test values and attributes and make sure that
 * a single-character attributes, commonly used for access keys
 * don't get transformed.
 */
add_task(async function test_accented_works() {
  Services.prefs.setStringPref("intl.l10n.pseudo", "");

  let generateMessages = addMockFileSource();

  const l10n = new Localization([
    "/browser/menu.ftl"
  ], generateMessages);
  l10n.registerObservers();

  {
    // 1. Start with no pseudo

    let message = (await l10n.formatMessages([{id: "key"}]))[0];

    ok(message.value.includes("This is a single message"));
    ok(message.attributes[0].value.includes("This is a tooltip"));
    equal(message.attributes[1].value, "f");
  }

  {
    // 2. Set Accented Pseudo

    Services.prefs.setStringPref("intl.l10n.pseudo", "accented");
    let message = (await l10n.formatMessages([{id: "key"}]))[0];

    ok(message.value.includes("Ŧħīş īş ȧȧ şīƞɠŀḗḗ ḿḗḗşşȧȧɠḗḗ"));
    ok(message.attributes[0].value.includes("Ŧħīş īş ȧȧ ŧǿǿǿǿŀŧīƥ"));
    equal(message.attributes[1].value, "f");
  }

  {
    // 3. Set Bidi Pseudo

    Services.prefs.setStringPref("intl.l10n.pseudo", "bidi");
    let message = (await l10n.formatMessages([{id: "key"}]))[0];

    ok(message.value.includes("ıs ɐ sıuƃʅǝ ɯǝssɐƃǝ"));
    ok(message.attributes[0].value.includes("⊥ɥıs ıs ɐ ʇooʅʇıd"));
    equal(message.attributes[1].value, "f");
  }

  {
    // 4. Remove pseudo

    Services.prefs.setStringPref("intl.l10n.pseudo", "");
    let message = (await l10n.formatMessages([{id: "key"}]))[0];

    ok(message.value.includes("This is a single message"));
    ok(message.attributes[0].value.includes("This is a tooltip"));
    equal(message.attributes[1].value, "f");
  }

  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
  L10nRegistry.load = originalValues.load;
  Services.locale.setRequestedLocales(originalValues.requested);
});

/**
 * This test verifies that setting a bogus pseudo locale
 * strategy doesn't break anything.
 */
add_task(async function test_unavailable_strategy_works() {
  Services.prefs.setStringPref("intl.l10n.pseudo", "");

  let generateMessages = addMockFileSource();

  const l10n = new Localization([
    "/browser/menu.ftl"
  ], generateMessages);
  l10n.registerObservers();

  {
    // 1. Set unavailable pseudo strategy
    Services.prefs.setStringPref("intl.l10n.pseudo", "unknown-strategy");

    let message = (await l10n.formatMessages([{id: "key"}]))[0];

    ok(message.value.includes("This is a single message"));
    ok(message.attributes[0].value.includes("This is a tooltip"));
    equal(message.attributes[1].value, "f");
  }

  Services.prefs.setStringPref("intl.l10n.pseudo", "");
  L10nRegistry.sources.clear();
  L10nRegistry.ctxCache.clear();
  L10nRegistry.load = originalValues.load;
  Services.locale.setRequestedLocales(originalValues.requested);
});
