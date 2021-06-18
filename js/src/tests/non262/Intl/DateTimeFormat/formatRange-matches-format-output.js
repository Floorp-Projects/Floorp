// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// formatRange() returns the same output as format() when the date-time difference between
// the start and end date is too small.

// Test case when skeleton can't be retrieved from resolved pattern (bug 1298794).
// - Best-fit pattern for the skeleton "MMMMMdd" is "M月dd日".
// - Best-fit pattern for the skeleton "Mdd" is "M/dd".
//
// So in both cases the skeleton characters in the pattern are "Mdd", which means we can't
// retrieve the original input skeleton by simply inspecting the resolved pattern.
//
// Also see: https://unicode-org.atlassian.net/browse/ICU-13518
{
    let dtf = new Intl.DateTimeFormat("zh", {month: "narrow", day: "2-digit", timeZone: "UTC"});
    assertEq(dtf.formatRange(0, 0), dtf.format(0));
}

// Test that date-/time-style leads to the same output.
// ICU bug: https://unicode-org.atlassian.net/browse/ICU-20710
{
    let dtf = new Intl.DateTimeFormat("en", {dateStyle: "full", timeStyle: "full"});
    assertEq(dtf.formatRange(0, 0), dtf.format(0));
}

// Test that the hourCycle option is correctly processed (test with h24).
// ICU bug: https://unicode-org.atlassian.net/browse/ICU-20707
{
    let dtf = new Intl.DateTimeFormat("en-u-hc-h24", {hour: "2-digit", timeZone:"UTC"});
    assertEq(dtf.formatRange(0, 0), dtf.format(0));
}
{
  let dtf = new Intl.DateTimeFormat("en", {hourCycle: "h24", hour: "2-digit", timeZone:"UTC"});
  assertEq(dtf.formatRange(0, 0), dtf.format(0));
}

// Test that the hourCycle option is correctly processed (test with h11).
// ICU bug: https://unicode-org.atlassian.net/browse/ICU-20707
{
    let dt = 60 * 60 * 1000; // one hour
    let dtf = new Intl.DateTimeFormat("en-u-hc-h11", {hour: "2-digit", timeZone:"UTC"});
    assertEq(dtf.formatRange(dt, dt), dtf.format(dt));
}
{
  let dt = 60 * 60 * 1000; // one hour
  let dtf = new Intl.DateTimeFormat("en", {hourCycle: "h11", hour: "2-digit", timeZone:"UTC"});
  assertEq(dtf.formatRange(dt, dt), dtf.format(dt));
}

// Test that non-default calendars work correctly.
// ICU bug: https://unicode-org.atlassian.net/browse/ICU-20706
{
    let dtf = new Intl.DateTimeFormat("en-001-u-ca-hebrew");
    assertEq(dtf.formatRange(0, 0), dtf.format(0));
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
