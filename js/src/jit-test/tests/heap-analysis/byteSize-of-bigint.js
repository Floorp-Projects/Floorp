// |jit-test| skip-if: !getBuildConfiguration()['moz-memory']
// Run this test only if we're using jemalloc. Other malloc implementations
// exhibit surprising behaviors. For example, 32-bit Fedora builds have
// non-deterministic allocation sizes.

// Check JS::ubi::Node::size results for BigInts.

// We actually hard-code specific sizes into this test, even though they're
// implementation details, because in practice there are only two architecture
// variants to consider (32-bit and 64-bit), and if these sizes change, that's
// something SpiderMonkey hackers really want to know; they're supposed to be
// stable.

const config = getBuildConfiguration();

const pointerByteSize = config["pointer-byte-size"];
assertEq(pointerByteSize === 4 || pointerByteSize === 8, true);

const m32 = pointerByteSize === 4;

// 32-bit: sizeof(CellWithLengthAndFlags) + 2 * sizeof(BigInt::Digit) = 8 + 2 * 4 = 16
// 64-bit: sizeof(CellWithLengthAndFlags) + sizeof(BigInt::Digit) = 8 + 8 = 16
const SIZE_OF_BIGINT = 16;

// sizeof(BigInt::Digit)
const SIZE_OF_DIGIT = pointerByteSize;

// sizeof(JS::Value)
const SIZE_OF_VALUE = 8;

// See Nursery::bigIntHeaderSize().
const SIZE_OF_BIGINT_HEADER = 8;

const SIZE_OF_TENURED_BIGINT = SIZE_OF_BIGINT;
const SIZE_OF_NURSERY_BIGINT = SIZE_OF_BIGINT + SIZE_OF_BIGINT_HEADER;

function nurseryDigitSize(length) {
  // See <https://bugzilla.mozilla.org/show_bug.cgi?id=1607186> for why we currently
  // overallocate on 32-bit.
  if (m32) {
    length += (length & 1);
  }
  return length * SIZE_OF_DIGIT;
}

function mallocDigitSize(length) {
  // See <https://bugzilla.mozilla.org/show_bug.cgi?id=1607186> for why we currently
  // overallocate on 32-bit.
  if (m32) {
    length += (length & 1);
  }

  // Malloc buffer sizes are always a power of two.
  return 1 << Math.ceil(Math.log2(length * SIZE_OF_DIGIT));
}

// Constant BigInts (tenured, inline digits).
assertEq(byteSize(10n), SIZE_OF_TENURED_BIGINT);
assertEq(byteSize(0xffff_ffff_ffff_ffffn), SIZE_OF_TENURED_BIGINT);

// Constant BigInt (tenured, heap digits).
assertEq(byteSize(0x1_0000_0000_0000_0000n),
         SIZE_OF_TENURED_BIGINT + mallocDigitSize(m32 ? 3 : 2));
assertEq(byteSize(0x1_0000_0000_0000_0000_0000_0000n),
         SIZE_OF_TENURED_BIGINT + mallocDigitSize(m32 ? 4 : 2));
assertEq(byteSize(0x1_0000_0000_0000_0000_0000_0000_0000_0000n),
         SIZE_OF_TENURED_BIGINT + mallocDigitSize(m32 ? 5 : 3));


///////////////////////////////////////////////////////////////////////////////
// Nursery BigInt tests below this point.                                    //
///////////////////////////////////////////////////////////////////////////////

// Hack to skip this test if BigInts are not allocated in the nursery.
{
  const sample_nursery = BigInt(123456789);

  const before = byteSize(sample_nursery);
  gc();
  const after = byteSize(sample_nursery);

  let nursery_disabled = before == after;
  if (nursery_disabled) {
    printErr("nursery BigInts appear to be disabled");
    quit(0);
  }
}

// Convert an input BigInt, which is probably tenured because it's a literal in
// the source text, to a nursery-allocated BigInt with the same contents.
function copyBigInt(bi) {
  var plusOne = bi + 1n;
  return plusOne - 1n;
}

// Return the nursery byte size of |bi|.
function nByteSize(bi) {
  // BigInts that appear in the source will always be tenured.
  return byteSize(copyBigInt(bi));
}

// BigInts (nursery, inline digits).
assertEq(nByteSize(10n), SIZE_OF_NURSERY_BIGINT);
assertEq(nByteSize(0xffff_ffff_ffff_ffffn), SIZE_OF_NURSERY_BIGINT);

// BigInt (nursery, nursery heap digits).
//
// This assumes small nursery buffer allocations always succeed.
assertEq(nByteSize(0x1_0000_0000_0000_0000n),
         SIZE_OF_NURSERY_BIGINT + nurseryDigitSize(m32 ? 3 : 2));
assertEq(nByteSize(0x1_0000_0000_0000_0000_0000_0000n),
         SIZE_OF_NURSERY_BIGINT + nurseryDigitSize(m32 ? 4 : 2));
assertEq(nByteSize(0x1_0000_0000_0000_0000_0000_0000_0000_0000n),
         SIZE_OF_NURSERY_BIGINT + nurseryDigitSize(m32 ? 5 : 3));

// BigInt (nursery, malloc heap digits).
//
// |Nursery::MaxNurseryBufferSize| is 1024, so when
// |BigInt::digitLength * sizeof(BigInt::Digit)| exceeds 1024, the digits buffer
// should be malloc'ed. Pick a larger number to be future-proof.
assertEq(nByteSize(2n ** (64n * 1000n)),
         SIZE_OF_NURSERY_BIGINT + mallocDigitSize(m32 ? 2002 : 1001));
