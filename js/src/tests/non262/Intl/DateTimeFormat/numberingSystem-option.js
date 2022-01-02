const defaultLocale = "en";
const defaultNumberingSystem = new Intl.DateTimeFormat(defaultLocale).resolvedOptions().numberingSystem;

function createWithLocale(locale, numberingSystem) {
  return new Intl.DateTimeFormat(locale, {numberingSystem});
}

function create(numberingSystem) {
  return createWithLocale(defaultLocale, numberingSystem);
}

// Empty string should throw.
assertThrowsInstanceOf(() => create(""), RangeError);

// Trailing \0 should throw.
assertThrowsInstanceOf(() => create("latn\0"), RangeError);

// Too short or too long strings should throw.
assertThrowsInstanceOf(() => create("a"), RangeError);
assertThrowsInstanceOf(() => create("toolongstring"), RangeError);

// Throw even when prefix is valid.
assertThrowsInstanceOf(() => create("latn-toolongstring"), RangeError);

// |numberingSystem| can be set to |undefined|.
let dtf = create(undefined);
assertEq(dtf.resolvedOptions().numberingSystem, defaultNumberingSystem);

// Unsupported numbering systems are ignored.
dtf = create("xxxxxxxx");
assertEq(dtf.resolvedOptions().numberingSystem, defaultNumberingSystem);

// Numbering system in options overwrite Unicode extension keyword.
dtf = createWithLocale(`${defaultLocale}-u-nu-thai`, "arab");
assertEq(dtf.resolvedOptions().locale, defaultLocale);
assertEq(dtf.resolvedOptions().numberingSystem, "arab");

// |numberingSystem| option ignores case.
dtf = create("ARAB");
assertEq(dtf.resolvedOptions().locale, defaultLocale);
assertEq(dtf.resolvedOptions().numberingSystem, "arab");

for (let [numberingSystem, {algorithmic}] of Object.entries(numberingSystems)) {
  let dtf1 = new Intl.DateTimeFormat(`${defaultLocale}-u-nu-${numberingSystem}`);
  let dtf2 = new Intl.DateTimeFormat(defaultLocale, {numberingSystem});

  if (!algorithmic) {
    assertEq(dtf1.resolvedOptions().numberingSystem, numberingSystem);
    assertEq(dtf2.resolvedOptions().numberingSystem, numberingSystem);
  } else {
    // We don't yet support algorithmic numbering systems.
    assertEq(dtf1.resolvedOptions().numberingSystem, defaultNumberingSystem);
    assertEq(dtf2.resolvedOptions().numberingSystem, defaultNumberingSystem);
  }

  assertEq(dtf2.format(0), dtf1.format(0));
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
