/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// FIXME(bug 844882): Parallel array properties should not be exposed.

/* Include the common 1D operations. */
#define PA_LENGTH(a) (a.shape[0])
#define PA_GET(a, i) (a.get(i))
#define PA_NEW(length, buffer, offset) \
  (NewParallelArray(ParallelArrayView, [length], buffer, offset))

#define PA_MAP_NAME     ParallelArrayMap
#define PA_REDUCE_NAME  ParallelArrayReduce
#define PA_SCAN_NAME    ParallelArrayScan
#define PA_SCATTER_NAME ParallelArrayScatter
#define PA_FILTER_NAME  ParallelArrayFilter

#include "ParallelArrayCommonOps.js"

#undef PA_LENGTH
#undef PA_GET
#undef PA_NEW
#undef PA_MAP_NAME
#undef PA_REDUCE_NAME
#undef PA_SCAN_NAME
#undef PA_SCATTER_NAME
#undef PA_FILTER_NAME

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
    var index = TO_INT32(index1d / stride);
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
  var length = TO_UINT32(buffer.length);
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
  // Note that due to how DecompileArg works, it must be called in the nearest
  // builtin frame, and so cannot factored out into
  // ParallelArrayConstructFromComprehension.
  if (!IsCallable(func))
    ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(1, func));
  return ParallelArrayConstructFromComprehension(this, shape, func, undefined);
}

/**
 * Wrapper around |ParallelArrayConstructFromComprehension()| for the
 * case where 3 arguments are supplied.
 */
function ParallelArrayConstructFromFunctionMode(shape, func, mode) {
  // See DecompileArg note above.
  if (!IsCallable(func))
    ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(1, func));
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
  if (typeof shape === "number") {
    var length = TO_UINT32(shape);
    if (length !== shape)
      ThrowError(JSMSG_PAR_ARRAY_BAD_ARG, "");
    ParallelArrayBuild(self, [length], func, mode);
  } else if (!shape || typeof shape.length !== "number") {
    ThrowError(JSMSG_PAR_ARRAY_BAD_ARG, "");
  } else {
    var shape1 = [];
    for (var i = 0, l = shape.length; i < l; i++) {
      var s0 = shape[i];
      var s1 = TO_UINT32(s0);
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
    ForkJoin(constructSlice, CheckParallel(mode));
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
  }

  function fill1(indexStart, indexEnd) {
    for (var i = indexStart; i < indexEnd; i++)
      UnsafeSetElement(buffer, i, func(i));
  }

  function fill2(indexStart, indexEnd) {
    var x = TO_INT32(indexStart / yDimension);
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
    var x = TO_INT32(indexStart / (yDimension * zDimension));
    var r = indexStart - x * yDimension * zDimension;
    var y = TO_INT32(r / zDimension);
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
 * Divides the outermost dimension into two dimensions. Does not copy
 * or affect the underlying data, just how it is divided amongst
 * dimensions. So if we had a vector with shape [N, ...] and you
 * partition with amount=4, you get a [N/4, 4, ...] vector. Note that
 * N must be evenly divisible by 4 in that case.
 */
function ParallelArrayPartition(amount) {
  if (!IS_UINT32(amount))
    ThrowError(JSMSG_PAR_ARRAY_BAD_ARG, "");

  var length = this.shape[0];
  var partitions = TO_INT32(length / amount);

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

/*
 * Mark the comprehension form and friends as clone-at-callsite. See note in
 * ParallelArrayCommonOps.js
 */
SetScriptHints(ParallelArrayConstructEmpty, { cloneAtCallsite: true });
SetScriptHints(ParallelArrayConstructFromArray, { cloneAtCallsite: true });
SetScriptHints(ParallelArrayConstructFromFunction, { cloneAtCallsite: true });
SetScriptHints(ParallelArrayConstructFromFunctionMode, { cloneAtCallsite: true });
SetScriptHints(ParallelArrayConstructFromComprehension, { cloneAtCallsite: true });
SetScriptHints(ParallelArrayView,       { cloneAtCallsite: true });
SetScriptHints(ParallelArrayBuild,      { cloneAtCallsite: true });

/*
 * Mark the common getters as clone-at-callsite and inline. This is
 * overkill as we should only clone per receiver, but we have no
 * mechanism for that right now. Bug 804767 might permit another
 * alternative by specializing the inlined gets.
 */
SetScriptHints(ParallelArrayGet1,       { cloneAtCallsite: true, inline: true });
SetScriptHints(ParallelArrayGet2,       { cloneAtCallsite: true, inline: true });
SetScriptHints(ParallelArrayGet3,       { cloneAtCallsite: true, inline: true });
