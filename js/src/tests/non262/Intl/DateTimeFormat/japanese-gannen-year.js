// |reftest| skip-if(!this.hasOwnProperty("Intl"))

var dtf = new Intl.DateTimeFormat("ja-u-ca-japanese", {
    era: "short",
    timeZone: "Asia/Tokyo",
});

var endShowa = new Date("1989-01-07T00:00:00.000Z");
var startHeisei = new Date("1989-01-08T00:00:00.000Z");

assertEq(dtf.format(endShowa), "昭和64/1/7");
assertEq(dtf.format(startHeisei), "平成1/1/8");

var parts = dtf.formatToParts(startHeisei);
assertEq(parts.filter(p => p.type === "era")[0].value, "平成");
assertEq(parts.filter(p => p.type === "year")[0].value, "1");

var dtf = new Intl.DateTimeFormat("ja-u-ca-japanese", {
    era: "short",
    year: "numeric",
    month: "long",
    day: "numeric",
    timeZone: "Asia/Tokyo",
});

assertEq(dtf.format(endShowa), "昭和64年1月7日");
assertEq(dtf.format(startHeisei), "平成元年1月8日");

var parts = dtf.formatToParts(startHeisei);
assertEq(parts.filter(p => p.type === "era")[0].value, "平成");
assertEq(parts.filter(p => p.type === "year")[0].value, "元");

// ICU returns mixed numbers when an explicit numbering system is present.

var dtf = new Intl.DateTimeFormat("ja-u-ca-japanese-nu-arab", {
    era: "short",
    timeZone: "Asia/Tokyo",
});

assertEq(dtf.format(endShowa), "昭和٦٤/١/٧");
assertEq(dtf.format(startHeisei), "平成١/١/٨");

var dtf = new Intl.DateTimeFormat("ja-u-ca-japanese-nu-arab", {
    era: "short",
    year: "numeric",
    month: "numeric",
    timeZone: "Asia/Tokyo",
});

assertEq(dtf.format(endShowa), "昭和64年١月");
assertEq(dtf.format(startHeisei), "平成元年١月");

if (typeof reportCompare === "function")
    reportCompare(0, 0);
