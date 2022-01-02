// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Test that the interval formatting uses the original skeleton, not the skeleton
// derived from the resolved pattern.
{
  let dtf = new Intl.DateTimeFormat("zh-Hans-CN", {
    formatMatcher: "best fit",
    month: "narrow",
    day: "2-digit",
    timeZone: "UTC"
  });

  assertEq(dtf.format(Date.UTC(2016, 7, 1)), "8月01日");
  assertEq(dtf.formatRange(Date.UTC(2016, 7, 1), Date.UTC(2016, 7, 2)), "8月1日至2日");
}

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
