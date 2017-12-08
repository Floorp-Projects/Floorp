// |reftest| skip-if(!xulRuntime.shell)
/* -*- Mode: js2; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that a SAB buffer (which is distinct from the SAB object) does not leak
// when a serialization object that is keeping it alive becomes garbage.

if (!this.SharedArrayBuffer || !this.sharedArrayRawBufferCount) {
    reportCompare(true,true);
    quit(0);
}

// Record the count of buffers we have at the outset.
var k = sharedArrayRawBufferCount();

var sab = new SharedArrayBuffer(4096);

// sharedArrayRawBufferCount may return 0 in some configurations; bail out if so.
if (sharedArrayRawBufferCount() == 0) {
    reportCompare(true, true);
    quit(0);
}

// There is now one (more) buffer.
assertEq(sharedArrayRawBufferCount(), 1+k);

// Capture the buffer in a serialization object.
var y = serialize(sab)

// Serializing it did not increase the number of buffers.
assertEq(sharedArrayRawBufferCount(), 1+k);

// The serialization buffer hangs onto the SAB buffer, gc does not reap it even
// if the original variable does not reference it and the SAB object is dead and
// gone.
sab = null;
gc();
gc();
assertEq(sharedArrayRawBufferCount(), 1+k);

// Drop the serialization buffer too, and reap dead objects.  Now the count
// should be zero because the serialization buffer should have been reaped and
// nothing else kept the SAB buffer alive.
y = null;
gc();
gc();

assertEq(sharedArrayRawBufferCount(), 0+k);

reportCompare(true, true);
