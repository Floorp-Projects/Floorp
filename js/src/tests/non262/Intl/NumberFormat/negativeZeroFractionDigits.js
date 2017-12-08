// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const optionsList = [
    {minimumFractionDigits: -0, maximumFractionDigits: -0},
    {minimumFractionDigits: -0, maximumFractionDigits: +0},
    {minimumFractionDigits: +0, maximumFractionDigits: -0},
    {minimumFractionDigits: +0, maximumFractionDigits: +0},
];

for (let options of optionsList) {
    let numberFormat = new Intl.NumberFormat("en-US", options);

    let {minimumFractionDigits, maximumFractionDigits} = numberFormat.resolvedOptions();
    assertEq(minimumFractionDigits, +0);
    assertEq(maximumFractionDigits, +0);

    assertEq(numberFormat.format(123), "123");
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
