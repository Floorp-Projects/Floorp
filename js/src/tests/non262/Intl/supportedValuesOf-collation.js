// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const collations = Intl.supportedValuesOf("collation");

assertEq(new Set(collations).size, collations.length, "No duplicates are present");
assertEqArray(collations, [...collations].sort(), "The list is sorted");

const typeRE = /^[a-z0-9]{3,8}(-[a-z0-9]{3,8})*$/;
for (let collation of collations) {
  assertEq(typeRE.test(collation), true, `${collation} matches the 'type' production`);
}

for (let collation of collations) {
  assertEq(new Intl.Locale("und", {collation}).collation, collation, `${collation} is canonicalised`);
}

// Not all locales support all possible collations, so test the minimal set to
// cover all supported collations.
const locales = [
  "en", // "emoji", "eor"
  "ar", // compat
  "de", // phonebk
  "es", // trad
  "ko", // searchjl
  "ln", // phonetic
  "si", // dict
  "sv", // reformed
  "zh", // big5han, gb2312, pinyin, stroke, unihan, zhuyin
];

for (let collation of collations) {
  let supported = false;
  for (let locale of locales) {
    let obj = new Intl.Collator(locale, {collation});
    if (obj.resolvedOptions().collation === collation) {
      supported = true;
    }
  }

  assertEq(supported, true, `${collation} is supported by Collator`);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
