/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We use varying sorts across the self-hosted codebase. All sorts are
// consolidated here to avoid confusion and re-implementation of existing
// algorithms.

// For sorting values with limited range; uint8 and int8.
function CountingSort(array, len, signed) {
    var buffer = new List();
    var min = 0;

    // Map int8 values onto the uint8 range when storing in buffer.
    if (signed)  {
        min = -128;
    }

    for (var i = 0; i  < 256; i++) {
        buffer[i] = 0;
    }

    // Populate the buffer
    for (var i = 0; i < len; i++) {
        var val = array[i];
        buffer[val - min]++
    }

    // Traverse the buffer in order and write back elements to array
    var val = 0;
    for (var i = 0; i < len; i++) {
        // Invariant: sum(buffer[val:]) == len-i
        while (true) {
            if (buffer[val] > 0) {
                array[i] = val + min;
                buffer[val]--;
                break;
            } else {
                val++;
            }
        }
    }
    return array;
}

// For sorting small arrays.
function InsertionSort(array, from, to, comparefn) {
    var item, swap;
    for (var i = from + 1; i <= to; i++) {
        item = array[i];
        for (var j = i - 1; j >= from; j--) {
            swap = array[j];
            if (comparefn(swap, item) <= 0)
                break;
            array[j + 1] = swap;
        }
        array[j + 1] = item;
    }
}

function SwapArrayElements(array, i, j) {
    var swap = array[i];
    array[i] = array[j];
    array[j] = swap;
}

// A helper function for MergeSort.
function Merge(array, start, mid, end, lBuffer, rBuffer, comparefn) {
    var i, j, k;

    var sizeLeft = mid - start + 1;
    var sizeRight =  end - mid;

    // Copy our virtual arrays into separate buffers.
    for (i = 0; i < sizeLeft; i++)
        lBuffer[i] = array[start + i];

    for (j = 0; j < sizeRight; j++)
        rBuffer[j] = array[mid + 1 + j];


    i = 0;
    j = 0;
    k = start;
    while (i < sizeLeft && j < sizeRight) {
        if (comparefn(lBuffer[i], rBuffer[j]) <= 0) {
            _DefineDataProperty(array, k, lBuffer[i]);
            i++;
        } else {
            _DefineDataProperty(array, k, rBuffer[j]);
            j++;
        }
        k++;
    }

    // Empty out any remaining elements in the buffer.
    while (i < sizeLeft) {
        _DefineDataProperty(array, k, lBuffer[i]);
        i++;
        k++;
    }

    while (j < sizeRight) {
        _DefineDataProperty(array, k, rBuffer[j]);
        j++;
        k++;
    }
}

// Helper function for overwriting a sparse array with a
// dense array, filling remaining slots with holes.
function MoveHoles(sparse, sparseLen, dense, denseLen) {
    for (var i in dense)
        _DefineDataProperty(sparse, i, dense[i]);
    for (var j = denseLen; j < sparseLen; j++)
        delete sparse[j];
}

// Iterative, bottom up, mergesort.
function MergeSort(array, len, comparefn) {
    // To save effort we will do all of our work on a dense array,
    // then create holes at the end.
    var denseArray = [];
    var denseLen = 0;
    for (var i = 0; i < len; i++) {
        if (i in array)
            _DefineDataProperty(denseArray, denseLen++, array[i]);
    }
    if (denseLen < 1)
        return array;

    // Insertion sort for small arrays, where "small" is defined by performance
    // testing.
    if (len < 24) {
        InsertionSort(denseArray, 0, denseLen - 1, comparefn);
        MoveHoles(array, len, denseArray, denseLen);
        return array;
    }

    // We do all of our allocating up front
    var lBuffer = new List();
    var rBuffer = new List();

    var mid, end, endOne, endTwo;
    for (var windowSize = 1; windowSize < denseLen; windowSize = 2 * windowSize) {
        for (var start = 0; start < denseLen - 1; start += 2*windowSize) {
            assert(windowSize < denseLen, "The window size is larger than the array denseLength!");
            // The midpoint between the two subarrays.
            mid = start + windowSize - 1;
            // To keep from going over the edge.
            end = start + 2 * windowSize - 1;
            end = end < denseLen - 1 ? end : denseLen - 1;
            // Skip lopsided runs to avoid doing useless work
            if (mid > end)
                continue;
            Merge(denseArray, start, mid, end, lBuffer, rBuffer, comparefn);
        }
    }
    MoveHoles(array, len, denseArray, denseLen);
    return array;
}

// Rearranges the elements in array[from:to + 1] and returns an index j such that:
// - from < j < to
// - each element in array[from:j] is less than or equal to array[j]
// - each element in array[j + 1:to + 1] greater than or equal to array[j].
function Partition(array, from, to, comparefn) {
    assert(to - from >= 3, "Partition will not work with less than three elements");

    var medianIndex = (from + to) >> 1;

    var i = from + 1;
    var j = to;

    SwapArrayElements(array, medianIndex, i);

    // Median of three pivot selection.
    if (comparefn(array[from], array[to]) > 0)
        SwapArrayElements(array, from, to);

    if (comparefn(array[i], array[to]) > 0)
        SwapArrayElements(array, i, to);

    if (comparefn(array[from], array[i]) > 0)
        SwapArrayElements(array, from, i);

    var pivotIndex = i;

    // Hoare partition method.
    for(;;) {
        do i++; while (comparefn(array[i], array[pivotIndex]) < 0);
        do j--; while (comparefn(array[j], array[pivotIndex]) > 0);
        if (i > j)
            break;
        SwapArrayElements(array, i, j);
    }

    SwapArrayElements(array, pivotIndex, j);
    return j;
}

// In-place QuickSort.
function QuickSort(array, len, comparefn) {
    // Managing the stack ourselves seems to provide a small performance boost.
    var stack = new List();
    var top = 0;

    var start = 0;
    var end   = len - 1;

    var pivotIndex, i, j, leftLen, rightLen;

    for (;;) {
        // Insertion sort for the first N elements where N is some value
        // determined by performance testing.
        if (end - start <= 23) {
            InsertionSort(array, start, end, comparefn);
            if (top < 1)
                break;
            end   = stack[--top];
            start = stack[--top];
        } else {
            pivotIndex = Partition(array, start, end, comparefn);

            // Calculate the left and right sub-array lengths and save
            // stack space by directly modifying start/end so that
            // we sort the longest of the two during the next iteration.
            // This reduces the maximum stack size to log2(len).
            leftLen = (pivotIndex - 1) - start;
            rightLen = end - (pivotIndex + 1);

            if (rightLen > leftLen) {
                stack[top++] = start;
                stack[top++] = pivotIndex - 1;
                start = pivotIndex + 1;
            } else {
                stack[top++] = pivotIndex + 1;
                stack[top++] = end;
                end = pivotIndex - 1;
            }

        }
    }
    return array;
}
