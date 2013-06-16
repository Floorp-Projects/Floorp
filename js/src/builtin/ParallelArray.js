/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// FIXME(bug 844882): Parallel array properties should not be exposed.

// The mode asserts options object.
#define TRY_PARALLEL(MODE) \
  ((!MODE || MODE.mode !== "seq"))
#define ASSERT_SEQUENTIAL_IS_OK(MODE) \
  do { if (MODE) AssertSequentialIsOK(MODE) } while(false)

// Slice array: see ComputeAllSliceBounds()
#define SLICE_INFO(START, END) START, END, START, 0
#define SLICE_START(ID) ((ID << 2) + 0)
#define SLICE_END(ID)   ((ID << 2) + 1)
#define SLICE_POS(ID)   ((ID << 2) + 2)

// How many items at a time do we do recomp. for parallel execution.
// Note that filter currently assumes that this is no greater than 32
// in order to make use of a bitset.
#define CHUNK_SHIFT 5
#define CHUNK_SIZE 32

// Safe versions of ARRAY.push(ELEMENT)
#define ARRAY_PUSH(ARRAY, ELEMENT) \
  callFunction(std_Array_push, ARRAY, ELEMENT);
#define ARRAY_SLICE(ARRAY, ELEMENT) \
  callFunction(std_Array_slice, ARRAY, ELEMENT);

/**
 * Determine the number of chunks of size CHUNK_SIZE;
 * note that the final chunk may be smaller than CHUNK_SIZE.
 */
function ComputeNumChunks(length) {
  var chunks = length >>> CHUNK_SHIFT;
  if (chunks << CHUNK_SHIFT === length)
    return chunks;
  return chunks + 1;
}

/**
 * Computes the bounds for slice |sliceIndex| of |numItems| items,
 * assuming |numSlices| total slices. If numItems is not evenly
 * divisible by numSlices, then the final thread may have a bit of
 * extra work.
 */
function ComputeSliceBounds(numItems, sliceIndex, numSlices) {
  var sliceWidth = (numItems / numSlices) | 0;
  var startIndex = sliceWidth * sliceIndex;
  var endIndex = sliceIndex === numSlices - 1 ? numItems : sliceWidth * (sliceIndex + 1);
  return [startIndex, endIndex];
}

/**
 * Divides |numItems| items amongst |numSlices| slices. The result
 * is an array containing multiple values per slice: the start
 * index, end index, current position, and some padding. The
 * current position is initially the same as the start index. To
 * access the values for a particular slice, use the macros
 * SLICE_START() and so forth.
 */
function ComputeAllSliceBounds(numItems, numSlices) {
  // FIXME(bug 844890): Use typed arrays here.
  var info = [];
  for (var i = 0; i < numSlices; i++) {
    var [start, end] = ComputeSliceBounds(numItems, i, numSlices);
    ARRAY_PUSH(info, SLICE_INFO(start, end));
  }
  return info;
}

/**
 * Compute the partial products in reverse order.
 * e.g., if the shape is [A,B,C,D], then the
 * array |products| will be [1,D,CD,BCD].
 */
function ComputeProducts(shape) {
  var product = 1;
  var products = [1];
  var sdimensionality = shape.length;
  for (var i = sdimensionality - 1; i > 0; i--) {
    product *= shape[i];
    ARRAY_PUSH(products, product);
  }
  return products;
}

/**
 * Given a shape and some index |index1d|, computes and returns an
 * array containing the N-dimensional index that maps to |index1d|.
 */
function ComputeIndices(shape, index1d) {

  var products = ComputeProducts(shape);
  var l = shape.length;

  var result = [];
  for (var i = 0; i < l; i++) {
    // Obtain product of all higher dimensions.
    // So if i == 0 and shape is [A,B,C,D], yields BCD.
    var stride = products[l - i - 1];

    // Compute how many steps of width stride we could take.
    var index = (index1d / stride) | 0;
    ARRAY_PUSH(result, index);

    // Adjust remaining indices for smaller dimensions.
    index1d -= (index * stride);
  }

  return result;
}

function StepIndices(shape, indices) {
  for (var i = shape.length - 1; i >= 0; i--) {
    var indexi = indices[i] + 1;
    if (indexi < shape[i]) {
      indices[i] = indexi;
      return;
    }
    indices[i] = 0;
  }
}

// Constructor
//
// We split the 3 construction cases so that we don't case on arguments.

/**
 * This is the function invoked for |new ParallelArray()|
 */
function ParallelArrayConstructEmpty() {
  this.buffer = [];
  this.offset = 0;
  this.shape = [0];
  this.get = ParallelArrayGet1;
}

/**
 * This is the function invoked for |new ParallelArray(array)|.
 * It copies the data from its array-like argument |array|.
 */
function ParallelArrayConstructFromArray(buffer) {
  var buffer = ToObject(buffer);
  var length = buffer.length >>> 0;
  if (length !== buffer.length)
    ThrowError(JSMSG_PAR_ARRAY_BAD_ARG, "");

  var buffer1 = [];
  for (var i = 0; i < length; i++)
    ARRAY_PUSH(buffer1, buffer[i]);

  this.buffer = buffer1;
  this.offset = 0;
  this.shape = [length];
  this.get = ParallelArrayGet1;
}

/**
 * Wrapper around |ParallelArrayConstructFromComprehension()| for the
 * case where 2 arguments are supplied. This is typically what users will
 * invoke. We provide an explicit two-argument version rather than
 * relying on JS's semantics for absent arguments because it simplifies
 * the ion code that does inlining of PA constructors.
 */
function ParallelArrayConstructFromFunction(shape, func) {
  return ParallelArrayConstructFromComprehension(this, shape, func, undefined);
}

/**
 * Wrapper around |ParallelArrayConstructFromComprehension()| for the
 * case where 3 arguments are supplied.
 */
function ParallelArrayConstructFromFunctionMode(shape, func, mode) {
  return ParallelArrayConstructFromComprehension(this, shape, func, mode);
}

