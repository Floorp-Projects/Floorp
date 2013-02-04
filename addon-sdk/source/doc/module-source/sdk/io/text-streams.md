<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Drew Willcoxon [adw@mozilla.com]  -->
<!-- edited by Noelle Murata [fiveinchpixie@gmail.com]  -->

The `text-streams` module provides streams for reading and writing text using
particular character encodings.

<api name="TextReader">
@class
<api name="TextReader">
@constructor
  Creates a buffered input stream that reads text from a backing stream using a
  given text encoding.
@param inputStream {stream}
  The backing stream, an [`nsIInputStream`](http://mxr.mozilla.org/mozilla-central/source/xpcom/io/nsIInputStream.idl).
  It must already be opened.
@param [charset] {string}
  `inputStream` is expected to be in the character encoding named by this value.
  If not specified, "UTF-8" is assumed.  See [`nsICharsetConverterManager.idl`](http://mxr.mozilla.org/mozilla-central/source/intl/uconv/idl/nsICharsetConverterManager.idl)
  for documentation on how to determine other valid values for this.
</api>

<api name="closed">
@property {boolean}
  True if the stream is closed.
</api>

<api name="close">
@method
  Closes both the stream and its backing stream.
</api>

<api name="read">
@method
  Reads and returns a string from the stream.  If the stream is closed, an
  exception is thrown.
@param [numChars] {number}
  The number of characters to read.  If not given, the remainder of the stream
  is read.
@returns {string}
  The string read.  If the stream is at the end, the empty string is returned.
</api>

</api>


<api name="TextWriter">
@class
<api name="TextWriter">
@constructor
  Creates a buffered output stream that writes text to a backing stream using a
  given text encoding.
@param outputStream {stream}
  The backing stream, an [`nsIOutputStream`](http://mxr.mozilla.org/mozilla-central/source/xpcom/io/nsIOutputStream.idl).
  It must already be opened.
@param [charset] {string}
  Text will be written to `outputStream` using the character encoding named by
  this value.  If not specified, "UTF-8" is assumed.  See [`nsICharsetConverterManager.idl`](http://mxr.mozilla.org/mozilla-central/source/intl/uconv/idl/nsICharsetConverterManager.idl)
  for documentation on how to determine other valid values for this.
</api>

<api name="closed">
@property {boolean}
  True if the stream is closed.
</api>

<api name="close">
@method
  Flushes the backing stream's buffer and closes both the stream and the backing
  stream.  If the stream is already closed, an exception is thrown.
</api>

<api name="flush">
@method
  Flushes the backing stream's buffer.
</api>

<api name="write">
@method
  Writes a string to the stream.  If the stream is closed, an exception is
  thrown.
@param str {string}
  The string to write.
</api>

<api name="writeAsync">
@method
  Writes a string on a background thread.  After the write completes, the
  backing stream's buffer is flushed, and both the stream and the backing stream
  are closed, also on the background thread.  If the stream is already closed,
  an exception is thrown immediately.
@param str {string}
  The string to write.
@param [callback] {callback}
  *`callback`*, if given, must be a function.  It's called as `callback(error)`
   when the write completes.  `error` is an `Error` object or undefined if there
   was no error.  Inside *`callback`*, `this` is the `TextWriter` object.
</api>
</api>
