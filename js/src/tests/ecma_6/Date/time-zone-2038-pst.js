// |reftest| skip-if(!xulRuntime.shell)

// Note: The default time zone is set to PST8PDT for all jstests (when run in the shell).

assertEq(/^(PST|PDT)$/.test(getTimeZone()), true);

const Month = {
    January: 0,
    February: 1,
    March: 2,
    April: 3,
    May: 4,
    June: 5,
    July: 6,
    August: 7,
    September: 8,
    October: 9,
    November: 10,
    December: 11,
};

// U.S. daylight saving rules changed in 2007, excerpt from tzdata's
// northamerica file:
// NAME  FROM  TO    IN   ON       AT    SAVE  LETTER/S   
// US    1967  2006  Oct  lastSun  2:00  0     S
// US    1967  1973  Apr  lastSun  2:00  1:00  D
// US    1974  only  Jan  6        2:00  1:00  D
// US    1975  only  Feb  23       2:00  1:00  D
// US    1976  1986  Apr  lastSun  2:00  1:00  D
// US    1987  2006  Apr  Sun>=1   2:00  1:00  D
// US    2007  max   Mar  Sun>=8   2:00  1:00  D
// US    2007  max   Nov  Sun>=1   2:00  0     S


// When 2040 is mapped to 1984, the old U.S. rules are applied, i.e. DST isn't
// yet observed on March 31. If mapped to 2012, the new U.S. rules are applied
// and DST is already observed, which is the expected behaviour.
// A similar effect is visible in November.
// NOTE: This test expects that 2012 and 2040 use the same DST rules. If this
//       ever changes, the test needs to be updated accordingly.
{
    let dt1 = new Date(2040, Month.March, 31);
    assertEq(dt1.toString(), "Sat Mar 31 2040 00:00:00 GMT-0700 (PDT)");

    let dt2 = new Date(2040, Month.November, 1);
    assertEq(dt2.toString(), "Thu Nov 01 2040 00:00:00 GMT-0700 (PDT)");
}

// 2038 is mapped to 2027 instead of 1971.
{
    let dt1 = new Date(2038, Month.March, 31);
    assertEq(dt1.toString(), "Wed Mar 31 2038 00:00:00 GMT-0700 (PDT)");

    let dt2 = new Date(2038, Month.November, 1);
    assertEq(dt2.toString(), "Mon Nov 01 2038 00:00:00 GMT-0700 (PDT)");
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
