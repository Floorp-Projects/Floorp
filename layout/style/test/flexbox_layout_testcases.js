/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et: */

/*
 * This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/.
 */

/**
 * For the purposes of this test, flex items are specified as a hash with a
 * hash-entry for each CSS property that is to be set.  In these per-property
 * entries, the key is the property-name, and the value can be either of the
 * following:
 *  (a) the property's specified value (which indicates that we don't need to
 *      bother checking the computed value of this particular property)
 *  ...OR...
 *  (b) an array with 2-3 entries...
 *        [specifiedValue, expectedComputedValue (, epsilon) ]
 *      ...which indicates that the property's computed value should be
 *      checked.  The array's first entry (for the specified value) may be
 *      null; this means that no value should be explicitly specified for this
 *      property. The second entry is the property's expected computed
 *      value. The third (optional) entry is an epsilon value, which allows for
 *      fuzzy equality when testing the computed value.
 *
 * To allow these testcases to be re-used in both horizontal and vertical
 * flex containers, we specify "width"/"min-width"/etc. using the aliases
 * "_main-size", "_min-main-size", etc.  The test code can map these
 * placeholder names to their corresponding property-names using the maps
 * defined below -- gRowPropertyMapping, gColumnPropertyMapping, etc.
 *
 * If the testcase needs to customize its flex container at all (e.g. by
 * specifying a custom container-size), it can do so by including a hash
 * called "container_properties", with propertyName:propertyValue mappings.
 * (This hash can use aliased property-names like "_main-size" as well.)
 */

// The standard main-size we'll use for our flex container when setting up
// the testcases defined below:
var gDefaultFlexContainerSize = "200px";

// Left-to-right versions of placeholder property-names used in
// testcases below:
var gRowPropertyMapping =
{
  "_main-size":               "width",
  "_min-main-size":           "min-width",
  "_max-main-size":           "max-width",
  "_border-main-start-width": "border-left-width",
  "_border-main-end-width":   "border-right-width",
  "_padding-main-start":      "padding-left",
  "_padding-main-end":        "padding-right",
  "_margin-main-start":       "margin-left",
  "_margin-main-end":         "margin-right"
};

// Right-to-left versions of placeholder property-names used in
// testcases below:
var gRowReversePropertyMapping =
{
  "_main-size":               "width",
  "_min-main-size":           "min-width",
  "_max-main-size":           "max-width",
  "_border-main-start-width": "border-right-width",
  "_border-main-end-width":   "border-left-width",
  "_padding-main-start":      "padding-right",
  "_padding-main-end":        "padding-left",
  "_margin-main-start":       "margin-right",
  "_margin-main-end":         "margin-left"
};

// Top-to-bottom versions of placeholder property-names used in
// testcases below:
var gColumnPropertyMapping =
{
  "_main-size":               "height",
  "_min-main-size":           "min-height",
  "_max-main-size":           "max-height",
  "_border-main-start-width": "border-top-width",
  "_border-main-end-width":   "border-bottom-width",
  "_padding-main-start":      "padding-top",
  "_padding-main-end":        "padding-bottom",
  "_margin-main-start":       "margin-top",
  "_margin-main-end":         "margin-bottom"
};

// Bottom-to-top versions of placeholder property-names used in
// testcases below:
var gColumnReversePropertyMapping =
{
  "_main-size":               "height",
  "_min-main-size":           "min-height",
  "_max-main-size":           "max-height",
  "_border-main-start-width": "border-bottom-width",
  "_border-main-end-width":   "border-top-width",
  "_padding-main-start":      "padding-bottom",
  "_padding-main-end":        "padding-top",
  "_margin-main-start":       "margin-bottom",
  "_margin-main-end":         "margin-top"
};

