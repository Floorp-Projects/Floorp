/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *  (a) the property's specified value
 *  ...or...
 *  (b) an array with 2 entries: [specifiedValue, expectedComputedValue] if the
 *      property's computed value is intended to be checked. The first entry
 *      (for the specified value) may be null; this means that no value should
 *      be explicitly specified for this property.
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
         "_main-size": [ null,  "1px" ]
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
         "_main-size": [ null,  "1px" ]
       },
       {
         "flex": "1",
         "_main-size": [ null,  "1px" ]
       },
       {
         "flex": "1",
         "_main-size": [ null,  "1px" ]
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
         "_main-size": [ null,  "0.966667px" ]
       },
       {
         "flex": "1",
         // NOTE: Expected value is off slightly, from float error when
         // resolving flexible lengths:
         "_main-size": [ null,  "0.983333px" ]
       },
       {
         "flex": "1",
         // NOTE: Expected value is off slightly, from float error when
         // resolving flexible lengths:
         "_main-size": [ null,  "0.983333px" ]
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
];
