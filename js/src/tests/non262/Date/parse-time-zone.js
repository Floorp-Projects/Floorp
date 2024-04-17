// |reftest| skip-if(winWidget) -- Windows doesn't accept IANA names for the TZ env variable

// bug 1676708
inTimeZone("Europe/London", () => {
  let dt = new Date("Wed Nov 11 2020 19:18:50 GMT+0010");
  assertEq(dt.getTime(), new Date(2020, Month.November, 11, 19, 08, 50).getTime());
});

if (typeof reportCompare === "function")
  reportCompare(true, true);
