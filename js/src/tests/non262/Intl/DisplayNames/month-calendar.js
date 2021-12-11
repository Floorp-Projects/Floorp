// |reftest| skip-if(!this.hasOwnProperty('Intl')||!this.hasOwnProperty('addIntlExtras'))

addMozIntlDisplayNames(this);

let dn1 = new Intl.DisplayNames("en", {type: "month", calendar: "gregory"});
assertEq(dn1.of(1), "January");
assertEq(dn1.resolvedOptions().calendar, "gregory");

let dn2 = new Intl.DisplayNames("en", {type: "month", calendar: "hebrew"});
assertEq(dn2.of(1), "Tishri");
assertEq(dn2.resolvedOptions().calendar, "hebrew");

let dn3 = new Intl.DisplayNames("en", {type: "month", calendar: "islamicc"});
assertEq(dn3.of(1), "Muharram");
assertEq(dn3.resolvedOptions().calendar, "islamic-civil");

if (typeof reportCompare === "function")
  reportCompare(true, true);
