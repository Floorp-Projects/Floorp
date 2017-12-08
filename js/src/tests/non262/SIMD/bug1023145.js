// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

delete Object.prototype.__proto__;
var Int32x4 = SIMD.Int32x4;
var ar = Int32x4.array(1);
var array = new ar([Int32x4(1, 2, 3, 4)]);

if (typeof reportCompare === "function")
    reportCompare(true, true);
