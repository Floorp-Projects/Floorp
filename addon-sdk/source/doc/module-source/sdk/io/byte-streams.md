<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Drew Willcoxon [adw@mozilla.com]  -->
<!-- edited by Noelle Murata [fiveinchpixie@gmail.com]  -->

The `byte-streams` module provides streams for reading and writing bytes.

<api name="ByteReader">
@class
<api name="ByteReader">
@constructor
  Creates a binary input stream that reads bytes from a backing stream.
@param inputStream {stream}
  The backing stream, an <a href="http://mxr.mozilla.org/mozilla-central/
source/xpcom/io/nsIInputStream.idl"><code>nsIInputStream</code></a>.
</api>
<api name="closed">
@property {boolean}
  True if the stream is closed.
</api>

<api name="close">
@method
  Closes both the stream and its backing stream.  If the stream is already
  closed, an exception is thrown.
</api>

<api name="read">
@method
  Reads a string from the stream.  If the stream is closed, an exception is
  thrown.
@param [numBytes] {number}
  The number of bytes to read.  If not given, the remainder of the entire stream
  is read.
@returns {string}
  A string containing the bytes read.  If the stream is at the end, returns the
  empty string.
</api>
</api>

<api name="ByteWriter">
@class
<api name="ByteWriter">
@constructor
  Creates a binary output stream that writes bytes to a backing stream.
@param outputStream {stream}
  The backing stream, an <a href="http://mxr.mozilla.org/mozilla-central/
source/xpcom/io/nsIOutputStream.idl"><code>nsIOutputStream</code></a>.
</api>
<api name="closed">
@property {boolean}
  True if the stream is closed.
</api>
<api name="close">
@method
  Closes both the stream and its backing stream.  If the stream is already
  closed, an exception is thrown.
</api>
<api name="write">
@method
  Writes a string to the stream.  If the stream is closed, an exception is
  thrown.
@param str {string}
  The string to write.
</api>
</api>
