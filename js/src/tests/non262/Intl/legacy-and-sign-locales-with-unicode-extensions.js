// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const languageTags = {
  // Case 1: legacy, regular tags.
  "art-lojban": "jbo",
  "cel-gaulish": "xtg",
  "zh-guoyu": "zh",
  "zh-hakka": "hak",
  "zh-xiang": "hsn",

  // Case 2: redundant language tags.
  "sgn-BR": "bzs",
  "sgn-CO": "csn",
  "sgn-DE": "gsg",
  "sgn-DK": "dsl",
  "sgn-ES": "ssp",
  "sgn-FR": "fsl",
  "sgn-GB": "bfi",
  "sgn-GR": "gss",
  "sgn-IE": "isg",
  "sgn-IT": "ise",
  "sgn-JP": "jsl",
  "sgn-MX": "mfs",
  "sgn-NI": "ncs",
  "sgn-NL": "dse",
  "sgn-NO": "nsi",
  "sgn-PT": "psr",
  "sgn-SE": "swl",
  "sgn-US": "ase",
  "sgn-ZA": "sfs",

  // Case 3: the special case.
  "ja-Latn-hepburn-heploc": "ja-Latn-alalc97",
};
const extensions = ["-u-attr", "-u-nu-latn"];
const intlConstructors = [Intl.Collator, Intl.DateTimeFormat, Intl.NumberFormat];
const options = [{localeMatcher: "best fit"}, {localeMatcher: "lookup"}];

for (let [tag, canonical] of Object.entries(languageTags)) {
  // Ensure Intl.getCanonicalLocales canonicalizes the language tag correctly.
  assertEq(Intl.getCanonicalLocales(tag)[0], canonical);

  // Unicode extensions don't change the canonicalization result.
  for (let ext of extensions) {
    assertEq(Intl.getCanonicalLocales(tag + ext)[0], canonical + ext);
    assertEq(Intl.getCanonicalLocales(canonical + ext)[0], canonical + ext);
  }

  for (let intlCtor of intlConstructors) {
    for (let opt of options) {
      // The non-canonical tag is supported iff the canonical tag is supported.
      let supported = intlCtor.supportedLocalesOf(tag, opt);
      let supportedCanonical = intlCtor.supportedLocalesOf(canonical, opt);
      assertEq(supported.length, supportedCanonical.length);

      let isSupported = supported.length > 0;
      if (isSupported) {
        assertEq(supported[0], canonical);
        assertEq(supportedCanonical[0], canonical);
      }

      // Unicode extensions don't change the result of supportedLocalesOf().
      for (let ext of extensions) {
        let supportedExt = intlCtor.supportedLocalesOf(tag + ext, opt);
        let supportedCanonicalExt = intlCtor.supportedLocalesOf(canonical + ext, opt);
        assertEq(supportedExt.length, supportedCanonical.length);
        assertEq(supportedCanonicalExt.length, supportedCanonical.length);

        if (isSupported) {
          assertEq(supportedExt[0], canonical + ext);
          assertEq(supportedCanonicalExt[0], canonical + ext);
        }
      }
    }
  }
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
