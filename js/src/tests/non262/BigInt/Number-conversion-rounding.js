// |reftest| skip-if(!this.hasOwnProperty("BigInt"))
// Any copyright is dedicated to the Public Domain.
// https://creativecommons.org/licenses/publicdomain/

/**
 * Simple single-Digit on x64, double-Digit on x86 tests.
 */

assertEq(BigInt(Number(2n**53n - 2n)), 2n**53n - 2n);
assertEq(BigInt(Number(2n**53n - 1n)), 2n**53n - 1n);
assertEq(BigInt(Number(2n**53n)), 2n**53n);
assertEq(BigInt(Number(2n**53n + 1n)), 2n**53n);
assertEq(BigInt(Number(2n**53n + 2n)), 2n**53n + 2n);
assertEq(BigInt(Number(2n**53n + 3n)), 2n**53n + 4n);
assertEq(BigInt(Number(2n**53n + 4n)), 2n**53n + 4n);
assertEq(BigInt(Number(2n**53n + 5n)), 2n**53n + 4n);
assertEq(BigInt(Number(2n**53n + 6n)), 2n**53n + 6n);
assertEq(BigInt(Number(2n**53n + 7n)), 2n**53n + 8n);
assertEq(BigInt(Number(2n**53n + 8n)), 2n**53n + 8n);

assertEq(BigInt(Number(2n**54n - 4n)), 2n**54n - 4n);
assertEq(BigInt(Number(2n**54n - 3n)), 2n**54n - 4n);
assertEq(BigInt(Number(2n**54n - 2n)), 2n**54n - 2n);
assertEq(BigInt(Number(2n**54n - 1n)), 2n**54n);
assertEq(BigInt(Number(2n**54n)), 2n**54n);
assertEq(BigInt(Number(2n**54n + 1n)), 2n**54n);
assertEq(BigInt(Number(2n**54n + 2n)), 2n**54n);
assertEq(BigInt(Number(2n**54n + 3n)), 2n**54n + 4n);
assertEq(BigInt(Number(2n**54n + 4n)), 2n**54n + 4n);
assertEq(BigInt(Number(2n**54n + 5n)), 2n**54n + 4n);
assertEq(BigInt(Number(2n**54n + 6n)), 2n**54n + 8n);
assertEq(BigInt(Number(2n**54n + 7n)), 2n**54n + 8n);
assertEq(BigInt(Number(2n**54n + 8n)), 2n**54n + 8n);

assertEq(BigInt(Number(2n**55n - 8n)), 2n**55n - 8n);
assertEq(BigInt(Number(2n**55n - 7n)), 2n**55n - 8n);
assertEq(BigInt(Number(2n**55n - 6n)), 2n**55n - 8n);
assertEq(BigInt(Number(2n**55n - 5n)), 2n**55n - 4n);
assertEq(BigInt(Number(2n**55n - 4n)), 2n**55n - 4n);
assertEq(BigInt(Number(2n**55n - 3n)), 2n**55n - 4n);
assertEq(BigInt(Number(2n**55n - 2n)), 2n**55n);
assertEq(BigInt(Number(2n**55n - 1n)), 2n**55n);
assertEq(BigInt(Number(2n**55n)), 2n**55n);
assertEq(BigInt(Number(2n**55n + 1n)), 2n**55n);
assertEq(BigInt(Number(2n**55n + 2n)), 2n**55n);
assertEq(BigInt(Number(2n**55n + 3n)), 2n**55n);
assertEq(BigInt(Number(2n**55n + 4n)), 2n**55n);
assertEq(BigInt(Number(2n**55n + 5n)), 2n**55n + 8n);
assertEq(BigInt(Number(2n**55n + 6n)), 2n**55n + 8n);
assertEq(BigInt(Number(2n**55n + 7n)), 2n**55n + 8n);
assertEq(BigInt(Number(2n**55n + 8n)), 2n**55n + 8n);
assertEq(BigInt(Number(2n**55n + 9n)), 2n**55n + 8n);
assertEq(BigInt(Number(2n**55n + 10n)), 2n**55n + 8n);
assertEq(BigInt(Number(2n**55n + 11n)), 2n**55n + 8n);
assertEq(BigInt(Number(2n**55n + 12n)), 2n**55n + 16n);
assertEq(BigInt(Number(2n**55n + 13n)), 2n**55n + 16n);
assertEq(BigInt(Number(2n**55n + 14n)), 2n**55n + 16n);
assertEq(BigInt(Number(2n**55n + 15n)), 2n**55n + 16n);
assertEq(BigInt(Number(2n**55n + 16n)), 2n**55n + 16n);