/**
 * "Comprehension form": This is the function invoked for |new
 * ParallelArray(dim, fn)|. If |dim| is a number, then it creates a
 * new 1-dimensional parallel array with shape |[dim]| where index |i|
 * is equal to |fn(i)|. If |dim| is a vector, then it creates a new
 * N-dimensional parallel array where index |a, b, ... z| is equal to
 * |fn(a, b, ...z)|.
 *
 * The final |mode| argument is an internal argument used only
 * during our unit-testing.
 */
function ParallelArrayConstructFromComprehension(self, shape, func, mode) {
  // FIXME(bug 844887): Check |IsCallable(func)|

  if (typeof shape === "number") {
    var length = shape >>> 0;
    if (length !== shape)
      ThrowError(JSMSG_PAR_ARRAY_BAD_ARG, "");
    ParallelArrayBuild(self, [length], func, mode);
  } else if (!shape || typeof shape.length !== "number") {
    ThrowError(JSMSG_PAR_ARRAY_BAD_ARG, "");
  } else {
    var shape1 = [];
    for (var i = 0, l = shape.length; i < l; i++) {
      var s0 = shape[i];
      var s1 = s0 >>> 0;
      if (s1 !== s0)
        ThrowError(JSMSG_PAR_ARRAY_BAD_ARG, "");
      ARRAY_PUSH(shape1, s1);
    }
    ParallelArrayBuild(self, shape1, func, mode);
  }
}

/**
 * Internal function used when constructing new parallel arrays. The
 * NewParallelArray() intrinsic takes a ctor function which it invokes
 * with the given shape, buffer, offset. The |this| parameter will be
 * the newly constructed parallel array.
 */
function ParallelArrayView(shape, buffer, offset) {
  this.shape = shape;
  this.buffer = buffer;
  this.offset = offset;

  switch (shape.length) {
    case 1: this.get = ParallelArrayGet1; break;
    case 2: this.get = ParallelArrayGet2; break;
    case 3: this.get = ParallelArrayGet3; break;
    default: this.get = ParallelArrayGetN; break;
  }

  // Due to inlining of NewParallelArray, the return type of this function
  // gets recorded as the return type of NewParallelArray at inlined sites, so
  // we must take care to return the same thing.
  return this;
}

/**
 * Helper for the comprehension form. Constructs an N-dimensional
 * array where |N == shape.length|. |shape| must be an array of
 * integers. The data for any given index vector |i| is determined by
 * |func(...i)|.
 */
function ParallelArrayBuild(self, shape, func, mode) {
  self.offset = 0;
  self.shape = shape;

  var length;
  var xDimension, yDimension, zDimension;
  var computefunc;

  switch (shape.length) {
  case 1:
    length = shape[0];
    self.get = ParallelArrayGet1;
    computefunc = fill1;
    break;
  case 2:
    xDimension = shape[0];
    yDimension = shape[1];
    length = xDimension * yDimension;
    self.get = ParallelArrayGet2;
    computefunc = fill2;
    break;
  case 3:
    xDimension = shape[0];
    yDimension = shape[1];
    zDimension = shape[2];
    length = xDimension * yDimension * zDimension;
    self.get = ParallelArrayGet3;
    computefunc = fill3;
    break;
  default:
    length = 1;
    for (var i = 0; i < shape.length; i++)
      length *= shape[i];
    self.get = ParallelArrayGetN;
    computefunc = fillN;
    break;
  }

  var buffer = self.buffer = NewDenseArray(length);

  parallel: for (;;) {
    // Avoid parallel compilation if we are already nested in another
    // parallel section or the user told us not to parallelize. The
    // use of a for (;;) loop is working around some ion limitations:
    //
    // - Breaking out of named blocks does not currently work (bug 684384);
    // - Unreachable Code Elim. can't properly handle if (a && b) (bug 669796)
    if (ShouldForceSequential())
      break parallel;
    if (!TRY_PARALLEL(mode))
      break parallel;
    if (computefunc === fillN)
      break parallel;

    var chunks = ComputeNumChunks(length);
    var numSlices = ForkJoinSlices();
    var info = ComputeAllSliceBounds(chunks, numSlices);
    ForkJoin(constructSlice, ForkJoinMode(mode));
    return;
  }

  // Sequential fallback:
  ASSERT_SEQUENTIAL_IS_OK(mode);
  computefunc(0, length);
  return;

  function constructSlice(sliceId, numSlices, warmup) {
    var chunkPos = info[SLICE_POS(sliceId)];
    var chunkEnd = info[SLICE_END(sliceId)];

    if (warmup && chunkEnd > chunkPos)
      chunkEnd = chunkPos + 1;

    while (chunkPos < chunkEnd) {
      var indexStart = chunkPos << CHUNK_SHIFT;
      var indexEnd = std_Math_min(indexStart + CHUNK_SIZE, length);
      computefunc(indexStart, indexEnd);
      UnsafeSetElement(info, SLICE_POS(sliceId), ++chunkPos);
    }

    return chunkEnd == info[SLICE_END(sliceId)];
  }

  function fill1(indexStart, indexEnd) {
    for (var i = indexStart; i < indexEnd; i++)
      UnsafeSetElement(buffer, i, func(i));
  }

  function fill2(indexStart, indexEnd) {
    var x = (indexStart / yDimension) | 0;
    var y = indexStart - x * yDimension;
    for (var i = indexStart; i < indexEnd; i++) {
      UnsafeSetElement(buffer, i, func(x, y));
      if (++y == yDimension) {
        y = 0;
        ++x;
      }
    }
  }

  function fill3(indexStart, indexEnd) {
    var x = (indexStart / (yDimension * zDimension)) | 0;
    var r = indexStart - x * yDimension * zDimension;
    var y = (r / zDimension) | 0;
    var z = r - y * zDimension;
    for (var i = indexStart; i < indexEnd; i++) {
      UnsafeSetElement(buffer, i, func(x, y, z));
      if (++z == zDimension) {
        z = 0;
        if (++y == yDimension) {
          y = 0;
          ++x;
        }
      }
    }
  }

  function fillN(indexStart, indexEnd) {
    var indices = ComputeIndices(shape, indexStart);
    for (var i = indexStart; i < indexEnd; i++) {
      var result = callFunction(std_Function_apply, func, null, indices);
      UnsafeSetElement(buffer, i, result);
      StepIndices(shape, indices);
    }
  }
}

