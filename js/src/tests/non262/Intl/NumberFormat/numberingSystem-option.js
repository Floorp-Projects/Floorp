const defaultLocale = "en";
const defaultNumberingSystem = new Intl.NumberFormat(defaultLocale).resolvedOptions().numberingSystem;

function createWithLocale(locale, numberingSystem) {
  return new Intl.NumberFormat(locale, {numberingSystem});
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
let nf = create(undefined);
assertEq(nf.resolvedOptions().numberingSystem, defaultNumberingSystem);

// Unsupported numbering systems are ignored.
nf = create("xxxxxxxx");
assertEq(nf.resolvedOptions().numberingSystem, defaultNumberingSystem);

// Numbering system in options overwrite Unicode extension keyword.
nf = createWithLocale(`${defaultLocale}-u-nu-thai`, "arab");
assertEq(nf.resolvedOptions().locale, defaultLocale);
assertEq(nf.resolvedOptions().numberingSystem, "arab");

// |numberingSystem| option ignores case.
nf = create("ARAB");
assertEq(nf.resolvedOptions().locale, defaultLocale);
assertEq(nf.resolvedOptions().numberingSystem, "arab");

for (let [numberingSystem, {algorithmic}] of Object.entries(numberingSystems)) {
  let nf1 = new Intl.NumberFormat(`${defaultLocale}-u-nu-${numberingSystem}`);
  let nf2 = new Intl.NumberFormat(defaultLocale, {numberingSystem});

  if (!algorithmic) {
    assertEq(nf1.resolvedOptions().numberingSystem, numberingSystem);
    assertEq(nf2.resolvedOptions().numberingSystem, numberingSystem);
  } else {
    // We don't yet support algorithmic numbering systems.
    assertEq(nf1.resolvedOptions().numberingSystem, defaultNumberingSystem);
    assertEq(nf2.resolvedOptions().numberingSystem, defaultNumberingSystem);
  }

  assertEq(nf2.format(0), nf1.format(0));
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