// The list of actual testcase definitions:
var gFlexboxTestcases =
[
 // No flex properties specified --> should just use 'width' for sizing
 {
   items:
     [
       { "_main-size": [ "40px", "40px" ] },
       { "_main-size": [ "65px", "65px" ] },
     ]
 },
 // flex-basis is specified:
 {
   items:
     [
       { "flex-basis": "50px",
         "_main-size": [ null,  "50px" ]
       },
       {
         "flex-basis": "20px",
         "_main-size": [ null, "20px" ]
       },
     ]
 },
 // flex-basis is *large* -- sum of flex-basis values is > flex container size:
 // (w/ 0 flex-shrink so we don't shrink):
 {
   items:
     [
       {
         "flex": "0 0 150px",
         "_main-size": [ null, "150px" ]
       },
       {
         "flex": "0 0 90px",
         "_main-size": [ null, "90px" ]
       },
     ]
 },
 // flex-basis is *large* -- each flex-basis value is > flex container size:
 // (w/ 0 flex-shrink so we don't shrink):
 {
   items:
     [
       {
         "flex": "0 0 250px",
         "_main-size": [ null, "250px" ]
       },
       {
         "flex": "0 0 400px",
         "_main-size": [ null, "400px" ]
       },
     ]
 },
 // flex-basis has percentage value:
 {
   items:
     [
       {
         "flex-basis": "30%",
         "_main-size": [ null, "60px" ]
       },
       {
         "flex-basis": "45%",
         "_main-size": [ null, "90px" ]
       },
     ]
 },
 // flex-basis has calc(percentage) value:
 {
   items:
     [
       {
         "flex-basis": "calc(20%)",
         "_main-size": [ null, "40px" ]
       },
       {
         "flex-basis": "calc(80%)",
         "_main-size": [ null, "160px" ]
       },
     ]
 },
 // flex-basis has calc(percentage +/- length) value:
 {
   items:
     [
       {
         "flex-basis": "calc(10px + 20%)",
         "_main-size": [ null, "50px" ]
       },
       {
         "flex-basis": "calc(60% - 1px)",
         "_main-size": [ null, "119px" ]
       },
     ]
 },
 // flex-grow is specified:
 {
   items:
     [
       {
         "flex": "1",
         "_main-size": [ null,  "60px" ]
       },
       {
         "flex": "2",
         "_main-size": [ null, "120px" ]
       },
       {
         "flex": "0 20px",
         "_main-size": [ null, "20px" ]
       }
     ]
 },
 // Same ratio as prev. testcase; making sure we handle float inaccuracy
 {
   items:
     [
       {
         "flex": "100000",
         "_main-size": [ null,  "60px" ]
       },
       {
         "flex": "200000",
         "_main-size": [ null, "120px" ]
       },
       {
         "flex": "0.000001 20px",
         "_main-size": [ null,  "20px" ]
       }
     ]
 },
 // Same ratio as prev. testcase, but with items cycled and w/
 // "flex: none" & explicit size instead of "flex: 0 20px"
 {
   items:
     [
       {
         "flex": "none",
         "_main-size": [ "20px", "20px" ]
       },
       {
         "flex": "1",
         "_main-size": [ null,   "60px" ]
       },
       {
         "flex": "2",
         "_main-size": [ null,  "120px" ]
       }
     ]
 },

 // ...and now with flex-grow:[huge] to be sure we handle infinite float values
 // gracefully.
 {
   items:
     [
       {
         "flex": "9999999999999999999999999999999999999999999999999999999",
         "_main-size": [ null,  "200px" ]
       },
     ]
 },
 {
   items:
     [
       {
         "flex": "9999999999999999999999999999999999999999999999999999999",
         "_main-size": [ null,  "50px" ]
       },
       {
         "flex": "9999999999999999999999999999999999999999999999999999999",
         "_main-size": [ null,  "50px" ]
       },
       {
         "flex": "9999999999999999999999999999999999999999999999999999999",
         "_main-size": [ null,  "50px" ]
       },
       {
         "flex": "9999999999999999999999999999999999999999999999999999999",
         "_main-size": [ null,  "50px" ]
       },
     ]
 },
 {
   items:
     [
       {
         "flex": "99999999999999999999999999999999999",
         "_main-size": [ null,  "50px" ]
       },
       {
         "flex": "99999999999999999999999999999999999",
         "_main-size": [ null,  "50px" ]
       },
       {
         "flex": "99999999999999999999999999999999999",
         "_main-size": [ null,  "50px" ]
       },
       {
         "flex": "99999999999999999999999999999999999",
         "_main-size": [ null,  "50px" ]
       },
     ]
 },

 // And now, some testcases to check that we handle float accumulation error
 // gracefully.

 // First, a testcase with just a custom-sized huge container, to be sure we'll
 // be able to handle content on that scale, in the subsequent more-complex
 // testcases:
 {
   container_properties:
   {
     "_main-size": "9000000px"
   },
   items:
     [
       {
         "flex": "1",
         "_main-size": [ null,  "9000000px" ]
       },
     ]
 },
 // ...and now with two flex items dividing up that container's huge size:
 {
   container_properties:
   {
     "_main-size": "9000000px"
   },
   items:
     [
       {
         "flex": "2",
         "_main-size": [ null,  "6000000px" ]
       },
       {
         "flex": "1",
         "_main-size": [ null,  "3000000px" ]
       },
     ]
 },

 // OK, now to actually test accumulation error. Below, we have six flex items
 // splitting up the container's size, with huge differences between flex
 // weights.  For simplicity, I've set up the weights so that they sum exactly
 // to the container's size in px. So 1 unit of flex *should* get you 1px.
 //
 // NOTE: The expected computed "_main-size" values for the flex items below
 // appear to add up to more than their container's size, which would suggest
 // that they overflow their container unnecessarily. But they don't actually
 // overflow -- this discrepancy is simply because Gecko's code for reporting
 // computed-sizes rounds to 6 significant figures (in particular, the method
 // (nsTSubstring_CharT::AppendFloat() does this).  Internally, in app-units,
 // the child frames' main-sizes add up exactly to the container's main-size,
 // as you'd hope & expect.
 {
   container_properties:
   {
     "_main-size": "9000000px"
   },
   items:
     [
       {
         "flex": "3000000",
         "_main-size": [ null,  "3000000px" ]
       },
       {
         "flex": "1",
         "_main-size": [ null,  "1px" ]
       },
       {
         "flex": "1",
         "_main-size": [ null,  "1px" ]
       },
       {
         "flex": "2999999",
         // NOTE: Expected value is off slightly, from float error when
         // resolving flexible lengths & when generating computed value string:
         "_main-size": [ null,  "3000000px" ]
       },
       {
         "flex": "2999998",
         // NOTE: Expected value is off slightly, from float error when
         // resolving flexible lengths & when generating computed value string:
         "_main-size": [ null,  "3000000px" ]
       },
       {
         "flex": "1",
         "_main-size": [ null,  "1px", 0.2 ]
       },
     ]
 },
 // Same flex items as previous testcase, but now reordered such that the items
 // with tiny flex weights are all listed last:
 {
   container_properties:
   {
     "_main-size": "9000000px"
   },
   items:
     [
       {
         "flex": "3000000",
         "_main-size": [ null,  "3000000px" ]
       },
       {
         "flex": "2999999",
         // NOTE: Expected value is off slightly, from float error when
         // resolving flexible lengths & when generating computed value string:
         "_main-size": [ null,  "3000000px" ]
       },
       {
         "flex": "2999998",
         // NOTE: Expected value is off slightly, from float error when
         // resolving flexible lengths & when generating computed value string:
         "_main-size": [ null,  "3000000px" ]
       },
       {
         "flex": "1",
         "_main-size": [ null,  "1px", 0.2 ]
       },
       {
         "flex": "1",
         "_main-size": [ null,  "1px", 0.2 ]
       },
       {
         "flex": "1",
         "_main-size": [ null,  "1px", 0.2 ]
       },
     ]
 },
 // Same flex items as previous testcase, but now reordered such that the items
 // with tiny flex weights are all listed first:
 {
   container_properties:
   {
     "_main-size": "9000000px"
   },
   items:
     [
       {
         "flex": "1",
         // NOTE: Expected value is off slightly, from float error when
         // resolving flexible lengths:
         "_main-size": [ null,  "1px", 0.2 ]
       },
       {
         "flex": "1",
         // NOTE: Expected value is off slightly, from float error when
         // resolving flexible lengths:
         "_main-size": [ null,  "1px", 0.2 ]
       },
       {
         "flex": "1",
         // NOTE: Expected value is off slightly, from float error when
         // resolving flexible lengths:
         "_main-size": [ null,  "1px", 0.2 ]
       },
       {
         "flex": "3000000",
         "_main-size": [ null,  "3000000px" ]
       },
       {
         "flex": "2999999",
         // NOTE: Expected value is off slightly, from float error when
         // resolving flexible lengths & when generating computed value string:
         "_main-size": [ null,  "3000000px" ]
       },
       {
         "flex": "2999998",
         // NOTE: Expected value is off slightly, from float error when
         // resolving flexible lengths & when generating computed value string:
         "_main-size": [ null,  "3000000px" ]
       },
     ]
 },

 // Trying "flex: auto" (== "1 1 auto") w/ a mix of flex-grow/flex-basis values
 {
   items:
     [
       {
         "flex": "auto",
         "_main-size": [ null, "45px" ]
       },
       {
         "flex": "2",
         "_main-size": [ null, "90px" ]
       },
       {
         "flex": "20px 1 0",
         "_main-size": [ null, "65px" ]
       }
     ]
 },
 // Same as previous, but with items cycled & different syntax
 {
   items:
     [
       {
         "flex": "20px",
         "_main-size": [ null, "65px" ]
       },
       {
         "flex": "1",
         "_main-size": [ null, "45px" ]
       },
       {
         "flex": "2",
         "_main-size": [ null, "90px" ]
       }
     ]
 },
 {
   items:
     [
       {
         "flex": "2",
         "_main-size": [ null,  "100px" ],
         "border": "0px dashed",
         "_border-main-start-width": [ "5px",  "5px" ],
         "_border-main-end-width": [ "15px", "15px" ],
         "_margin-main-start": [ "22px", "22px" ],
         "_margin-main-end": [ "8px", "8px" ]
       },
       {
         "flex": "1",
         "_main-size": [ null,  "50px" ],
         "_margin-main-start": [ "auto", "0px" ],
         "_padding-main-end": [ "auto", "0px" ],
       }
     ]
 },
 // Test negative flexibility:

 // Basic testcase: just 1 item (relying on initial "flex-shrink: 1") --
 // should shrink to container size.
 {
   items:
     [
       { "_main-size": [ "400px",  "200px" ] },
     ],
 },
 // ...and now with a "flex" specification and a different flex-shrink value:
 {
   items:
     [
       {
         "flex": "4 2 250px",
         "_main-size": [ null,  "200px" ]
       },
     ],
 },
 // ...and now with multiple items, which all shrink proportionally (by 50%)
 // to fit to the container, since they have the same (initial) flex-shrink val
 {
   items:
     [
       { "_main-size": [ "80px",   "40px" ] },
       { "_main-size": [ "40px",   "20px" ] },
       { "_main-size": [ "30px",   "15px" ] },
       { "_main-size": [ "250px", "125px" ] },
     ]
 },
 // ...and now with positive flexibility specified. (should have no effect, so
 // everything still shrinks by the same proportion, since the flex-shrink
 // values are all the same).
 {
   items:
     [
       {
         "flex": "4 3 100px",
         "_main-size": [ null,  "80px" ]
       },
       {
         "flex": "5 3 50px",
         "_main-size": [ null,  "40px" ]
       },
       {
         "flex": "0 3 100px",
         "_main-size": [ null, "80px" ]
       }
     ]
 },
 // ...and now with *different* flex-shrink values:
 {
   items:
     [
       {
         "flex": "4 2 50px",
         "_main-size": [ null,  "30px" ]
       },
       {
         "flex": "5 3 50px",
         "_main-size": [ null,  "20px" ]
       },
       {
         "flex": "0 0 150px",
         "_main-size": [ null, "150px" ]
       }
     ]
 },
 // Same ratio as prev. testcase; making sure we handle float inaccuracy
 {
   items:
     [
       {
         "flex": "4 20000000 50px",
         "_main-size": [ null,  "30px" ]
       },
       {
         "flex": "5 30000000 50px",
         "_main-size": [ null,  "20px" ]
       },
       {
         "flex": "0 0.0000001 150px",
         "_main-size": [ null, "150px" ]
       }
     ]
 },
 // Another "different flex-shrink values" testcase:
 {
   items:
     [
       {
         "flex": "4 2 115px",
         "_main-size": [ null,  "69px" ]
       },
       {
         "flex": "5 1 150px",
         "_main-size": [ null,  "120px" ]
       },
       {
         "flex": "1 4 30px",
         "_main-size": [ null,  "6px" ]
       },
       {
         "flex": "1 0 5px",
         "_main-size": [ null, "5px" ]
       },
     ]
 },

 // ...and now with min-size (clamping the effects of flex-shrink on one item):
 {
   items:
     [
       {
         "flex": "4 5 75px",
         "_min-main-size": "50px",
         "_main-size": [ null,  "50px" ],
       },
       {
         "flex": "5 5 100px",
         "_main-size": [ null,  "62.5px" ]
       },
       {
         "flex": "0 4 125px",
         "_main-size": [ null, "87.5px" ]
       }
     ]
 },

 // Test a min-size that's much larger than initial preferred size, but small
 // enough that our flexed size pushes us over it:
 {
   items:
     [
       {
         "flex": "auto",
         "_min-main-size": "110px",
         "_main-size": [ "50px",  "125px" ]
       },
       {
         "flex": "auto",
         "_main-size": [ null, "75px" ]
       }
     ]
 },

 // Test a min-size that's much larger than initial preferred size, and is
 // even larger than our positively-flexed size, so that we have to increase it
 // (as a 'min violation') after we've flexed.
 {
   items:
     [
       {
         "flex": "auto",
         "_min-main-size": "150px",
         "_main-size": [ "50px",  "150px" ]
       },
       {
         "flex": "auto",
         "_main-size": [ null, "50px" ]
       }
     ]
 },

 // Test min-size on multiple items simultaneously:
 {
   items:
     [
       {
         "flex": "auto",
         "_min-main-size": "20px",
         "_main-size": [ null,  "20px" ]
       },
       {
         "flex": "9 auto",
         "_min-main-size": "150px",
         "_main-size": [ "50px",  "180px" ]
       },
     ]
 },
 {
   items:
     [
       {
         "flex": "1 1 0px",
         "_min-main-size": "90px",
         "_main-size": [ null, "90px" ]
       },
       {
         "flex": "1 1 0px",
         "_min-main-size": "80px",
         "_main-size": [ null, "80px" ]
       },
       {
         "flex": "1 1 40px",
         "_main-size": [ null, "30px" ]
       }
     ]
 },

 // Test a case where _min-main-size will be violated on different items in
 // successive iterations of the "resolve the flexible lengths" loop
 {
   items:
     [
       {
         "flex": "1 2 100px",
         "_min-main-size": "90px",
         "_main-size": [ null, "90px" ]
       },
       {
         "flex": "1 1 100px",
         "_min-main-size": "70px",
         "_main-size": [ null, "70px" ]
       },
       {
         "flex": "1 1 100px",
         "_main-size": [ null, "40px" ]
       }
     ]
 },

 // Test some cases that have a min-size violation on one item and a
 // max-size violation on another:

 // Here, both items initially grow to 100px. That violates both
 // items' sizing constraints (it's smaller than the min-size and larger than
 // the max-size), so we clamp both of them and sum the clamping-differences:
 //
 //   (130px - 100px) + (50px - 100px) = (30px) + (-50px) = -20px
 //
 // This sum is negative, so (per spec) we freeze the item that had its
 // max-size violated (the second one) and restart the algorithm.  This time,
 // all the available space (200px - 50px = 150px) goes to the not-yet-frozen
 // first item, and that puts it above its min-size, so all is well.
 {
   items:
     [
       {
         "flex": "auto",
         "_min-main-size": "130px",
         "_main-size": [ null, "150px" ]
       },
       {
         "flex": "auto",
         "_max-main-size": "50px",
         "_main-size": [ null,  "50px" ]
       },
     ]
 },

 // As above, both items initially grow to 100px, and that violates both items'
 // constraints. However, now the sum of the clamping differences is:
 //
 //   (130px - 100px) + (80px - 100px) = (30px) + (-20px) = 10px
 //
 // This sum is positive, so (per spec) we freeze the item that had its
 // min-size violated (the first one) and restart the algorithm. This time,
 // all the available space (200px - 130px = 70px) goes to the not-yet-frozen
 // second item, and that puts it below its max-size, so all is well.
 {
   items:
     [
       {
         "flex": "auto",
         "_min-main-size": "130px",
         "_main-size": [ null, "130px" ]
       },
       {
         "flex": "auto",
         "_max-main-size": "80px",
         "_main-size": [ null,  "70px" ]
       },
     ]
 },

 // As above, both items initially grow to 100px, and that violates both items'
 // constraints. So we clamp both items and sum the clamping differences to
 // see what to do next.  The sum is:
 //
 //   (80px - 100px) + (120px - 100px) = (-20px) + (20px) = 0px
 //
 // Per spec, if the sum is 0, we're done -- we leave both items at their
 // clamped sizes.
 {
   items:
     [
       {
         "flex": "auto",
         "_max-main-size": "80px",
         "_main-size": [ null,  "80px" ]
       },
       {
         "flex": "auto",
         "_min-main-size": "120px",
         "_main-size": [ null, "120px" ]
       },
     ]
 },

 // Test cases where flex-grow sums to less than 1:
 // ===============================================
 // This makes us treat the flexibilities like "fraction of free space"
 // instead of weights, so that e.g. a single item with "flex-grow: 0.1"
 // will only get 10% of the free space instead of all of the free space.

 // Basic cases where flex-grow sum is less than 1:
 {
   items:
     [
       {
         "flex": "0.1 100px",
         "_main-size": [ null, "110px" ] // +10% of free space
       },
     ]
 },
 {
   items:
     [
       {
         "flex": "0.8 0px",
         "_main-size": [ null, "160px" ] // +80% of free space
       },
     ]
 },

 // ... and now with two flex items:
 {
   items:
     [
       {
         "flex": "0.4 70px",
         "_main-size": [ null, "110px" ] // +40% of free space
       },
       {
         "flex": "0.2 30px",
         "_main-size": [ null,  "50px" ] // +20% of free space
       },
     ]
 },

 // ...and now with max-size modifying how much free space one item can take:
 {
   items:
     [
       {
         "flex": "0.4 70px",
         "_main-size": [ null, "110px" ] // +40% of free space
       },
       {
         "flex": "0.2 30px",
         "_max-main-size": "35px",
         "_main-size": [ null,  "35px" ] // +20% free space, then clamped
       },
     ]
 },
 // ...and now with a max-size smaller than our flex-basis:
 // (This makes us freeze the second item right away, before we compute
 // the initial free space.)
 {
   items:
     [
       {
         "flex": "0.4 70px",
         "_main-size": [ null, "118px" ] // +40% of 200px-70px-10px
       },
       {
         "flex": "0.2 30px",
         "_max-main-size": "10px",
         "_main-size": [ null,  "10px" ] // immediately frozen
       },
     ]
 },
 // ...and now with a max-size and a huge flex-basis, such that we initially
 // have negative free space, which makes the "% of [original] free space"
 // calculations a bit more subtle. We set the "original free space" after
 // we've clamped the second item (the first time the free space is positive).
 {
   items:
     [
       {
         "flex": "0.4 70px",
         "_main-size": [ null, "118px" ] // +40% of free space _after freezing
                                         // the other item_
       },
       {
         "flex": "0.2 150px",
         "_max-main-size": "10px",
         "_main-size": [ null,  "10px" ] // clamped immediately
       },
     ]
 },

 // Now with min-size modifying how much free space our items take:
 {
   items:
     [
       {
         "flex": "0.4 70px",
         "_main-size": [ null, "110px" ] // +40% of free space
       },
       {
         "flex": "0.2 30px",
         "_min-main-size": "70px",
         "_main-size": [ null,  "70px" ] // +20% free space, then clamped
       },
     ]
 },

 // ...and now with a large enough min-size that it prevents the other flex
 // item from taking its full desired portion of the original free space:
 {
   items:
     [
       {
         "flex": "0.4 70px",
         "_main-size": [ null, "80px" ] // (Can't take my full +40% of
                                        // free space due to other item's
                                        // large min-size.)
       },
       {
         "flex": "0.2 30px",
         "_min-main-size": "120px",
         "_main-size": [ null,  "120px" ] // +20% free space, then clamped
       },
     ]
 },
 // ...and now with a large-enough min-size that it pushes the other flex item
 // to actually shrink a bit (with default "flex-shrink:1"):
 {
   items:
     [
       {
         "flex": "0.3 30px",
         "_main-size": [ null,  "20px" ] // -10px, instead of desired +45px
       },
       {
         "flex": "0.2 20px",
         "_min-main-size": "180px",
         "_main-size": [ null,  "180px" ] // +160px, instead of desired +30px
       },
     ]
 },

 // In this case, the items' flexibilities don't initially sum to < 1, but they
 // do after we freeze the third item for violating its max-size.
 {
   items:
     [
       {
         "flex": "0.3 30px",
         "_main-size": [ null,  "75px" ]
         // 1st loop: desires (0.3 / 5) * 150px = 9px. Tentatively granted.
         // 2nd loop: desires 0.3 * 150px = 45px. Tentatively granted.
         // 3rd loop: desires 0.3 * 150px = 45px. Granted +45px.
       },
       {
         "flex": "0.2 20px",
         "_max-main-size": "30px",
         "_main-size": [ null,  "30px" ]
         // First loop: desires (0.2 / 5) * 150px = 6px. Tentatively granted.
         // Second loop: desires 0.2 * 150px = 30px. Frozen at +10px.
       },
       {
         "flex": "4.5 0px",
         "_max-main-size": "20px",
         "_main-size": [ null,  "20px" ]
         // First loop: desires (4.5 / 5) * 150px = 135px. Frozen at +20px.
       },
     ]
 },

 // Make sure we calculate "original free space" correctly when one of our
 // flex items will be clamped right away, due to max-size preventing it from
 // growing at all:
 {
   // Here, the second flex item is effectively inflexible; it's
   // immediately frozen at 40px since we're growing & this item's max size
   // trivially prevents it from growing. This leaves us with an "original
   // free space" of 60px. The first flex item takes half of that, due to
   // its flex-grow value of 0.5.
   items:
     [
       {
         "flex": "0.5 100px",
         "_main-size": [ null,  "130px" ]
       },
       {
         "flex": "1 98px",
         "_max-main-size": "40px",
         "_main-size": [ null,  "40px" ]
       },
     ]
 },
 {
   // Same as previous example, but with a larger flex-basis on the second
   // element (which shouldn't ultimately matter, because its max size clamps
   // its size immediately anyway).
   items:
     [
       {
         "flex": "0.5 100px",
         "_main-size": [ null,  "130px" ]
       },
       {
         "flex": "1 101px",
         "_max-main-size": "40px",
         "_main-size": [ null,  "40px" ]
       },
     ]
 },

 {
   // Here, the third flex item is effectively inflexible; it's immediately
   // frozen at 0px since we're growing & this item's max size trivially
   // prevents it from growing. This leaves us with an "original free space" of
   // 100px. The first flex item takes 40px, and the third takes 50px, due to
   // their flex values of 0.4 and 0.5.
   items:
     [
       {
         "flex": "0.4 50px",
         "_main-size": [ null,  "90px" ]
       },
       {
         "flex": "0.5 50px",
         "_main-size": [ null,  "100px" ]
       },
       {
         "flex": "0 90px",
         "_max-main-size": "0px",
         "_main-size": [ null,  "0px" ]
       },
     ]
 },
 {
   // Same as previous example, but with slightly larger flex-grow values on
   // the first and second items, which sum to 1.0 and produce slightly larger
   // main sizes. This demonstrates that there's no discontinuity between the
   // "< 1.0 sum" to ">= 1.0 sum" behavior, in this situation at least.
   items:
     [
       {
         "flex": "0.45 50px",
         "_main-size": [ null,  "95px" ]
       },
       {
         "flex": "0.55 50px",
         "_main-size": [ null,  "105px" ]
       },
       {
         "flex": "0 90px",
         "_max-main-size": "0px",
         "_main-size": [ null,  "0px" ]
       },
     ]
 },

 // Test cases where flex-shrink sums to less than 1:
 // =================================================
 // This makes us treat the flexibilities more like "fraction of (negative)
 // free space" instead of weights, so that e.g. a single item with
 // "flex-shrink: 0.1" will only shrink by 10% of amount that it overflows
 // its container by.
 //
 // It gets a bit more complex when there are multiple flex items, because
 // flex-shrink is scaled by the flex-basis before it's used as a weight. But
 // even with that scaling, the general principal is that e.g. if the
 // flex-shrink values *sum* to 0.6, then the items will collectively only
 // shrink by 60% (and hence will still overflow).

 // Basic cases where flex-grow sum is less than 1:
 {
   items:
     [
       {
         "flex": "0 0.1 300px",
         "_main-size": [ null,  "290px" ] // +10% of (negative) free space
       },
     ]
 },
 {
   items:
     [
       {
         "flex": "0 0.8 400px",
         "_main-size": [ null,  "240px" ] // +80% of (negative) free space
       },
     ]
 },

 // ...now with two flex items, with the same flex-basis value:
 {
   items:
     [
       {
         "flex": "0 0.4 150px",
         "_main-size": [ null,  "110px" ] // +40% of (negative) free space
       },
       {
         "flex": "0 0.2 150px",
         "_main-size": [ null,  "130px" ] // +20% of (negative) free space
       },
     ]
 },

 // ...now with two flex items, with different flex-basis values (and hence
 // differently-scaled flex factors):
 {
   items:
     [
       {
         "flex": "0 0.3 100px",
         "_main-size": [ null,  "76px" ]
       },
       {
         "flex": "0 0.1 200px",
         "_main-size": [ null,  "184px" ]
       }
     ]
     // Notes:
     //  - Free space: -100px
     //  - Sum of flex-shrink factors: 0.3 + 0.1 = 0.4
     //  - Since that sum ^ is < 1, we'll only distribute that fraction of
     //    the free space. We'll distribute: -100px * 0.4 = -40px
     //
     //  - 1st item's scaled flex factor:  0.3 * 100px = 30
     //  - 2nd item's scaled flex factor:  0.1 * 200px = 20
     //  - 1st item's share of distributed free space: 30/(30+20) = 60%
     //  - 2nd item's share of distributed free space: 20/(30+20) = 40%
     //
     // SO:
     //  - 1st item gets 60% * -40px = -24px.  100px-24px = 76px
     //  - 2nd item gets 40% * -40px = -16px.  200px-16px = 184px
 },

 // ...now with min-size modifying how much one item can shrink:
 {
   items:
     [
       {
         "flex": "0 0.3 100px",
         "_main-size": [ null,  "70px" ]
       },
       {
         "flex": "0 0.1 200px",
         "_min-main-size": "190px",
         "_main-size": [ null,  "190px" ]
       }
     ]
     // Notes:
     //  - We proceed as in previous testcase, but clamp the second flex item
     //    at its min main size.
     //  - After that point, we have a total flex-shrink of = 0.3, so we
     //    distribute 0.3 * -100px = -30px to the remaining unfrozen flex
     //    items. Since there's only one unfrozen item left, it gets all of it.
 },

 // ...now with min-size larger than our flex-basis:
 // (This makes us freeze the second item right away, before we compute
 // the initial free space.)
 {
   items:
     [
       {
         "flex": "0 0.3 100px",
         "_main-size": [ null,  "55px" ] // +30% of 200px-100px-250px
       },
       {
         "flex": "0 0.1 200px",
         "_min-main-size": "250px",
         "_main-size": [ null,  "250px" ] // immediately frozen
       }
     ]
     // (Same as previous example, except the min-main-size prevents the
     // second item from shrinking at all)
 },

 // ...and now with a min-size and a small flex-basis, such that we initially
 // have positive free space, which makes the "% of [original] free space"
 // calculations a bit more subtle. We set the "original free space" after
 // we've clamped the second item (the first time the free space is negative).
 {
   items:
     [
       {
         "flex": "0 0.3 100px",
         "_main-size": [ null,  "70px" ]
       },
       {
         "flex": "0 0.1 50px",
         "_min-main-size": "200px",
         "_main-size": [ null,  "200px" ]
       }
     ]
 },

 // Now with max-size making an item shrink more than its flex-shrink value
 // calls for:
 {
   items:
     [
       {
         "flex": "0 0.3 100px",
         "_main-size": [ null,  "70px" ]
       },
       {
         "flex": "0 0.1 200px",
         "_max-main-size": "150px",
         "_main-size": [ null,  "150px" ]
       }
     ]
     // Notes:
     //  - We proceed as in an earlier testcase, but clamp the second flex item
     //    at its max main size.
     //  - After that point, we have a total flex-shrink of = 0.3, so we
     //    distribute 0.3 * -100px = -30px to the remaining unfrozen flex
     //    items. Since there's only one unfrozen item left, it gets all of it.
 },

 // ...and now with a small enough max-size that it prevents the other flex
 // item from taking its full desired portion of the (negative) original free
 // space:
 {
   items:
     [
       {
         "flex": "0 0.3 100px",
         "_main-size": [ null,  "90px" ]
       },
       {
         "flex": "0 0.1 200px",
         "_max-main-size": "110px",
         "_main-size": [ null,  "110px" ]
       }
     ]
     // Notes:
     //  - We proceed as in an earlier testcase, but clamp the second flex item
     //    at its max main size.
     //  - After that point, we have a total flex-shrink of 0.3, which would
     //    have us distribute 0.3 * -100px = -30px to the (one) remaining
     //    unfrozen flex item. But our remaining free space is only -10px at
     //    that point, so we distribute that instead.
 },

 // ...and now with a small enough max-size that it pushes the other flex item
 // to actually grow a bit (with custom "flex-grow: 1" for this testcase):
 {
   items:
     [
       {
         "flex": "1 0.3 100px",
         "_main-size": [ null,  "120px" ]
       },
       {
         "flex": "1 0.1 200px",
         "_max-main-size": "80px",
         "_main-size": [ null,  "80px" ]
       }
     ]
 },

 // In this case, the items' flexibilities don't initially sum to < 1, but they
 // do after we freeze the third item for violating its min-size.
 {
   items:
     [
       {
         "flex": "0 0.3 100px",
         "_main-size": [ null,  "76px" ]
       },
       {
         "flex": "0 0.1 150px",
         "_main-size": [ null,  "138px" ]
       },
       {
         "flex": "0 0.8 10px",
         "_min-main-size": "40px",
         "_main-size": [ null,  "40px" ]
       }
     ]
     // Notes:
     //  - We immediately freeze the 3rd item, since we're shrinking and its
     //    min size obviously prevents it from shrinking at all.  This leaves
     //    200px - 100px - 150px - 40px = -90px of "initial free space".
     //
     //  - Our remaining flexible items have a total flex-shrink of 0.4,
     //    so we can distribute a total of 0.4 * -90px = -36px
     //
     //  - We distribute that space using *scaled* flex factors:
     //    * 1st item's scaled flex factor:  0.3 * 100px = 30
     //    * 2nd item's scaled flex factor:  0.1 * 150px = 15
     //   ...which means...
     //    * 1st item's share of distributed free space: 30/(30+15) = 2/3
     //    * 2nd item's share of distributed free space: 15/(30+15) = 1/3
     //
     // SO:
     //  - 1st item gets 2/3 * -36px = -24px. 100px - 24px = 76px
     //  - 2nd item gets 1/3 * -36px = -12px. 150px - 12px = 138px
 },

 // In this case, the items' flexibilities sum to > 1, in part due to an item
 // that *can't actually shrink* due to its 0 flex-basis (which gives it a
 // "scaled flex factor" of 0). This prevents us from triggering the special
 // behavior for flexibilities that sum to less than 1, and as a result, the
 // first item ends up absorbing all of the free space.
 {
   items:
     [
       {
         "flex": "0 .5 300px",
         "_main-size": [ null,  "200px" ]
       },
       {
         "flex": "0 5 0px",
         "_main-size": [ null,  "0px" ]
       }
     ]
 },

 // This case is similar to the one above, but with a *barely* nonzero base
 // size for the second item. This should produce a result similar to the case
 // above. (In particular, we should first distribute a very small amount of
 // negative free space to the second item, getting it to approximately zero,
 // and distribute the bulk of the negative free space to the first item,
 // getting it to approximately 200px.)
 {
   items:
     [
       {
         "flex": "0 .5 300px",
         "_main-size": [ null,  "200px" ]
       },
       {
         "flex": "0 1 0.01px",
         "_main-size": [ null,  "0px" ]
       }
     ]
 },
 // This case is similar to the ones above, but now we've increased the
 // flex-shrink value on the second-item so that it claims enough of the
 // negative free space to go below its min-size (0px). So, it triggers a min
 // violation & is frozen. For the loop *after* the min violation, the sum of
 // the remaining flex items' flex-shrink values is less than 1, so we trigger
 // the special <1 behavior and only distribute half of the remaining
 // (negative) free space to the first item (instead of all of it).
 {
   items:
     [
       {
         "flex": "0 .5 300px",
         "_main-size": [ null,  "250px" ]
       },
       {
         "flex": "0 5 0.01px",
         "_main-size": [ null,  "0px" ]
       }
     ]
 },
];
