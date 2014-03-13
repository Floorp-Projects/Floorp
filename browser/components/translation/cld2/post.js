/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

addOnPreMain(function() {

  onmessage = function(aMsg){

    // Convert the string to an array of UTF8 bytes.
    var encoder = new TextEncoder();
    encoder['encoding'] = "utf-8";
    var utf8Array = encoder['encode'](aMsg.data);

    // Copy the UTF8 byte array to the heap.
    var strLength = utf8Array.length;
    var ptr = Module['_malloc'](strLength + 1);
    var heap = Module['HEAPU8'];
    new Uint8Array(heap.buffer, ptr, strLength).set(utf8Array);
    // Add a \0 at the end of the C string.
    heap[ptr + strLength] = 0;

    var lang = Pointer_stringify(_detectLangCode(ptr));
    var confident = !!Module['ccall']("lastResultReliable", "number");
    postMessage({'language': lang,
                 'confident': confident});

    Module['_free'](ptr);
  };

  postMessage("ready");

});
