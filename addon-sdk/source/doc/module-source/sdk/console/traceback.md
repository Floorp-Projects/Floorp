<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Atul Varma [atul@mozilla.com]  -->
<!-- edited by Noelle Murata [fiveinchpixie@gmail.com]  -->


The `traceback` module contains functionality similar to
Python's [traceback](http://docs.python.org/library/traceback.html) module.

## JSON Traceback Objects ##

Tracebacks are stored in JSON format. The stack is represented as an
array in which the most recent stack frame is the last element; each
element thus represents a stack frame and has the following keys:

<table>
  <tr>
    <td><code>filename</code></td>
    <td>The name of the file that the stack frame takes place in.</td>
  </tr>
  <tr>
    <td><code>lineNo</code></td>
    <td>The line number is being executed at the stack frame.</td>
  </tr>
  <tr>
    <td><code>funcName</code></td>
    <td>The name of the function being executed at the stack frame, or
    <code>null</code> if the function is anonymous or the stack frame is
    being executed in a top-level script or module.</td>
  </tr>
</table>

<api name="fromException">
@function
  Attempts to extract the traceback from *`exception`*.

@returns {traceback}
  JSON representation of the traceback or `null` if not found.

@param exception {exception}
  exception where exception is an `nsIException`.
</api>

See [nsIException](https://developer.mozilla.org/en/NsIException) for more
information.

<api name="get">
@function

@returns {JSON}
  Returns the JSON representation of the stack at the point that this
  function is called.
</api>

<api name="format">
@function
Given a JSON representation of the stack or an exception instance,
returns a formatted plain text representation of it, similar to
Python's formatted stack tracebacks.  If no argument is provided, the
stack at the point this function is called is used.

@param [tbOrException] {object}
</api>