/**
 * Creates a new parallel array by applying |func(e, i, self)| for each
 * element |e| with index |i|. Note that
 * this always operates on the outermost dimension only.
 */
function ParallelArrayMap(func, mode) {
  // FIXME(bug 844887): Check |this instanceof ParallelArray|
  // FIXME(bug 844887): Check |IsCallable(func)|

  var self = this;
  var length = self.shape[0];
  var buffer = NewDenseArray(length);

  parallel: for (;;) { // see ParallelArrayBuild() to explain why for(;;) etc
    if (ShouldForceSequential())
      break parallel;
    if (!TRY_PARALLEL(mode))
      break parallel;

    var chunks = ComputeNumChunks(length);
    var numSlices = ForkJoinSlices();
    var info = ComputeAllSliceBounds(chunks, numSlices);
    ForkJoin(mapSlice, ForkJoinMode(mode));
    return NewParallelArray(ParallelArrayView, [length], buffer, 0);
  }

  // Sequential fallback:
  ASSERT_SEQUENTIAL_IS_OK(mode);
  for (var i = 0; i < length; i++) {
    // Note: Unlike JS arrays, parallel arrays cannot have holes.
    var v = func(self.get(i), i, self);
    UnsafeSetElement(buffer, i, v);
  }
  return NewParallelArray(ParallelArrayView, [length], buffer, 0);

  function mapSlice(sliceId, numSlices, warmup) {
    var chunkPos = info[SLICE_POS(sliceId)];
    var chunkEnd = info[SLICE_END(sliceId)];

    if (warmup && chunkEnd > chunkPos + 1)
      chunkEnd = chunkPos + 1;

    while (chunkPos < chunkEnd) {
      var indexStart = chunkPos << CHUNK_SHIFT;
      var indexEnd = std_Math_min(indexStart + CHUNK_SIZE, length);

      for (var i = indexStart; i < indexEnd; i++)
        UnsafeSetElement(buffer, i, func(self.get(i), i, self));

      UnsafeSetElement(info, SLICE_POS(sliceId), ++chunkPos);
    }

    return chunkEnd == info[SLICE_END(sliceId)];
  }
}

/**
 * Reduces the elements in a parallel array's outermost dimension
 * using the given reduction function.
 */
function ParallelArrayReduce(func, mode) {
  // FIXME(bug 844887): Check |this instanceof ParallelArray|
  // FIXME(bug 844887): Check |IsCallable(func)|

  var self = this;
  var length = self.shape[0];

  if (length === 0)
    ThrowError(JSMSG_PAR_ARRAY_REDUCE_EMPTY);

  parallel: for (;;) { // see ParallelArrayBuild() to explain why for(;;) etc
    if (ShouldForceSequential())
      break parallel;
    if (!TRY_PARALLEL(mode))
      break parallel;

    var chunks = ComputeNumChunks(length);
    var numSlices = ForkJoinSlices();
    if (chunks < numSlices)
      break parallel;

    var info = ComputeAllSliceBounds(chunks, numSlices);
    var subreductions = NewDenseArray(numSlices);
    ForkJoin(reduceSlice, ForkJoinMode(mode));
    var accumulator = subreductions[0];
    for (var i = 1; i < numSlices; i++)
      accumulator = func(accumulator, subreductions[i]);
    return accumulator;
  }

  // Sequential fallback:
  ASSERT_SEQUENTIAL_IS_OK(mode);
  var accumulator = self.get(0);
  for (var i = 1; i < length; i++)
    accumulator = func(accumulator, self.get(i));
  return accumulator;

  function reduceSlice(sliceId, numSlices, warmup) {
    var chunkStart = info[SLICE_START(sliceId)];
    var chunkPos = info[SLICE_POS(sliceId)];
    var chunkEnd = info[SLICE_END(sliceId)];

    // (*) This function is carefully designed so that the warmup
    // (which executes with chunkStart === chunkPos) will execute all
    // potential loads and stores. In particular, the warmup run
    // processes two chunks rather than one. Moreover, it stores
    // accumulator into subreductions and then loads it again to
    // ensure that the load is executed during the warmup, as it will
    // certainly be executed during subsequent runs.

    if (warmup && chunkEnd > chunkPos + 2)
      chunkEnd = chunkPos + 2;

    if (chunkStart === chunkPos) {
      var indexPos = chunkStart << CHUNK_SHIFT;
      var accumulator = reduceChunk(self.get(indexPos), indexPos + 1, indexPos + CHUNK_SIZE);

      UnsafeSetElement(subreductions, sliceId, accumulator, // see (*) above
                       info, SLICE_POS(sliceId), ++chunkPos);
    }

    var accumulator = subreductions[sliceId]; // see (*) above

    while (chunkPos < chunkEnd) {
      var indexPos = chunkPos << CHUNK_SHIFT;
      accumulator = reduceChunk(accumulator, indexPos, indexPos + CHUNK_SIZE);
      UnsafeSetElement(subreductions, sliceId, accumulator,
                       info, SLICE_POS(sliceId), ++chunkPos);
    }

    return chunkEnd == info[SLICE_END(sliceId)];
  }

  function reduceChunk(accumulator, from, to) {
    to = std_Math_min(to, length);
    for (var i = from; i < to; i++)
      accumulator = func(accumulator, self.get(i));
    return accumulator;
  }
}

/**
 * |scan()| returns an array [s_0, ..., s_N] where
 * |s_i| is equal to the reduction (as per |reduce()|)
 * of elements |0..i|. This is the generalization
 * of partial sum.
 */
