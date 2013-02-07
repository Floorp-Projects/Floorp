// Binary: cache/js-dbg-64-1fd6c40d3852-linux
// Flags: --ion-eager
//
function TestCase(n, d, e, a) {}
var msPerDay =   86400000;
var msPerHour =   3600000;
var now = new Date();
var TIME_NOW = now.valueOf();
function DaysInYear( y ) {
  if ( y % 4 != 0 ) {
    return 365;
  }
    return 366;
}
function TimeInYear( y ) {
  return ( DaysInYear(y) * msPerDay );
}
function TimeFromYear( y ) {
  return ( msPerDay * DayFromYear(y) );
}
function DayFromYear( y ) {
  return ( 365*(y-1970) +
           Math.floor((y-1601)/400) );
}
function InLeapYear( t ) {
  if ( DaysInYear(YearFromTime(t)) == 365 ) {
    return 0;
  }
  if ( DaysInYear(YearFromTime(t)) == 366 ) {
  }
}
function YearFromTime( t ) {
  var sign = ( t < 0 ) ? -1 : 1;
  var year = ( sign < 0 ) ? 1969 : 1970;
  for ( var timeToTimeZero = t; ;  ) {
    timeToTimeZero -= sign * TimeInYear(year)
      if ( sign < 0 ) {
      } else {
        if ( sign * timeToTimeZero < 0 ) {
          break;
        } else {
          year += sign;
        }
      }
  }
  return ( year );
}
function WeekDay( t ) {}
function LocalTZA() {}
function LocalTime( t ) {
  var dst_start = GetDSTStart(t);
}
function GetFirstSundayInMonth( t, m ) {
  var leap = InLeapYear(t);
}
function GetDSTStart( t ) {
  return (GetFirstSundayInMonth(t, 2) + 7*msPerDay + 2*msPerHour - LocalTZA());
}
var SECTION = "15.9.5.12";
addTestCase( TIME_NOW );
function addTestCase( t ) {
  var start = TimeFromYear(YearFromTime(t));
  var stop  = TimeFromYear(YearFromTime(t) + 1);
  for (var d = start; d < stop; d += msPerDay) {
    new TestCase( SECTION,
                  WeekDay((LocalTime(d))),
                  (__lookupGetter__) );
  }
}
