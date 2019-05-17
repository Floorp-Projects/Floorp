/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommonn.org/licenses/publicdomain/
 */

/*
 * For the sake of cross compatibility with other implementations we
 * follow the W3C "NOTE-datetime" specification when parsing dates of
 * the form YYYY-MM-DDTHH:MM:SS save for a few exceptions: months, days, hours
 * minutes, and seconds may be either one _or_ two digits long, and the 'T'
 * preceding the time part may be replaced with a space. So, a string like
 * "1997-3-8 1:1:1" will parse successfully. See bug: 1203298
 */

/**************
 * BEGIN TEST *
 **************/

assertEq(new Date("1997-03-08 1:1:1.01").getTime(),
         new Date("1997-03-08T01:01:01.01").getTime());
assertEq(new Date("1997-03-08 11:19:20").getTime(),
         new Date("1997-03-08T11:19:20").getTime());
assertEq(new Date("1997-3-08 11:19:20").getTime(),
         new Date("1997-03-08T11:19:20").getTime());
assertEq(new Date("1997-3-8 11:19:20").getTime(),
         new Date("1997-03-08T11:19:20").getTime());
assertEq(new Date("+001997-3-8 11:19:20").getTime(),
         new Date("1997-03-08T11:19:20").getTime());
assertEq(new Date("+001997-03-8 11:19:20").getTime(),
         new Date("1997-03-08T11:19:20").getTime());
assertEq(new Date("1997-03-08 11:19").getTime(),
         new Date("1997-03-08T11:19").getTime());
assertEq(new Date("1997-03-08 1:19").getTime(),
         new Date("1997-03-08T01:19").getTime());
assertEq(new Date("1997-03-08 1:1").getTime(),
         new Date("1997-03-08T01:01").getTime());
assertEq(new Date("1997-03-08 1:1:01").getTime(),
         new Date("1997-03-08T01:01:01").getTime());
assertEq(new Date("1997-03-08 1:1:1").getTime(),
         new Date("1997-03-08T01:01:01").getTime());
assertEq(new Date("1997-03-08 11").getTime(),
         new Date("1997-03-08T11").getTime()); // Date(NaN)
assertEq(new Date("1997-03-08").getTime(),
         new Date("1997-03-08").getTime());
assertEq(new Date("1997-03-8").getTime(),
         new Date("1997-03-08").getTime());
assertEq(new Date("1997-3-8").getTime(),
         new Date("1997-03-08").getTime());
assertEq(new Date("1997-3-8 ").getTime(),
         new Date("1997-03-08T").getTime()); // Date(NaN)
assertEq(new Date("1997-3-8T11:19:20").getTime(),
         new Date(NaN).getTime());
assertEq(new Date("1997-03-8T11:19:20").getTime(),
         new Date(NaN).getTime());
assertEq(new Date("+001997-3-8T11:19:20").getTime(),
         new Date(NaN).getTime());
assertEq(new Date("+001997-3-08T11:19:20").getTime(),
         new Date(NaN).getTime());
assertEq(new Date("1997-3-8 :00:01").getTime(),
         new Date(NaN).getTime());
assertEq(new Date("1997-3-8 01::01").getTime(),
         new Date(NaN).getTime());
assertEq(new Date("1997-03-08T1:19").getTime(),
         new Date(NaN).getTime());         
assertEq(new Date("1997-03-08T1:1").getTime(),
         new Date(NaN).getTime());
assertEq(new Date("1997-03-08T1:1:01").getTime(),
         new Date(NaN).getTime());
assertEq(new Date("1997-03-08T1:1:1").getTime(),
         new Date(NaN).getTime());

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