function ParallelArrayScan(func, mode) {
  // FIXME(bug 844887): Check |this instanceof ParallelArray|
  // FIXME(bug 844887): Check |IsCallable(func)|

  var self = this;
  var length = self.shape[0];

  if (length === 0)
    ThrowError(JSMSG_PAR_ARRAY_REDUCE_EMPTY);

  var buffer = NewDenseArray(length);

  parallel: for (;;) { // see ParallelArrayBuild() to explain why for(;;) etc
    if (ShouldForceSequential())
      break parallel;
    if (!TRY_PARALLEL(mode))
      break parallel;

    var chunks = ComputeNumChunks(length);
    var numSlices = ForkJoinSlices();
    if (chunks < numSlices)
      break parallel;
    var info = ComputeAllSliceBounds(chunks, numSlices);

    // Scan slices individually (see comment on phase1()).
    ForkJoin(phase1, ForkJoinMode(mode));

    // Compute intermediates array (see comment on phase2()).
    var intermediates = [];
    var accumulator = buffer[finalElement(0)];
    ARRAY_PUSH(intermediates, accumulator);
    for (var i = 1; i < numSlices - 1; i++) {
      accumulator = func(accumulator, buffer[finalElement(i)]);
      ARRAY_PUSH(intermediates, accumulator);
    }

    // Reset the current position information for each slice, but
    // convert from chunks to indices (see comment on phase2()).
    for (var i = 0; i < numSlices; i++) {
      info[SLICE_POS(i)] = info[SLICE_START(i)] << CHUNK_SHIFT;
      info[SLICE_END(i)] = info[SLICE_END(i)] << CHUNK_SHIFT;
    }
    info[SLICE_END(numSlices - 1)] = std_Math_min(info[SLICE_END(numSlices - 1)], length);

    // Complete each slice using intermediates array (see comment on phase2()).
    ForkJoin(phase2, ForkJoinMode(mode));
    return NewParallelArray(ParallelArrayView, [length], buffer, 0);
  }

  // Sequential fallback:
  ASSERT_SEQUENTIAL_IS_OK(mode);
  scan(self.get(0), 0, length);
  return NewParallelArray(ParallelArrayView, [length], buffer, 0);

  function scan(accumulator, start, end) {
    UnsafeSetElement(buffer, start, accumulator);
    for (var i = start + 1; i < end; i++) {
      accumulator = func(accumulator, self.get(i));
      UnsafeSetElement(buffer, i, accumulator);
    }
    return accumulator;
  }

  /**
   * In phase 1, we divide the source array into |numSlices| slices and
   * compute scan on each slice sequentially as if it were the entire
   * array. This function is responsible for computing one of those
   * slices.
   *
   * So, if we have an array [A,B,C,D,E,F,G,H,I], |numSlices == 3|,
   * and our function |func| is sum, then we would wind up computing a
   * result array like:
   *
   *     [A, A+B, A+B+C, D, D+E, D+E+F, G, G+H, G+H+I]
   *      ^~~~~~~~~~~~^  ^~~~~~~~~~~~^  ^~~~~~~~~~~~~^
   *      Slice 0        Slice 1        Slice 2
   *
   * Read on in phase2 to see what we do next!
   */
  function phase1(sliceId, numSlices, warmup) {
    var chunkStart = info[SLICE_START(sliceId)];
    var chunkPos = info[SLICE_POS(sliceId)];
    var chunkEnd = info[SLICE_END(sliceId)];

    if (warmup && chunkEnd > chunkPos + 2)
      chunkEnd = chunkPos + 2;

    if (chunkPos == chunkStart) {
      // For the first chunk, the accumulator begins as the value in
      // the input at the start of the chunk.
      var indexStart = chunkPos << CHUNK_SHIFT;
      var indexEnd = std_Math_min(indexStart + CHUNK_SIZE, length);
      scan(self.get(indexStart), indexStart, indexEnd);
      UnsafeSetElement(info, SLICE_POS(sliceId), ++chunkPos);
    }

    while (chunkPos < chunkEnd) {
      // For each subsequent chunk, the accumulator begins as the
      // combination of the final value of prev chunk and the value in
      // the input at the start of this chunk. Note that this loop is
      // written as simple as possible, at the cost of an extra read
      // from the buffer per iteration.
      var indexStart = chunkPos << CHUNK_SHIFT;
      var indexEnd = std_Math_min(indexStart + CHUNK_SIZE, length);
      var accumulator = func(buffer[indexStart - 1], self.get(indexStart));
      scan(accumulator, indexStart, indexEnd);
      UnsafeSetElement(info, SLICE_POS(sliceId), ++chunkPos);
    }

    return chunkEnd == info[SLICE_END(sliceId)];
  }

  /**
   * Computes the index of the final element computed by the slice |sliceId|.
   */
  function finalElement(sliceId) {
    var chunkEnd = info[SLICE_END(sliceId)]; // last chunk written by |sliceId| is endChunk - 1
    var indexStart = std_Math_min(chunkEnd << CHUNK_SHIFT, length);
    return indexStart - 1;
  }

  /**
   * After computing the phase1 results, we compute an
   * |intermediates| array. |intermediates[i]| contains the result
   * of reducing the final value from each preceding slice j<i with
   * the final value of slice i. So, to continue our previous
   * example, the intermediates array would contain:
   *
   *   [A+B+C, (A+B+C)+(D+E+F), ((A+B+C)+(D+E+F))+(G+H+I)]
   *
   * Here I have used parenthesization to make clear the order of
   * evaluation in each case.
   *
   *   An aside: currently the intermediates array is computed
   *   sequentially. In principle, we could compute it in parallel,
   *   at the cost of doing duplicate work. This did not seem
   *   particularly advantageous to me, particularly as the number
   *   of slices is typically quite small (one per core), so I opted
   *   to just compute it sequentially.
   *
   * Phase 2 combines the results of phase1 with the intermediates
   * array to produce the final scan results. The idea is to
   * reiterate over each element S[i] in the slice |sliceId|, which
   * currently contains the result of reducing with S[0]...S[i]
   * (where S0 is the first thing in the slice), and combine that
   * with |intermediate[sliceId-1]|, which represents the result of
   * reducing everything in the input array prior to the slice.
   *
   * To continue with our example, in phase 1 we computed slice 1 to
   * be [D, D+E, D+E+F]. We will combine those results with
   * |intermediates[1-1]|, which is |A+B+C|, so that the final
   * result is [(A+B+C)+D, (A+B+C)+(D+E), (A+B+C)+(D+E+F)]. Again I
   * am using parentheses to clarify how these results were reduced.
   *
   * SUBTLE: Because we are mutating |buffer| in place, we have to
   * be very careful about bailouts!  We cannot checkpoint a chunk
   * at a time as we do elsewhere because that assumes it is safe to
   * replay the portion of a chunk which was already processed.
   * Therefore, in this phase, we track the current position at an
   * index granularity, although this requires two memory writes per
   * index.
   */
  function phase2(sliceId, numSlices, warmup) {
    if (sliceId == 0)
      return true; // No work to do for the 0th slice.

    var indexPos = info[SLICE_POS(sliceId)];
    var indexEnd = info[SLICE_END(sliceId)];

    if (warmup)
      indexEnd = std_Math_min(indexEnd, indexPos + CHUNK_SIZE);

    var intermediate = intermediates[sliceId - 1];
    for (; indexPos < indexEnd; indexPos++) {
      UnsafeSetElement(buffer, indexPos, func(intermediate, buffer[indexPos]),
                       info, SLICE_POS(sliceId), indexPos + 1);
    }

    return indexEnd == info[SLICE_END(sliceId)];
  }
}

