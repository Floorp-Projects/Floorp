// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
print("Test for correct short-circuiting implementation of Date.set methods");

/**************
 * BEGIN TEST *
 **************/
var global = 0;
var date;

/* Test that methods don't short circuit argument evaluation. */
date = new Date(0).setSeconds(NaN, {valueOf:function(){global = 3}});
assertEq(global, 3);

date = new Date(0).setUTCSeconds(NaN, {valueOf:function(){global = 4}});
assertEq(global, 4);

date = new Date(0).setMinutes(NaN, {valueOf:function(){global = 5}});
assertEq(global, 5);

date = new Date(0).setUTCMinutes(NaN, {valueOf:function(){global = 6}});
assertEq(global, 6);

date = new Date(0).setHours(NaN, {valueOf:function(){global = 7}});
assertEq(global, 7);

date = new Date(0).setUTCHours(NaN, {valueOf:function(){global = 8}});
assertEq(global, 8);

date = new Date(0).setMonth(NaN, {valueOf:function(){global = 11}});
assertEq(global, 11);

date = new Date(0).setUTCMonth(NaN, {valueOf:function(){global = 12}});
assertEq(global, 12);

date = new Date(0).setFullYear(NaN, {valueOf:function(){global = 13}});
assertEq(global, 13);

date = new Date(0).setUTCFullYear(NaN, {valueOf:function(){global = 14}});
assertEq(global, 14);



/* Test that argument evaluation is not short circuited if Date == NaN */
date = new Date(NaN).setMilliseconds({valueOf:function(){global = 15}});
assertEq(global, 15);

date = new Date(NaN).setUTCMilliseconds({valueOf:function(){global = 16}});
assertEq(global, 16);

date = new Date(NaN).setSeconds({valueOf:function(){global = 17}});
assertEq(global, 17);

date = new Date(NaN).setUTCSeconds({valueOf:function(){global = 18}});
assertEq(global, 18);

date = new Date(NaN).setMinutes({valueOf:function(){global = 19}});
assertEq(global, 19);

date = new Date(NaN).setUTCMinutes({valueOf:function(){global = 20}});
assertEq(global, 20);

date = new Date(NaN).setHours({valueOf:function(){global = 21}});
assertEq(global, 21);

date = new Date(NaN).setUTCHours({valueOf:function(){global = 22}});
assertEq(global, 22);

date = new Date(NaN).setDate({valueOf:function(){global = 23}});
assertEq(global, 23);

date = new Date(NaN).setUTCDate({valueOf:function(){global = 24}});
assertEq(global, 24);

date = new Date(NaN).setMonth({valueOf:function(){global = 25}});
assertEq(global, 25);

date = new Date(NaN).setUTCMonth({valueOf:function(){global = 26}});
assertEq(global, 26);

date = new Date(NaN).setFullYear({valueOf:function(){global = 27}});
assertEq(global, 27);

date = new Date(NaN).setUTCFullYear({valueOf:function(){global = 28}});
assertEq(global, 28);


/* Test the combination of the above two. */
date = new Date(NaN).setSeconds(NaN, {valueOf:function(){global = 31}});
assertEq(global, 31);

date = new Date(NaN).setUTCSeconds(NaN, {valueOf:function(){global = 32}});
assertEq(global, 32);

date = new Date(NaN).setMinutes(NaN, {valueOf:function(){global = 33}});
assertEq(global, 33);

date = new Date(NaN).setUTCMinutes(NaN, {valueOf:function(){global = 34}});
assertEq(global, 34);

date = new Date(NaN).setHours(NaN, {valueOf:function(){global = 35}});
assertEq(global, 35);

date = new Date(NaN).setUTCHours(NaN, {valueOf:function(){global = 36}});
assertEq(global, 36);

date = new Date(NaN).setMonth(NaN, {valueOf:function(){global = 39}});
assertEq(global, 39);

date = new Date(NaN).setUTCMonth(NaN, {valueOf:function(){global = 40}});
assertEq(global, 40);

date = new Date(NaN).setFullYear(NaN, {valueOf:function(){global = 41}});
assertEq(global, 41);

date = new Date(NaN).setUTCFullYear(NaN, {valueOf:function(){global = 42}});
assertEq(global, 42);


/*Test two methods evaluation*/
var secondGlobal = 0;

date = new Date(NaN).setFullYear({valueOf:function(){global = 43}}, {valueOf:function(){secondGlobal = 1}});
assertEq(global, 43);
assertEq(secondGlobal, 1);

date = new Date(0).setFullYear(NaN, {valueOf:function(){global = 44}}, {valueOf:function(){secondGlobal = 2}});
assertEq(global, 44);
assertEq(secondGlobal, 2);


/* Test year methods*/
date = new Date(0).setYear({valueOf:function(){global = 45}});
assertEq(global, 45);

date = new Date(NaN).setYear({valueOf:function(){global = 46}});
assertEq(global, 46);


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
