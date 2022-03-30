"use strict";

/**
 * Bug 1761930 - Undefine ReadableStream.prototype.pipeTo for Instagram
 *
 * Instagram's ReadableStream feature detection only checks if .pipeTo exists,
 * and if it does, it assumes .pipeThrough exists as well. Unfortuantely, that
 * assumption is not yet true in Firefox, and thus, some video playbacks fail.
 */

console.info(
  "ReadableStream.prototype.pipeTo has been overwritten for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1761343 for details."
);

Object.defineProperty(ReadableStream.prototype.wrappedJSObject, "pipeTo", {
  get: undefined,
  set: undefined,
});
