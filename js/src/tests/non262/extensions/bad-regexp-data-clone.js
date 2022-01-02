// |reftest| skip-if(!xulRuntime.shell)
// -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

let data = new Uint8Array([
  104,97,108,101,6,0,255,255,95,98,
  0,0,0,0,0,104,97,108,101,9,0,255,
  255,95,98,115,0,0,0,0,0,0,65,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0
]);
let cloneBuffer = serialize(null);
cloneBuffer.clonebuffer = data.buffer;

// One of the bytes above encodes a JS::RegExpFlags, but that byte contains bits
// outside of JS::RegExpFlag::AllFlags and so will trigger an error.
assertThrowsInstanceOf(() => deserialize(cloneBuffer), InternalError);

if (typeof reportCompare === "function")
  reportCompare(0, 0, 'ok');
