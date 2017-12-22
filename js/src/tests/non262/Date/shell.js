
var BUGNUMBER;
var summary;


/*
 * Date functions used by tests in Date suite
 *
 */
var msPerDay =   86400000;
var msPerHour =   3600000; // 1000 * 60 * 60
var TZ_DIFF = getTimeZoneDiff();  // offset of tester's timezone from UTC
var TZ_ADJUST = TZ_DIFF * msPerHour;
var TIME_2000  = 946684800000;
var TIME_1900  = -2208988800000;
var UTC_29_FEB_2000 = TIME_2000 + 31*msPerDay + 28*msPerDay;
var UTC_1_JAN_2005 = TIME_2000 + TimeInYear(2000) + TimeInYear(2001) +
  TimeInYear(2002) + TimeInYear(2003) + TimeInYear(2004);
var now = new Date();
var TIME_NOW = now.valueOf();  //valueOf() is to accurate to the millisecond
                               //Date.parse() is accurate only to the second

/*
 * Originally, the test suite used a hard-coded value TZ_DIFF = -8.
 * But that was only valid for testers in the Pacific Standard Time Zone!
 * We calculate the proper number dynamically for any tester. We just
 * have to be careful not to use a date subject to Daylight Savings Time...
 */
function getTimeZoneDiff()
{
  return -((new Date(2000, 1, 1)).getTimezoneOffset())/60;
}

function DaysInYear( y ) {
  if ( y % 4 != 0 ) {
    return 365;
  }
  if ( (y % 4 == 0) && (y % 100 != 0) ) {
    return 366;
  }
  if ( (y % 100 == 0) && (y % 400 != 0) ) {
    return 365;
  }
  if ( (y % 400 == 0) ){
    return 366;
  } else {
    return "ERROR: DaysInYear(" + y + ") case not covered";
  }
}

function TimeInYear( y ) {
  return ( DaysInYear(y) * msPerDay );
}

function runDSTOffsetCachingTestsFraction(part, parts)
{
  BUGNUMBER = 563938;
  summary = 'Cache DST offsets to improve SunSpider score';

  print(BUGNUMBER + ": " + summary);

  var MAX_UNIX_TIMET = 2145859200;
  var RANGE_EXPANSION_AMOUNT = 30 * 24 * 60 * 60;

  /**
   * Computes the time zone offset in minutes at the given timestamp.
   */
  function tzOffsetFromUnixTimestamp(timestamp)
  {
    var d = new Date(NaN);
    d.setTime(timestamp); // local slot = NaN, UTC slot = timestamp
    return d.getTimezoneOffset(); // get UTC, calculate local => diff in minutes
  }

  /**
   * Clear the DST offset cache, leaving it initialized to include a timestamp
   * completely unlike the provided one (i.e. one very, very far away in time
   * from it).  Thus an immediately following lookup for the provided timestamp
   * will cache-miss and compute a clean value.
   */
  function clearDSTOffsetCache(undesiredTimestamp)
  {
    var opposite = (undesiredTimestamp + MAX_UNIX_TIMET / 2) % MAX_UNIX_TIMET;

    // Generic purge to known, but not necessarily desired, state
    tzOffsetFromUnixTimestamp(0);
    tzOffsetFromUnixTimestamp(MAX_UNIX_TIMET);

    // Purge to desired state.  Cycle 2x in case opposite or undesiredTimestamp
    // is close to 0 or MAX_UNIX_TIMET.
    tzOffsetFromUnixTimestamp(opposite);
    tzOffsetFromUnixTimestamp(undesiredTimestamp);
    tzOffsetFromUnixTimestamp(opposite);
    tzOffsetFromUnixTimestamp(undesiredTimestamp);
  }

  function computeCanonicalTZOffset(timestamp)
  {
    clearDSTOffsetCache(timestamp);
    return tzOffsetFromUnixTimestamp(timestamp);
  }

  var TEST_TIMESTAMPS_SECONDS =
    [
     // Special-ish timestamps
     0,
     RANGE_EXPANSION_AMOUNT,
     MAX_UNIX_TIMET,
    ];

  var ONE_DAY = 24 * 60 * 60;
  var EIGHTY_THREE_HOURS = 83 * 60 * 60;
  var NINETY_EIGHT_HOURS = 98 * 60 * 60;
  function nextIncrement(i)
  {
    return i === EIGHTY_THREE_HOURS ? NINETY_EIGHT_HOURS : EIGHTY_THREE_HOURS;
  }

  // Now add a long sequence of non-special timestamps, from a fixed range, that
  // overlaps a DST change by "a bit" on each side.  67 days should be enough
  // displacement that we can occasionally exercise the implementation's
  // thirty-day expansion and the DST-offset-change logic.  Use two different
  // increments just to be safe and catch something a single increment might not.
  var DST_CHANGE_DATE = 1268553600; // March 14, 2010
  for (var t = DST_CHANGE_DATE - 67 * ONE_DAY,
           i = nextIncrement(NINETY_EIGHT_HOURS),
           end = DST_CHANGE_DATE + 67 * ONE_DAY;
       t < end;
       i = nextIncrement(i), t += i)
  {
    TEST_TIMESTAMPS_SECONDS.push(t);
  }

  var TEST_TIMESTAMPS =
    TEST_TIMESTAMPS_SECONDS.map(function(v) { return v * 1000; });

  /**************
   * BEGIN TEST *
   **************/

  // Compute the correct time zone offsets for all timestamps to be tested.
  var CORRECT_TZOFFSETS = TEST_TIMESTAMPS.map(computeCanonicalTZOffset);

  // Intentionally and knowingly invoking every single logic path in the cache
  // isn't easy for a human to get right (and know he's gotten it right), so
  // let's do it the easy way: exhaustively try all possible four-date sequences
  // selecting from our array of possible timestamps.

  var sz = TEST_TIMESTAMPS.length;
  var start = Math.floor((part - 1) / parts * sz);
  var end = Math.floor(part / parts * sz);

  print("Exhaustively testing timestamps " +
        "[" + start + ", " + end + ") of " + sz + "...");

  try
  {
    for (var i = start; i < end; i++)
    {
      print("Testing timestamp " + i + "...");

      var t1 = TEST_TIMESTAMPS[i];
      for (var j = 0; j < sz; j++)
      {
        var t2 = TEST_TIMESTAMPS[j];
        for (var k = 0; k < sz; k++)
        {
          var t3 = TEST_TIMESTAMPS[k];
          for (var w = 0; w < sz; w++)
          {
            var t4 = TEST_TIMESTAMPS[w];

            clearDSTOffsetCache(t1);

            var tzo1 = tzOffsetFromUnixTimestamp(t1);
            var tzo2 = tzOffsetFromUnixTimestamp(t2);
            var tzo3 = tzOffsetFromUnixTimestamp(t3);
            var tzo4 = tzOffsetFromUnixTimestamp(t4);

            assertEq(tzo1, CORRECT_TZOFFSETS[i]);
            assertEq(tzo2, CORRECT_TZOFFSETS[j]);
            assertEq(tzo3, CORRECT_TZOFFSETS[k]);
            assertEq(tzo4, CORRECT_TZOFFSETS[w]);
          }
        }
      }
    }
  }
  catch (e)
  {
    assertEq(true, false,
             "Error when testing with timestamps " +
             i + ", " + j + ", " + k + ", " + w +
             " (" + t1 + ", " + t2 + ", " + t3 + ", " + t4 + ")!");
  }

  reportCompare(true, true);
  print("All tests passed!");
}