/**
 * Simple double-Digit on x64, triple-Digit on x86 tests.
 */

// The tests below that aren't subtracting bits will have no bits in the
// ultimate significand from the most-significant digit (because of the implicit
// one being excluded).
assertEq(BigInt(Number(2n**64n - 2n**11n)), 2n**64n - 2n**11n);
assertEq(BigInt(Number(2n**64n - 2n**11n + 2n**10n - 1n)), 2n**64n - 2n**11n);
assertEq(BigInt(Number(2n**64n - 2n**11n + 2n**10n)), 2n**64n);
assertEq(BigInt(Number(2n**64n - 2n**10n)), 2n**64n);
assertEq(BigInt(Number(2n**64n)), 2n**64n);
assertEq(BigInt(Number(2n**64n + 1n)), 2n**64n);
assertEq(BigInt(Number(2n**64n + 2n**5n)), 2n**64n);
assertEq(BigInt(Number(2n**64n + 2n**10n)), 2n**64n);
assertEq(BigInt(Number(2n**64n + 2n**11n)), 2n**64n);
assertEq(BigInt(Number(2n**64n + 2n**11n + 1n)), 2n**64n + 2n**12n);
assertEq(BigInt(Number(2n**64n + 2n**12n)), 2n**64n + 2n**12n);
assertEq(BigInt(Number(2n**64n + 2n**12n + 1n)), 2n**64n + 2n**12n);
assertEq(BigInt(Number(2n**64n + 2n**12n + 2n**5n)), 2n**64n + 2n**12n);
assertEq(BigInt(Number(2n**64n + 2n**12n + 2n**10n)), 2n**64n + 2n**12n);
assertEq(BigInt(Number(2n**64n + 2n**12n + 2n**11n - 1n)), 2n**64n + 2n**12n);
assertEq(BigInt(Number(2n**64n + 2n**12n + 2n**11n)), 2n**64n + 2n**13n);
assertEq(BigInt(Number(2n**64n + 2n**12n + 2n**11n + 1n)), 2n**64n + 2n**13n);

// These tests *will* have a bit from the most-significant digit in the ultimate
// significand.
assertEq(BigInt(Number(2n**65n - 2n**12n)), 2n**65n - 2n**12n);
assertEq(BigInt(Number(2n**65n - 2n**12n + 2n**11n - 1n)), 2n**65n - 2n**12n);
assertEq(BigInt(Number(2n**65n - 2n**12n + 2n**11n)), 2n**65n);
assertEq(BigInt(Number(2n**65n - 2n**11n)), 2n**65n);
assertEq(BigInt(Number(2n**65n)), 2n**65n);
assertEq(BigInt(Number(2n**65n + 1n)), 2n**65n);
assertEq(BigInt(Number(2n**65n + 2n**5n)), 2n**65n);
assertEq(BigInt(Number(2n**65n + 2n**11n)), 2n**65n);
assertEq(BigInt(Number(2n**65n + 2n**12n)), 2n**65n);
assertEq(BigInt(Number(2n**65n + 2n**12n + 1n)), 2n**65n + 2n**13n);
assertEq(BigInt(Number(2n**65n + 2n**13n)), 2n**65n + 2n**13n);
assertEq(BigInt(Number(2n**65n + 2n**13n + 1n)), 2n**65n + 2n**13n);
assertEq(BigInt(Number(2n**65n + 2n**13n + 2n**5n)), 2n**65n + 2n**13n);
assertEq(BigInt(Number(2n**65n + 2n**13n + 2n**11n)), 2n**65n + 2n**13n);
assertEq(BigInt(Number(2n**65n + 2n**13n + 2n**12n - 1n)), 2n**65n + 2n**13n);
assertEq(BigInt(Number(2n**65n + 2n**13n + 2n**12n)), 2n**65n + 2n**14n);
assertEq(BigInt(Number(2n**65n + 2n**13n + 2n**12n + 1n)), 2n**65n + 2n**14n);

