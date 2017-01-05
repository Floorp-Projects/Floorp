// |reftest| skip-if(!this.hasOwnProperty("Intl")||!this.hasOwnProperty('addIntlExtras'))

addIntlExtras(Intl);

const optionsList = [
    {minimumFractionDigits: -0, maximumFractionDigits: -0},
    {minimumFractionDigits: -0, maximumFractionDigits: +0},
    {minimumFractionDigits: +0, maximumFractionDigits: -0},
    {minimumFractionDigits: +0, maximumFractionDigits: +0},
];

for (let options of optionsList) {
    let pluralRules = new Intl.PluralRules("en-US", options);

    let {minimumFractionDigits, maximumFractionDigits} = pluralRules.resolvedOptions();
    assertEq(minimumFractionDigits, +0);
    assertEq(maximumFractionDigits, +0);

    assertEq(pluralRules.select(123), "other");
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