/**
 * |scatter()| redistributes the elements in the parallel array
 * into a new parallel array.
 *
 * - targets: The index targets[i] indicates where the ith element
 *   should appear in the result.
 *
 * - defaultValue: what value to use for indices in the output array that
 *   are never targeted.
 *
 * - conflictFunc: The conflict function. Used to resolve what
 *   happens if two indices i and j in the source array are targeted
 *   as the same destination (i.e., targets[i] == targets[j]), then
 *   the final result is determined by applying func(targets[i],
 *   targets[j]). If no conflict function is provided, it is an error
 *   if targets[i] == targets[j].
 *
 * - length: length of the output array (if not specified, uses the
 *   length of the input).
 *
 * - mode: internal debugging specification.
 */
function ParallelArrayScatter(targets, defaultValue, conflictFunc, length, mode) {
  // FIXME(bug 844887): Check |this instanceof ParallelArray|
  // FIXME(bug 844887): Check targets is array-like
  // FIXME(bug 844887): Check |IsCallable(conflictFunc)|

  var self = this;

  if (length === undefined)
    length = self.shape[0];

  // The Divide-Scatter-Vector strategy:
  // 1. Slice |targets| array of indices ("scatter-vector") into N
  //    parts.
  // 2. Each of the N threads prepares an output buffer and a
  //    write-log.
  // 3. Each thread scatters according to one of the N parts into its
  //    own output buffer, tracking written indices in the write-log
  //    and resolving any resulting local collisions in parallel.
  // 4. Merge the parts (either in parallel or sequentially), using
  //    the write-logs as both the basis for finding merge-inputs and
  //    for detecting collisions.

  // The Divide-Output-Range strategy:
  // 1. Slice the range of indices [0..|length|-1] into N parts.
  //    Allocate a single shared output buffer of length |length|.
  // 2. Each of the N threads scans (the entirety of) the |targets|
  //    array, seeking occurrences of indices from that thread's part
  //    of the range, and writing the results into the shared output
  //    buffer.
  // 3. Since each thread has its own portion of the output range,
  //    every collision that occurs can be handled thread-locally.

  // SO:
  //
  // If |targets.length| >> |length|, Divide-Scatter-Vector seems like
  // a clear win over Divide-Output-Range, since for the latter, the
  // expense of redundantly scanning the |targets| will diminish the
  // gain from processing |length| in parallel, while for the former,
  // the total expense of building separate output buffers and the
  // merging post-process is small compared to the gain from
  // processing |targets| in parallel.
  //
  // If |targets.length| << |length|, then Divide-Output-Range seems
  // like it *could* win over Divide-Scatter-Vector. (But when is
  // |targets.length| << |length| or even |targets.length| < |length|?
  // Seems like an odd situation and an uncommon case at best.)
  //
  // The unanswered question is which strategy performs better when
  // |targets.length| approximately equals |length|, especially for
  // special cases like collision-free scatters and permutations.

  if (targets.length >>> 0 !== targets.length)
    ThrowError(JSMSG_BAD_ARRAY_LENGTH, ".prototype.scatter");

  var targetsLength = std_Math_min(targets.length, self.length);

  if (length >>> 0 !== length)
    ThrowError(JSMSG_BAD_ARRAY_LENGTH, ".prototype.scatter");

  parallel: for (;;) { // see ParallelArrayBuild() to explain why for(;;) etc
    if (ShouldForceSequential())
      break parallel;
    if (!TRY_PARALLEL(mode))
      break parallel;

    if (forceDivideScatterVector())
      return parDivideScatterVector();
    else if (forceDivideOutputRange())
      return parDivideOutputRange();
    else if (conflictFunc === undefined && targetsLength < length)
      return parDivideOutputRange();
    return parDivideScatterVector();
  }

  // Sequential fallback:
  ASSERT_SEQUENTIAL_IS_OK(mode);
  return seq();

  function forceDivideScatterVector() {
    return mode && mode.strategy && mode.strategy == "divide-scatter-vector";
  }

  function forceDivideOutputRange() {
    return mode && mode.strategy && mode.strategy == "divide-output-range";
  }

  function collide(elem1, elem2) {
    if (conflictFunc === undefined)
      ThrowError(JSMSG_PAR_ARRAY_SCATTER_CONFLICT);

    return conflictFunc(elem1, elem2);
  }


  function parDivideOutputRange() {
    var chunks = ComputeNumChunks(targetsLength);
    var numSlices = ForkJoinSlices();
    var checkpoints = NewDenseArray(numSlices);
    for (var i = 0; i < numSlices; i++)
      UnsafeSetElement(checkpoints, i, 0);

    var buffer = NewDenseArray(length);
    var conflicts = NewDenseArray(length);

    for (var i = 0; i < length; i++) {
      UnsafeSetElement(buffer, i, defaultValue);
      UnsafeSetElement(conflicts, i, false);
    }

    ForkJoin(fill, ForkJoinMode(mode));
    return NewParallelArray(ParallelArrayView, [length], buffer, 0);

    function fill(sliceId, numSlices, warmup) {
      var indexPos = checkpoints[sliceId];
      var indexEnd = targetsLength;
      if (warmup)
        indexEnd = std_Math_min(indexEnd, indexPos + CHUNK_SIZE);

      // Range in the output for which we are responsible:
      var [outputStart, outputEnd] = ComputeSliceBounds(length, sliceId, numSlices);

      for (; indexPos < indexEnd; indexPos++) {
        var x = self.get(indexPos);
        var t = checkTarget(indexPos, targets[indexPos]);
        if (t < outputStart || t >= outputEnd)
          continue;
        if (conflicts[t])
          x = collide(x, buffer[t]);
        UnsafeSetElement(buffer, t, x,
                         conflicts, t, true,
                         checkpoints, sliceId, indexPos + 1);
      }

      return indexEnd == targetsLength;
    }
  }

  function parDivideScatterVector() {
    // Subtle: because we will be mutating the localBuffers and
    // conflict arrays in place, we can never replay an entry in the
    // target array for fear of inducing a conflict where none existed
    // before. Therefore, we must proceed not by chunks but rather by
    // individual indices.
    var numSlices = ForkJoinSlices();
    var info = ComputeAllSliceBounds(targetsLength, numSlices);

    // FIXME(bug 844890): Use typed arrays here.
    var localBuffers = NewDenseArray(numSlices);
    for (var i = 0; i < numSlices; i++)
      UnsafeSetElement(localBuffers, i, NewDenseArray(length));
    var localConflicts = NewDenseArray(numSlices);
    for (var i = 0; i < numSlices; i++) {
      var conflicts_i = NewDenseArray(length);
      for (var j = 0; j < length; j++)
        UnsafeSetElement(conflicts_i, j, false);
      UnsafeSetElement(localConflicts, i, conflicts_i);
    }

    // Initialize the 0th buffer, which will become the output. For
    // the other buffers, we track which parts have been written to
    // using the conflict buffer so they do not need to be
    // initialized.
    var outputBuffer = localBuffers[0];
    for (var i = 0; i < length; i++)
      UnsafeSetElement(outputBuffer, i, defaultValue);

    ForkJoin(fill, ForkJoinMode(mode));
    mergeBuffers();
    return NewParallelArray(ParallelArrayView, [length], outputBuffer, 0);

    function fill(sliceId, numSlices, warmup) {
      var indexPos = info[SLICE_POS(sliceId)];
      var indexEnd = info[SLICE_END(sliceId)];
      if (warmup)
        indexEnd = std_Math_min(indexEnd, indexPos + CHUNK_SIZE);

      var localbuffer = localBuffers[sliceId];
      var conflicts = localConflicts[sliceId];
      while (indexPos < indexEnd) {
        var x = self.get(indexPos);
        var t = checkTarget(indexPos, targets[indexPos]);
        if (conflicts[t])
          x = collide(x, localbuffer[t]);
        UnsafeSetElement(localbuffer, t, x,
                         conflicts, t, true,
                         info, SLICE_POS(sliceId), ++indexPos);
      }

      return indexEnd == info[SLICE_END(sliceId)];
    }

    /**
     * Merge buffers 1..NUMSLICES into buffer 0. In principle, we could
     * parallelize the merge work as well. But for this first cut,
     * just do the merge sequentially.
     */
    function mergeBuffers() {
      var buffer = localBuffers[0];
      var conflicts = localConflicts[0];
      for (var i = 1; i < numSlices; i++) {
        var otherbuffer = localBuffers[i];
        var otherconflicts = localConflicts[i];
        for (var j = 0; j < length; j++) {
          if (otherconflicts[j]) {
            if (conflicts[j]) {
              buffer[j] = collide(otherbuffer[j], buffer[j]);
            } else {
              buffer[j] = otherbuffer[j];
              conflicts[j] = true;
            }
          }
        }
      }
    }
  }

  function seq() {
    var buffer = NewDenseArray(length);
    var conflicts = NewDenseArray(length);

    for (var i = 0; i < length; i++) {
      UnsafeSetElement(buffer, i, defaultValue);
      UnsafeSetElement(conflicts, i, false);
    }

    for (var i = 0; i < targetsLength; i++) {
      var x = self.get(i);
      var t = checkTarget(i, targets[i]);
      if (conflicts[t])
        x = collide(x, buffer[t]);

      UnsafeSetElement(buffer, t, x,
                       conflicts, t, true);
    }

    return NewParallelArray(ParallelArrayView, [length], buffer, 0);
  }

  function checkTarget(i, t) {
    if (TO_INT32(t) !== t)
      ThrowError(JSMSG_PAR_ARRAY_SCATTER_BAD_TARGET, i);

    if (t < 0 || t >= length)
      ThrowError(JSMSG_PAR_ARRAY_SCATTER_BOUNDS);

    // It's not enough to return t, as -0 | 0 === -0.
    return TO_INT32(t);
  }
}

