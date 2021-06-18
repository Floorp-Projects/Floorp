// |reftest| skip-if(!this.hasOwnProperty('Intl'))

if (typeof getAvailableLocalesOf === "undefined") {
  var getAvailableLocalesOf = SpecialPowers.Cu.getJSTestingFunctions().getAvailableLocalesOf;
}

function IsIntlService(c) {
  return typeof c === "function" &&
         c.hasOwnProperty("prototype") &&
         c.prototype.hasOwnProperty("resolvedOptions");
}

const intlConstructors = Object.getOwnPropertyNames(Intl).map(name => Intl[name]).filter(IsIntlService);

// Test all Intl service constructors.
for (let intlConstructor of intlConstructors) {
  // Retrieve all available locales of the given Intl service constructor.
  let available = getAvailableLocalesOf(intlConstructor.name);

  // "best fit" matchers could potentially return a different locale, so we only
  // test with "lookup" locale matchers. (NB: We don't yet support "best fit"
  // matchers.)
  let options = {localeMatcher: "lookup"};

  if (intlConstructor === Intl.DisplayNames) {
    // Intl.DisplayNames can't be constructed without the "type" option.
    options.type = "language";
  }

  for (let locale of available) {
    let obj = new intlConstructor(locale, options);
    let resolved = obj.resolvedOptions();

    // If |locale| is an available locale, the "lookup" locale matcher should
    // pick exactly that locale.
    assertEq(resolved.locale, locale);
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