// ...and in these tests, the contributed bit from the most-significant digit
// is additionally nonzero.
assertEq(BigInt(Number(2n**65n + 2n**64n)), 2n**65n + 2n**64n);
assertEq(BigInt(Number(2n**65n + 2n**64n + 1n)), 2n**65n + 2n**64n);
assertEq(BigInt(Number(2n**65n + 2n**64n + 2n**5n)), 2n**65n + 2n**64n);
assertEq(BigInt(Number(2n**65n + 2n**64n + 2n**11n)), 2n**65n + 2n**64n);
assertEq(BigInt(Number(2n**65n + 2n**64n + 2n**12n)), 2n**65n + 2n**64n);
assertEq(BigInt(Number(2n**65n + 2n**64n + 2n**12n + 1n)), 2n**65n + 2n**64n + 2n**13n);
assertEq(BigInt(Number(2n**65n + 2n**64n + 2n**13n)), 2n**65n + 2n**64n + 2n**13n);
assertEq(BigInt(Number(2n**65n + 2n**64n + 2n**13n + 1n)), 2n**65n + 2n**64n + 2n**13n);
assertEq(BigInt(Number(2n**65n + 2n**64n + 2n**13n + 2n**5n)), 2n**65n + 2n**64n + 2n**13n);
assertEq(BigInt(Number(2n**65n + 2n**64n + 2n**13n + 2n**11n)), 2n**65n + 2n**64n + 2n**13n);
assertEq(BigInt(Number(2n**65n + 2n**64n + 2n**13n + 2n**12n - 1n)), 2n**65n + 2n**64n + 2n**13n);
assertEq(BigInt(Number(2n**65n + 2n**64n + 2n**13n + 2n**12n)), 2n**65n + 2n**64n + 2n**14n);
assertEq(BigInt(Number(2n**65n + 2n**64n + 2n**13n + 2n**12n + 1n)), 2n**65n + 2n**64n + 2n**14n);

/**
 * Versions of the testing above with all the high-order bits massively bumped
 * upward to test that super-low bits, not just bits in high digits, are
 * properly accounted for in rounding.
 */

// The tests below that aren't subtracting bits will have no bits in the
// ultimate significand from the most-significant digit (because of the implicit
// one being excluded).
assertEq(BigInt(Number(2n**940n - 2n**887n + 1n)), 2n**940n - 2n**887n);
assertEq(BigInt(Number(2n**940n - 2n**887n + 2n**886n - 1n)), 2n**940n - 2n**887n);
assertEq(BigInt(Number(2n**940n - 2n**887n + 2n**886n)), 2n**940n);
assertEq(BigInt(Number(2n**940n - 2n**886n)), 2n**940n);
assertEq(BigInt(Number(2n**940n)), 2n**940n);
assertEq(BigInt(Number(2n**940n + 1n)), 2n**940n);
assertEq(BigInt(Number(2n**940n + 2n**880n)), 2n**940n);
assertEq(BigInt(Number(2n**940n + 2n**885n)), 2n**940n);
assertEq(BigInt(Number(2n**940n + 2n**887n)), 2n**940n);
assertEq(BigInt(Number(2n**940n + 2n**887n + 1n)), 2n**940n + 2n**888n);
assertEq(BigInt(Number(2n**940n + 2n**888n)), 2n**940n + 2n**888n);
assertEq(BigInt(Number(2n**940n + 2n**888n + 1n)), 2n**940n + 2n**888n);
assertEq(BigInt(Number(2n**940n + 2n**888n + 2n**5n)), 2n**940n + 2n**888n);
assertEq(BigInt(Number(2n**940n + 2n**888n + 2n**12n)), 2n**940n + 2n**888n);
assertEq(BigInt(Number(2n**940n + 2n**888n + 2n**887n - 1n)), 2n**940n + 2n**888n);
assertEq(BigInt(Number(2n**940n + 2n**888n + 2n**887n)), 2n**940n + 2n**889n);
assertEq(BigInt(Number(2n**940n + 2n**888n + 2n**887n + 1n)), 2n**940n + 2n**889n);