/**
 * The familiar filter() operation applied across the outermost
 * dimension.
 */
function ParallelArrayFilter(func, mode) {
  // FIXME(bug 844887): Check |this instanceof ParallelArray|
  // FIXME(bug 844887): Check |IsCallable(func)|

  var self = this;
  var length = self.shape[0];

  parallel: for (;;) { // see ParallelArrayBuild() to explain why for(;;) etc
    if (ShouldForceSequential())
      break parallel;
    if (!TRY_PARALLEL(mode))
      break parallel;

    var chunks = ComputeNumChunks(length);
    var numSlices = ForkJoinSlices();
    if (chunks < numSlices * 2)
      break parallel;

    var info = ComputeAllSliceBounds(chunks, numSlices);

    // Step 1. Compute which items from each slice of the result
    // buffer should be preserved. When we're done, we have an array
    // |survivors| containing a bitset for each chunk, indicating
    // which members of the chunk survived. We also keep an array
    // |counts| containing the total number of items that are being
    // preserved from within one slice.
    //
    // FIXME(bug 844890): Use typed arrays here.
    var counts = NewDenseArray(numSlices);
    for (var i = 0; i < numSlices; i++)
      UnsafeSetElement(counts, i, 0);
    var survivors = NewDenseArray(chunks);
    ForkJoin(findSurvivorsInSlice, ForkJoinMode(mode));

    // Step 2. Compress the slices into one contiguous set.
    var count = 0;
    for (var i = 0; i < numSlices; i++)
      count += counts[i];
    var buffer = NewDenseArray(count);
    if (count > 0)
      ForkJoin(copySurvivorsInSlice, ForkJoinMode(mode));

    return NewParallelArray(ParallelArrayView, [count], buffer, 0);
  }

  // Sequential fallback:
  ASSERT_SEQUENTIAL_IS_OK(mode);
  var buffer = [];
  for (var i = 0; i < length; i++) {
    var elem = self.get(i);
    if (func(elem, i, self))
      ARRAY_PUSH(buffer, elem);
  }
  return NewParallelArray(ParallelArrayView, [buffer.length], buffer, 0);

  /**
   * As described above, our goal is to determine which items we
   * will preserve from a given slice. We do this one chunk at a
   * time. When we finish a chunk, we record our current count and
   * the next chunk sliceId, lest we should bail.
   */
  function findSurvivorsInSlice(sliceId, numSlices, warmup) {
    var chunkPos = info[SLICE_POS(sliceId)];
    var chunkEnd = info[SLICE_END(sliceId)];

    if (warmup && chunkEnd > chunkPos)
      chunkEnd = chunkPos + 1;

    var count = counts[sliceId];
    while (chunkPos < chunkEnd) {
      var indexStart = chunkPos << CHUNK_SHIFT;
      var indexEnd = std_Math_min(indexStart + CHUNK_SIZE, length);
      var chunkBits = 0;

      for (var bit = 0; indexStart + bit < indexEnd; bit++) {
        var keep = !!func(self.get(indexStart + bit), indexStart + bit, self);
        chunkBits |= keep << bit;
        count += keep;
      }

      UnsafeSetElement(survivors, chunkPos, chunkBits,
                       counts, sliceId, count,
                       info, SLICE_POS(sliceId), ++chunkPos);
    }

    return chunkEnd == info[SLICE_END(sliceId)];
  }

  function copySurvivorsInSlice(sliceId, numSlices, warmup) {
    // Copies the survivors from this slice into the correct position.
    // Note that this is an idempotent operation that does not invoke
    // user code. Therefore, we don't expect bailouts and make an
    // effort to proceed chunk by chunk or avoid duplicating work.

    // Total up the items preserved by previous slices.
    var count = 0;
    if (sliceId > 0) { // FIXME(#819219)---work around a bug in Ion's range checks
      for (var i = 0; i < sliceId; i++)
        count += counts[i];
    }

    // Compute the final index we expect to write.
    var total = count + counts[sliceId];
    if (count == total)
      return true;

    // Iterate over the chunks assigned to us. Read the bitset for
    // each chunk. Copy values where a 1 appears until we have
    // written all the values that we expect to. We can just iterate
    // from 0...CHUNK_SIZE without fear of a truncated final chunk
    // because we are already checking for when count==total.
    var chunkStart = info[SLICE_START(sliceId)];
    var chunkEnd = info[SLICE_END(sliceId)];
    for (var chunk = chunkStart; chunk < chunkEnd; chunk++) {
      var chunkBits = survivors[chunk];
      if (!chunkBits)
        continue;

      var indexStart = chunk << CHUNK_SHIFT;
      for (var i = 0; i < CHUNK_SIZE; i++) {
        if (chunkBits & (1 << i)) {
          UnsafeSetElement(buffer, count++, self.get(indexStart + i));
          if (count == total)
            break;
        }
      }
    }

    return true;
  }
}

