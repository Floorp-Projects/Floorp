<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Drew Willcoxon [adw@mozilla.com]  -->
<!-- contributed by Atul Varma [atul@mozilla.com]  -->
<!-- edited by Noelle Murata [fiveinchpixie@gmail.com]  -->

The `file` module provides access to the local filesystem.

Paths
-----

Path specifications in this API are platform-specific.  This means that on
Windows paths are specified using the backslash path separator (`\`), and on
Unix-like systems like Linux and OS X paths are specified using the forward
slash path separator (`/`).

If your add-on uses literal Windows-style path specifications with this API,
your add-on likely won't work when users run it on Unix-like systems.  Likewise,
if your add-on uses literal Unix-style path specifications, it won't work for
users on Windows.

To ensure your add-on works for everyone, generate paths using the
[`join`](modules/sdk/io/file.html#join(...)) function.  Unfortunately
this API does not currently provide a way to obtain an absolute base path which
you could then use with `join`.  For now, you need to
[`require("chrome")`](dev-guide/tutorials/chrome.html) and use the
XPCOM directory service as described at
[MDN](https://developer.mozilla.org/en/Code_snippets/File_I%2F%2FO#Getting_special_files).

Note that if you do decide to hardcode Windows-style paths, be sure to escape
backslashes in strings.  For example, to specify the file at `C:\Users\Myk`, you
need to use the string `"C:\\Users\\Myk"`, not `"C:\Users\Myk"`.  You can read
more about escaping characters in strings at
[MDN](https://developer.mozilla.org/en/JavaScript/Guide/Values,_Variables,_and_Literals#Escaping_Characters).


<api name="basename">
@function
  Returns the last component of the given path.  For example,
  `basename("/foo/bar/baz")` returns `"baz"`.  If the path has no components,
  the empty string is returned.
@param path {string}
  The path of a file.
@returns {string}
  The last component of the given path.
</api>

<api name="dirname">
@function
  Returns the path of the directory containing the given file.  If the file is
  at the top of the volume, the empty string is returned.
@param path {string}
  The path of a file.
@returns {string}
  The path of the directory containing the file.
</api>

<api name="exists">
@function
  Returns true if a file exists at the given path and false otherwise.
@param path {string}
  The path of a file.
@returns {boolean}
  True if the file exists and false otherwise.
</api>

<api name="join">
@function
  Takes a variable number of strings, joins them on the file system's path
  separator, and returns the result.
@param ... {strings}
  A variable number of strings to join.  The first string must be an absolute
  path.
@returns {string}
  A single string formed by joining the strings on the file system's path
  separator.
</api>

<api name="list">
@function
  Returns an array of file names in the given directory.
@param path {string}
  The path of the directory.
@returns {array}
  An array of file names.  Each is a basename, not a full path.
</api>

<api name="mkpath">
@function
  Makes a new directory named by the given path.  Any subdirectories that do not
  exist are also created.  `mkpath` can be called multiple times on the same
  path.
@param path {string}
  The path to create.
</api>

<api name="open">
@function
  Returns a stream providing access to the contents of a file.
@param path {string}
  The path of the file to open.
@param [mode] {string}
  An optional string, each character of which describes a characteristic of the
  returned stream.  If the string contains `"r"`, the file is opened in
  read-only mode.  `"w"` opens the file in write-only mode.  `"b"` opens the
  file in binary mode.  If `"b"` is not present, the file is opened in text
  mode, and its contents are assumed to be UTF-8.  If *`mode`* is not given,
  `"r"` is assumed, and the file is opened in read-only text mode.
@returns {stream}
  If the file is opened in text read-only `mode`, a `TextReader` is returned,
  and if text write-only mode, a `TextWriter` is returned.  See
  [`text-streams`](modules/sdk/io/text-streams.html) for information on
  these text stream objects.  If the file is opened in binary read-only `mode`,
  a `ByteReader` is returned, and if binary write-only mode, a `ByteWriter` is
  returned.  See
  [`byte-streams`](modules/sdk/io/byte-streams.html) for more
  information on these byte stream objects.  Opened files should always be
  closed after use by calling `close` on the returned stream.
</api>

<api name="read">
@function
  Opens a file and returns a string containing its entire contents.
@param path {string}
  The path of the file to read.
@param [mode] {string}
  An optional string, each character of which describes a characteristic of the
  returned stream.  If the string contains `"b"`, the contents will be returned 
  in binary mode. If `"b"` is not present or `mode` is not given, the file
  contents will be returned in text mode. 
@returns {string}
  A string containing the file's entire contents.
</api>

<api name="remove">
@function
  Removes a file from the file system.  To remove directories, use `rmdir`.
@param path {string}
  The path of the file to remove.
</api>

<api name="rmdir">
@function
  Removes a directory from the file system.  If the directory is not empty, an
  exception is thrown.
@param path {string}
  The path of the directory to remove.
</api>
