// |reftest| skip-if(!this.hasOwnProperty('Intl')||!this.hasOwnProperty('addIntlExtras'))

addMozIntlDisplayNames(this);

const tests = {
  "en": {
    long: ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December", 13],
    short: ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", 13],
    narrow: ["J", "F", "M", "A", "M", "J", "J", "A", "S", "O", "N", "D", 13],
  },
  "de": {
    long: ["Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober", "November", "Dezember", 13],
    short: ["Jan", "Feb", "Mär", "Apr", "Mai", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dez", 13],
    narrow: ["J", "F", "M", "A", "M", "J", "J", "A", "S", "O", "N", "D", 13],
  },
  "fr": {
    long: ["janvier", "février", "mars", "avril", "mai", "juin", "juillet", "août", "septembre", "octobre", "novembre", "décembre", 13],
    short: ["janv.", "févr.", "mars", "avr.", "mai", "juin", "juil.", "août", "sept.", "oct.", "nov.", "déc.", 13],
    narrow: ["J", "F", "M", "A", "M", "J", "J", "A", "S", "O", "N", "D", 13],
  },
  "zh": {
    long: ["一月", "二月", "三月", "四月", "五月", "六月", "七月", "八月", "九月", "十月", "十一月", "十二月", 13],
    short: ["1月", "2月", "3月", "4月", "5月", "6月", "7月", "8月", "9月", "10月", "11月", "12月", 13],
    narrow: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", 13],
  },
  "zh-u-ca-chinese": {
    long: ["正月", "二月", "三月", "四月", "五月", "六月", "七月", "八月", "九月", "十月", "十一月", "腊月", 13],
    short: ["正月", "二月", "三月", "四月", "五月", "六月", "七月", "八月", "九月", "十月", "十一月", "腊月", 13],
    narrow: ["正", "二", "三", "四", "五", "六", "七", "八", "九", "十", "冬", "腊", 13],
  },
  "en-u-ca-hebrew": {
    long: ["Tishri", "Heshvan", "Kislev", "Tevet", "Shevat", "Adar I", "Adar", "Nisan", "Iyar", "Sivan", "Tamuz", "Av", "Elul"],
    short: ["Tishri", "Heshvan", "Kislev", "Tevet", "Shevat", "Adar I", "Adar", "Nisan", "Iyar", "Sivan", "Tamuz", "Av", "Elul"],
    narrow: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13"],
  },
};

for (let [locale, localeTests] of Object.entries(tests)) {
  let defaultCalendar = new Intl.DateTimeFormat(locale).resolvedOptions().calendar;

  for (let [style, styleTests] of Object.entries(localeTests)) {
    let dn = new Intl.DisplayNames(locale, {type: "month", style});

    let resolved = dn.resolvedOptions();
    assertEq(resolved.locale, locale);
    assertEq(resolved.calendar, defaultCalendar);
    assertEq(resolved.style, style);
    assertEq(resolved.type, "month");
    assertEq(resolved.fallback, "code");

    for (let i = 0; i < 13; i++) {
      assertEq(dn.of(i + 1), String(styleTests[i]));

      // Also works with strings.
      assertEq(dn.of(String(i + 1)), String(styleTests[i]));

      // Also works with objects.
      assertEq(dn.of(Object(i + 1)), String(styleTests[i]));
    }
  }
}

{
  let dn = new Intl.DisplayNames("en", {type: "month"});

  // Performs ToString on the input and then validates the stringified result.
  assertThrowsInstanceOf(() => dn.of(), RangeError);
  assertThrowsInstanceOf(() => dn.of(null), RangeError);
  assertThrowsInstanceOf(() => dn.of(Symbol()), TypeError);

  // Throws an error if |code| isn't an integer.
  assertThrowsInstanceOf(() => dn.of(1.5), RangeError);
  assertThrowsInstanceOf(() => dn.of(-Infinity), RangeError);
  assertThrowsInstanceOf(() => dn.of(Infinity), RangeError);
  assertThrowsInstanceOf(() => dn.of(NaN), RangeError);

  // Throws an error if outside of [1, 13].
  assertThrowsInstanceOf(() => dn.of(-1), RangeError);
  assertThrowsInstanceOf(() => dn.of(0), RangeError);
  assertThrowsInstanceOf(() => dn.of(14), RangeError);
}

// Test fallback behaviour.
{
  let dn1 = new Intl.DisplayNames("en", {type: "month"});
  let dn2 = new Intl.DisplayNames("en", {type: "month", fallback: "code"});
  let dn3 = new Intl.DisplayNames("en", {type: "month", fallback: "none"});

  assertEq(dn1.resolvedOptions().fallback, "code");
  assertEq(dn2.resolvedOptions().fallback, "code");
  assertEq(dn3.resolvedOptions().fallback, "none");

  assertEq(dn1.resolvedOptions().calendar, "gregory");
  assertEq(dn2.resolvedOptions().calendar, "gregory");
  assertEq(dn3.resolvedOptions().calendar, "gregory");

  // The Gregorian calendar doesn't have a thirteenth month.
  assertEq(dn1.of("13"), "13");
  assertEq(dn2.of("13"), "13");
  assertEq(dn3.of("13"), undefined);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
