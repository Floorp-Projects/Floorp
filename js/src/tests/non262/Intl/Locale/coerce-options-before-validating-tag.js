// |reftest| skip-if(!this.hasOwnProperty('Intl'))

// Throw a TypeError when the |options| argument is null before validating the
// language tag argument.
assertThrowsInstanceOf(() => {
  new Intl.Locale("invalid_tag", null);
}, TypeError);

if (typeof reportCompare === "function")
  reportCompare(0, 0);