/**
 * Divides the outermost dimension into two dimensions. Does not copy
 * or affect the underlying data, just how it is divided amongst
 * dimensions. So if we had a vector with shape [N, ...] and you
 * partition with amount=4, you get a [N/4, 4, ...] vector. Note that
 * N must be evenly divisible by 4 in that case.
 */
function ParallelArrayPartition(amount) {
  if (amount >>> 0 !== amount)
    ThrowError(JSMSG_PAR_ARRAY_BAD_ARG, "");

  var length = this.shape[0];
  var partitions = (length / amount) | 0;

  if (partitions * amount !== length)
    ThrowError(JSMSG_PAR_ARRAY_BAD_PARTITION);

  var shape = [partitions, amount];
  for (var i = 1; i < this.shape.length; i++)
    ARRAY_PUSH(shape, this.shape[i]);
  return NewParallelArray(ParallelArrayView, shape, this.buffer, this.offset);
}

/**
 * Collapses two outermost dimensions into one. So if you had
 * a [X, Y, ...] vector, you get a [X*Y, ...] vector.
 */
function ParallelArrayFlatten() {
  if (this.shape.length < 2)
    ThrowError(JSMSG_PAR_ARRAY_ALREADY_FLAT);

  var shape = [this.shape[0] * this.shape[1]];
  for (var i = 2; i < this.shape.length; i++)
    ARRAY_PUSH(shape, this.shape[i]);
  return NewParallelArray(ParallelArrayView, shape, this.buffer, this.offset);
}

