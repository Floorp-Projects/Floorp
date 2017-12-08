// |reftest| skip-if(!this.hasOwnProperty("Intl"))

function civilDate(options, date) {
    var opts = Object.assign({timeZone: "Asia/Riyadh"}, options);
    return new Intl.DateTimeFormat("ar-SA-u-ca-islamic-civil-nu-latn", opts).format(date);
}

function tabularDate(options, date) {
    var opts = Object.assign({timeZone: "Asia/Riyadh"}, options);
    return new Intl.DateTimeFormat("ar-SA-u-ca-islamic-tbla-nu-latn", opts).format(date);
}

function sightingDate(options, date) {
    var opts = Object.assign({timeZone: "Asia/Riyadh"}, options);
    return new Intl.DateTimeFormat("ar-SA-u-ca-islamic-rgsa-nu-latn", opts).format(date);
}

function ummAlQuraDate(options, date) {
    var opts = Object.assign({timeZone: "Asia/Riyadh"}, options);
    return new Intl.DateTimeFormat("ar-SA-u-ca-umalqura-nu-latn", opts).format(date);
}

// Test islamic-tbla (Tabular / Thursday epoch).
// Compare with islamic-civil (Tabular / Friday epoch).
function testIslamicTbla() {
    var date = new Date(Date.UTC(2015, 1 - 1, 1));

    // Month and year are the same.
    var monthYear = {year: "numeric", month: "numeric"};
    assertEq(civilDate(monthYear, date), tabularDate(monthYear, date));

    // Day is different by one.
    var day = {day: "numeric"};
    assertEq(Number(civilDate(day, date)) - Number(tabularDate(day, date)), -1);
}

// Test islamic-rgsa (Saudi Arabia sighting).
// Sighting of the hilal (crescent moon) in Saudi Arabia.
function testIslamicRgsa() {
    var date1 = new Date(Date.UTC(1975, 5 - 1, 6));
    var date2 = new Date(Date.UTC(2015, 1 - 1, 1));
    var dayMonthYear = {year: "numeric", month: "numeric", day: "numeric"};

    assertEq(sightingDate(dayMonthYear, date1), tabularDate(dayMonthYear, date1));
    assertEq(sightingDate(dayMonthYear, date2), civilDate(dayMonthYear, date2));
}

// Test islamic-umalqura (Umm al-Qura).
function testIslamicUmalqura() {
    var year = {year: "numeric"};
    var month = {month: "numeric"};
    var day = {day: "numeric"};

    // From ICU test files, which in turn was generated from:
    //   Official Umm-al-Qura calendar of SA:
    //   home, http://www.ummulqura.org.sa/default.aspx
    //   converter, http://www.ummulqura.org.sa/Index.aspx
    var dates = [
        [ {year: 2016, month:  1, day: 11}, {year: 1437, month:  4, day: 1} ],
        [ {year: 2016, month:  2, day: 10}, {year: 1437, month:  5, day: 1} ],
        [ {year: 2016, month:  3, day: 10}, {year: 1437, month:  6, day: 1} ],
        [ {year: 2016, month:  4, day:  8}, {year: 1437, month:  7, day: 1} ],
        [ {year: 2016, month:  5, day:  8}, {year: 1437, month:  8, day: 1} ],
        [ {year: 2016, month:  6, day:  6}, {year: 1437, month:  9, day: 1} ],
        [ {year: 2016, month:  7, day:  6}, {year: 1437, month: 10, day: 1} ],
        [ {year: 2016, month:  8, day:  4}, {year: 1437, month: 11, day: 1} ],
        [ {year: 2016, month:  9, day:  2}, {year: 1437, month: 12, day: 1} ],
        [ {year: 2016, month: 10, day:  2}, {year: 1438, month:  1, day: 1} ],
        [ {year: 2016, month: 11, day:  1}, {year: 1438, month:  2, day: 1} ],
        [ {year: 2016, month: 11, day: 30}, {year: 1438, month:  3, day: 1} ],
        [ {year: 2016, month: 12, day: 30}, {year: 1438, month:  4, day: 1} ],
    ];

    for (var [gregorian, ummAlQura] of dates) {
        var date = new Date(Date.UTC(gregorian.year, gregorian.month - 1, gregorian.day));

        // Use parseInt() to remove the trailing era indicator.
        assertEq(parseInt(ummAlQuraDate(year, date), 10), ummAlQura.year);
        assertEq(Number(ummAlQuraDate(month, date)), ummAlQura.month);
        assertEq(Number(ummAlQuraDate(day, date)), ummAlQura.day);
    }
}

testIslamicTbla();
testIslamicRgsa();
testIslamicUmalqura();

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