// These tests *will* have a bit from the most-significant digit in the ultimate
// significand.
assertEq(BigInt(Number(2n**941n - 2n**888n)), 2n**941n - 2n**888n);
assertEq(BigInt(Number(2n**941n - 2n**888n + 2n**887n - 1n)), 2n**941n - 2n**888n);
assertEq(BigInt(Number(2n**941n - 2n**888n + 2n**887n)), 2n**941n);
assertEq(BigInt(Number(2n**941n - 2n**887n)), 2n**941n);
assertEq(BigInt(Number(2n**941n)), 2n**941n);
assertEq(BigInt(Number(2n**941n + 1n)), 2n**941n);
assertEq(BigInt(Number(2n**941n + 2n**881n)), 2n**941n);
assertEq(BigInt(Number(2n**941n + 2n**886n)), 2n**941n);
assertEq(BigInt(Number(2n**941n + 2n**888n)), 2n**941n);
assertEq(BigInt(Number(2n**941n + 2n**888n + 1n)), 2n**941n + 2n**889n);
assertEq(BigInt(Number(2n**941n + 2n**889n)), 2n**941n + 2n**889n);
assertEq(BigInt(Number(2n**941n + 2n**889n + 1n)), 2n**941n + 2n**889n);
assertEq(BigInt(Number(2n**941n + 2n**889n + 2n**5n)), 2n**941n + 2n**889n);
assertEq(BigInt(Number(2n**941n + 2n**889n + 2n**12n)), 2n**941n + 2n**889n);
assertEq(BigInt(Number(2n**941n + 2n**889n + 2n**888n - 1n)), 2n**941n + 2n**889n);
assertEq(BigInt(Number(2n**941n + 2n**889n + 2n**888n)), 2n**941n + 2n**890n);
assertEq(BigInt(Number(2n**941n + 2n**889n + 2n**888n + 1n)), 2n**941n + 2n**890n);

// ...and in these tests, the contributed bit from the most-significant digit
// is additionally nonzero.
assertEq(BigInt(Number(2n**941n + 2n**940n)), 2n**941n + 2n**940n);
assertEq(BigInt(Number(2n**941n + 2n**940n + 1n)), 2n**941n + 2n**940n);
assertEq(BigInt(Number(2n**941n + 2n**940n + 2n**881n)), 2n**941n + 2n**940n);
assertEq(BigInt(Number(2n**941n + 2n**940n + 2n**886n)), 2n**941n + 2n**940n);
assertEq(BigInt(Number(2n**941n + 2n**940n + 2n**888n)), 2n**941n + 2n**940n);
assertEq(BigInt(Number(2n**941n + 2n**940n + 2n**888n + 1n)), 2n**941n + 2n**940n + 2n**889n);
assertEq(BigInt(Number(2n**941n + 2n**940n + 2n**889n)), 2n**941n + 2n**940n + 2n**889n);
assertEq(BigInt(Number(2n**941n + 2n**940n + 2n**889n + 1n)), 2n**941n + 2n**940n + 2n**889n);
assertEq(BigInt(Number(2n**941n + 2n**940n + 2n**889n + 2n**5n)), 2n**941n + 2n**940n + 2n**889n);
assertEq(BigInt(Number(2n**941n + 2n**940n + 2n**889n + 2n**12n)), 2n**941n + 2n**940n + 2n**889n);
assertEq(BigInt(Number(2n**941n + 2n**940n + 2n**889n + 2n**888n - 1n)), 2n**941n + 2n**940n + 2n**889n);
assertEq(BigInt(Number(2n**941n + 2n**940n + 2n**889n + 2n**888n)), 2n**941n + 2n**940n + 2n**890n);
assertEq(BigInt(Number(2n**941n + 2n**940n + 2n**889n + 2n**888n + 1n)), 2n**941n + 2n**940n + 2n**890n);

if (typeof reportCompare === "function")
  reportCompare(true, true);
