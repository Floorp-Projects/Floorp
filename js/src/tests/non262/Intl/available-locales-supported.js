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

  // All available locales must be reported as supported.
  let supported = intlConstructor.supportedLocalesOf(available);
  assertEqArray(supported, available);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
