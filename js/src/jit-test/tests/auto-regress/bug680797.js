// |jit-test| slow; error:InternalError

// Binary: cache/js-dbg-64-a2bbe9c999b4-linux
// Flags: -m -n
//
gczeal(2);
function Day( t ) {}
function YearFromTime( t ) {}
function HourFromTime( t ) {}
function MakeTime( hour, min, sec, ms ) {}
function MakeDay( year, month, date ) {}
function MakeDate( day, time ) {}
function TimeClip( t ) {
  return ToInteger( t );
}
function ToInteger( t ) {
  sign = 1
  return sign * Math.floor( Math.abs( t ) );
}
addNewTestCase( 28800000, 23, 59, 999,0, "TDATE = new Date(28800000);(TDATE).setHours(23,59,999);TDATE" );
function addNewTestCase( time, hours, min, sec, ms, DateString) {
  UTCDateFromTime( SetHours( time, hours, min, sec, ms ))
}
function MyDate() this.seconds=0;function UTCDateFromTime(t) {
  MyDateFromTime(t)
}
function MyDateFromTime( t ) {
  d = new MyDate
  d.year=YearFromTime(t)
  d.month=
  d.date=
  d.hours=HourFromTime(t)
  d.minutes=
  d.time=MakeTime( d.hours, d, d.seconds, d )
  d=TimeClip( MakeDate( MakeDay( d.year, d.month, ( ( MyDateFromTime(t) ) , null ) ), d.time ) )
}
function SetHours( t, hour, min, sec, ms ) {
  TIME =
  HOUR =
  MIN =  min == 0 ? TIME :
  SEC  = sec ==  0 ? addNewTestCaseSecFromTime : Number
  var MS   = ms == void 0 ? TIME  : ms;
  var RESULT6 = ( HOUR, MS );
  var UTC_TIME = MakeDate(Day(TIME), RESULT6);
  return TimeClip(UTC_TIME);
}
