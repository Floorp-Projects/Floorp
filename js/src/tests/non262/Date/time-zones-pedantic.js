// |reftest| skip-if(xulRuntime.OS=="WINNT"||xulRuntime.OS=="Darwin") -- Skip on OS X in addition to Windows

// Contains the tests from "time-zones.js" which fail on OS X. 

// bug 637244
inTimeZone("Asia/Novosibirsk", () => {
    let dt1 = new Date(1984, Month.April, 1, -1);
    assertDateTime(dt1, "Sat Mar 31 1984 23:00:00 GMT+0700 (NOVT)", "+07");

    let dt2 = new Date(1984, Month.April, 1);
    assertDateTime(dt2, "Sun Apr 01 1984 01:00:00 GMT+0800 (NOVST)", "+08");
});

if (typeof reportCompare === "function")
    reportCompare(true, true);
