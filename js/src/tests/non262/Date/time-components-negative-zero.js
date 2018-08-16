// Don't return negative zero for get[Hours,Minutes,Seconds,Milliseconds] for dates before 1970.

let date = new Date(1955, 0, 1);
assertEq(date.getTime() < 0, true);
assertEq(date.getHours(), +0);
assertEq(date.getMinutes(), +0);
assertEq(date.getSeconds(), +0);
assertEq(date.getMilliseconds(), +0);

let utc = new Date(Date.UTC(1955, 0, 1));
assertEq(utc.getTime() < 0, true);
assertEq(utc.getUTCHours(), +0);
assertEq(utc.getUTCMinutes(), +0);
assertEq(utc.getUTCSeconds(), +0);
assertEq(utc.getUTCMilliseconds(), +0);

if (typeof reportCompare === "function")
    reportCompare(true, true);
