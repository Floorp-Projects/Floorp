/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


const originalValues = {
  requested: Services.locale.requestedLocales,
};

const l10nReg = new L10nRegistry();

function getMockRegistry() {
  const mockSource = L10nFileSource.createMock("test", "app", ["en-US"], "/localization/{locale}/", [
    {
      path: "/localization/en-US/mock.ftl",
      source: `
key = This is a single message
   .tooltip = This is a tooltip
   .accesskey = f
`
    }
  ]);
  let registry = new L10nRegistry();
  registry.registerSources([mockSource]);
  return registry;
}

function getAttributeByName(attributes, name) {
  return attributes.find(attr => attr.name === name);
}

/**
 * This test verifies that as we switch between
 * different pseudo strategies, the Localization object
 * follows and formats using the given strategy.
 *
 * We test values and attributes and make sure that
 * a single-character attributes, commonly used for access keys
 * don't get transformed.
 */
add_task(async function test_pseudo_works() {
  Services.prefs.setStringPref("intl.l10n.pseudo", "");

  let mockRegistry = getMockRegistry();

  const l10n = new Localization([
    "mock.ftl",
  ], false, mockRegistry);

  {
    // 1. Start with no pseudo

    let message = (await l10n.formatMessages([{id: "key"}]))[0];

    ok(message.value.includes("This is a single message"));
    let attr0 = getAttributeByName(message.attributes, "tooltip");
    ok(attr0.value.includes("This is a tooltip"));
    let attr1 = getAttributeByName(message.attributes, "accesskey");
    equal(attr1.value, "f");
  }

  {
    // 2. Set Accented Pseudo

    Services.prefs.setStringPref("intl.l10n.pseudo", "accented");
    let message = (await l10n.formatMessages([{id: "key"}]))[0];

    ok(message.value.includes("[Ŧħiş iş aa şiƞɠŀee ḿeeşşaaɠee]"));
    let attr0 = getAttributeByName(message.attributes, "tooltip");
    ok(attr0.value.includes("[Ŧħiş iş aa ŧooooŀŧiƥ]"));
    let attr1 = getAttributeByName(message.attributes, "accesskey");
    equal(attr1.value, "f");
  }

  {
    // 3. Set Bidi Pseudo

    Services.prefs.setStringPref("intl.l10n.pseudo", "bidi");
    let message = (await l10n.formatMessages([{id: "key"}]))[0];

    ok(message.value.includes("iş a şiƞɠŀe ḿeşşaɠe"));
    let attr0 = getAttributeByName(message.attributes, "tooltip");
    ok(attr0.value.includes("Ŧħiş iş a ŧooŀŧiƥ"));
    let attr1 = getAttributeByName(message.attributes, "accesskey");
    equal(attr1.value, "f");
  }

  {
    // 4. Remove pseudo

    Services.prefs.setStringPref("intl.l10n.pseudo", "");
    let message = (await l10n.formatMessages([{id: "key"}]))[0];

    ok(message.value.includes("This is a single message"));
    let attr0 = getAttributeByName(message.attributes, "tooltip");
    ok(attr0.value.includes("This is a tooltip"));
    let attr1 = getAttributeByName(message.attributes, "accesskey");
    equal(attr1.value, "f");
  }

  Services.locale.requestedLocales = originalValues.requested;
});

/**
 * This test verifies that setting a bogus pseudo locale
 * strategy doesn't break anything.
 */
add_task(async function test_unavailable_strategy_works() {
  Services.prefs.setStringPref("intl.l10n.pseudo", "");

  let mockRegistry = getMockRegistry();

  const l10n = new Localization([
    "mock.ftl",
  ], false, mockRegistry);

  {
    // 1. Set unavailable pseudo strategy
    Services.prefs.setStringPref("intl.l10n.pseudo", "unknown-strategy");

    let message = (await l10n.formatMessages([{id: "key"}]))[0];

    ok(message.value.includes("This is a single message"));
    let attr0 = getAttributeByName(message.attributes, "tooltip");
    ok(attr0.value.includes("This is a tooltip"));
    let attr1 = getAttributeByName(message.attributes, "accesskey");
    equal(attr1.value, "f");
  }

  Services.prefs.setStringPref("intl.l10n.pseudo", "");
  Services.locale.requestedLocales = originalValues.requested;
});