//
// Accessors and utilities.
//

/**
 * Specialized variant of get() for one-dimensional case
 */
function ParallelArrayGet1(i) {
  if (i === undefined)
    return undefined;
  return this.buffer[this.offset + i];
}

/**
 * Specialized variant of get() for two-dimensional case
 */
function ParallelArrayGet2(x, y) {
  var xDimension = this.shape[0];
  var yDimension = this.shape[1];
  if (x === undefined)
    return undefined;
  if (x >= xDimension)
    return undefined;
  if (y === undefined)
    return NewParallelArray(ParallelArrayView, [yDimension], this.buffer, this.offset + x * yDimension);
  if (y >= yDimension)
    return undefined;
  var offset = y + x * yDimension;
  return this.buffer[this.offset + offset];
}

/**
 * Specialized variant of get() for three-dimensional case
 */
function ParallelArrayGet3(x, y, z) {
  var xDimension = this.shape[0];
  var yDimension = this.shape[1];
  var zDimension = this.shape[2];
  if (x === undefined)
    return undefined;
  if (x >= xDimension)
    return undefined;
  if (y === undefined)
    return NewParallelArray(ParallelArrayView, [yDimension, zDimension],
                            this.buffer, this.offset + x * yDimension * zDimension);
  if (y >= yDimension)
    return undefined;
  if (z === undefined)
    return NewParallelArray(ParallelArrayView, [zDimension],
                            this.buffer, this.offset + y * zDimension + x * yDimension * zDimension);
  if (z >= zDimension)
    return undefined;
  var offset = z + y*zDimension + x * yDimension * zDimension;
  return this.buffer[this.offset + offset];
}

/**
 * Generalized version of get() for N-dimensional case
 */
function ParallelArrayGetN(...coords) {
  if (coords.length == 0)
    return undefined;

  var products = ComputeProducts(this.shape);

  // Compute the offset of the given coordinates. Each index is
  // multipled by its corresponding entry in the |products|
  // array, counting in reverse. So if |coords| is [a,b,c,d],
  // then you get |a*BCD + b*CD + c*D + d|.
  var offset = this.offset;
  var sDimensionality = this.shape.length;
  var cDimensionality = coords.length;
  for (var i = 0; i < cDimensionality; i++) {
    if (coords[i] >= this.shape[i])
      return undefined;
    offset += coords[i] * products[sDimensionality - i - 1];
  }

  if (cDimensionality < sDimensionality) {
    var shape = callFunction(std_Array_slice, this.shape, cDimensionality);
    return NewParallelArray(ParallelArrayView, shape, this.buffer, offset);
  }
  return this.buffer[offset];
}

/** The length property yields the outermost dimension */
function ParallelArrayLength() {
  return this.shape[0];
}

function ParallelArrayToString() {
  var l = this.length;
  if (l == 0)
    return "";

  var open, close;
  if (this.shape.length > 1) {
    open = "<";
    close = ">";
  } else {
    open = close = "";
  }

  var result = "";
  for (var i = 0; i < l - 1; i++) {
    result += open + String(this.get(i)) + close;
    result += ",";
  }
  result += open + String(this.get(l - 1)) + close;
  return result;
}

/**
 * Internal debugging tool: checks that the given `mode` permits
 * sequential execution
 */
function AssertSequentialIsOK(mode) {
  if (mode && mode.mode && mode.mode !== "seq" && ParallelTestsShouldPass())
    ThrowError(JSMSG_WRONG_VALUE, "parallel execution", "sequential was forced");
}

function ForkJoinMode(mode) {
  // WARNING: this must match the enum ForkJoinMode in ForkJoin.cpp
  if (!mode || !mode.mode) {
    return 0;
  } else if (mode.mode === "compile") {
    return 1;
  } else if (mode.mode === "par") {
    return 2;
  } else if (mode.mode === "recover") {
    return 3;
  } else if (mode.mode === "bailout") {
    return 4;
  } else {
    ThrowError(JSMSG_PAR_ARRAY_BAD_ARG, "");
  }
}

/*
 * Mark the main operations as clone-at-callsite for better precision.
 * This is slightly overkill, as all that we really need is to
 * specialize to the receiver and the elemental function, but in
 * practice this is likely not so different, since element functions
 * are often used in exactly one place.
 */
SetScriptHints(ParallelArrayConstructEmpty, { cloneAtCallsite: true });
SetScriptHints(ParallelArrayConstructFromArray, { cloneAtCallsite: true });
SetScriptHints(ParallelArrayConstructFromFunction, { cloneAtCallsite: true });
SetScriptHints(ParallelArrayConstructFromFunctionMode, { cloneAtCallsite: true });
SetScriptHints(ParallelArrayConstructFromComprehension, { cloneAtCallsite: true });
SetScriptHints(ParallelArrayView,       { cloneAtCallsite: true });
SetScriptHints(ParallelArrayBuild,      { cloneAtCallsite: true });
SetScriptHints(ParallelArrayMap,        { cloneAtCallsite: true });
SetScriptHints(ParallelArrayReduce,     { cloneAtCallsite: true });
SetScriptHints(ParallelArrayScan,       { cloneAtCallsite: true });
SetScriptHints(ParallelArrayScatter,    { cloneAtCallsite: true });
SetScriptHints(ParallelArrayFilter,     { cloneAtCallsite: true });

/*
 * Mark the common getters as clone-at-callsite and inline. This is
 * overkill as we should only clone per receiver, but we have no
 * mechanism for that right now. Bug 804767 might permit another
 * alternative by specializing the inlined gets.
 */
SetScriptHints(ParallelArrayGet1,       { cloneAtCallsite: true, inline: true });
SetScriptHints(ParallelArrayGet2,       { cloneAtCallsite: true, inline: true });
SetScriptHints(ParallelArrayGet3,       { cloneAtCallsite: true, inline: true });
