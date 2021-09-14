// |reftest| skip-if(!this.hasOwnProperty('Intl')||!this.hasOwnProperty('addIntlExtras'))

addMozIntlDisplayNames(this);

const tests = {
  "en": {
    long: ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"],
    abbreviated: ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"],
    short: ["Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"],
    narrow: ["M", "T", "W", "T", "F", "S", "S"],
  },
};

for (let [locale, localeTests] of Object.entries(tests)) {
  for (let [style, weekdays] of Object.entries(localeTests)) {
    let dn = new Intl.DisplayNames(locale, {type: "weekday", style});

    let resolved = dn.resolvedOptions();
    assertEq(resolved.style, style);

    for (let [day, expected] of weekdays.entries()) {
      assertEq(dn.of(day + 1), expected);
    }
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
