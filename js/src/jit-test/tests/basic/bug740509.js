function TestCase(n, d, e, a)
TestCase.prototype.dump = function () {
}
var lfcode = new Array();
lfcode.push("2");
lfcode.push("var lfcode = new Array();\
lfcode.push(\"gczeal(4,1);\");\
while (true) {\
        var file = lfcode.shift(); if (file == ((0Xa  )  . shift      )) { break; }\
        eval(file);\
}\
");
lfcode.push("function testJSON(str, expectSyntaxError)\
");
lfcode.push("1");
lfcode.push("Number.prototype.toString = function() { return 3; };\
assertEq(JSON.stringify({ 3: 3, 4: 4 }, [(this  . abstract       )]),\
         '{\"3\":3}');\
");
lfcode.push("var HoursPerDay =  24;\
var MinutesPerHour = 60;\
var SecondsPerMinute = 60;\
var msPerSecond =  1000;\
var msPerMinute =  60000;\
var TZ_ADJUST  = TZ_DIFF * msPerHour;\
var PST_DIFF = TZ_DIFF - TZ_PST;\
var PST_ADJUST = TZ_PST * msPerHour;\
var TIME_0000  = (function ()\
  {\
var TIME_1970  = 0;\
var TIME_1900  = -2208988800000;\
var UTC_FEB_29_2000 = TIME_2000 + 31*msPerDay + 28*msPerDay;\
var UTC_JAN_1_2005 = TIME_2000 + TimeInYear(2000) + TimeInYear(2001) +\
  TimeInYear(2002) + TimeInYear(2003) + TimeInYear(2004);\
var TIME_NOW = now.valueOf();\
function getTimeZoneDiff()\
{\
  return -((new Date(2000, 1, 1)).getTimezoneOffset())/60;\
function adjustResultArray(ResultArray, msMode)\
    ResultArray[UTC_HOURS] = HourFromTime(t);\
    ResultArray[UTC_DATE] = DateFromTime(t);\
    ResultArray[UTC_MONTH] = MonthFromTime(t);\
    ResultArray[UTC_YEAR] = YearFromTime(t);\
function DaysInYear( y ) {\
    return \"ERROR: DaysInYear(\" + y + \") case not covered\";\
function DayNumber( t ) {\
function TimeWithinDay( t ) {\
function YearNumber( t ) {\
function TimeFromYear( y ) {\
function InLeapYear( t ) {\
    return \"ERROR:  InLeapYear(\"+ t + \") case not covered\";\
  for ( var timeToTimeZero = t; ;  ) {\
    return \"ERROR: MonthFromTime(\"+t+\") not known\";\
function DayWithinYear( t ) {\
  return( Day(t) - DayFromYear(YearFromTime(t)));\
");
lfcode.push("this.__proto__ = []; \
let ( _ = this ) Boolean   (\"({ set x([, b, c]) { } })\");\
");
while (true) {
	var file = lfcode.shift(); if (file == undefined) { break; }
	if (file == "evaluate") {
	} else {
                loadFile(file);
	}
}
function loadFile(lfVarx) {
	try {
		if (lfVarx.substr(-3) == ".js") {
		} else if (!isNaN(lfVarx)) {
			lfRunTypeId = parseInt(lfVarx);
		} else {
			switch (lfRunTypeId) {
				case 1: eval(lfVarx); break;
				case 2: new Function(lfVarx)(); break;
			}
		}
	} catch (lfVare) {	}
}
