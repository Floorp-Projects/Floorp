<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `deprecate` module provides helper functions to deprecate code.

<api name="deprecateFunction">
@function
  Dump to the console the error message given in the second argument,
  prefixed with `"DEPRECATED:"`, and print the stacktrace; then execute the
  function passed as first argument and returns its value.

  It does not raise an exception, but just displays the error message and
  continues to execute the function.

  This function is mostly used to flag usage of deprecated functions, that are
  still available but which we intend to remove in a future release.

@param fun {function}
  The function to execute after the error message
@param msg {string}
  The error message to display

@returns {*} The returned value from `fun`
</api>

<api name="deprecateUsage">
@function
  Dump to the console the error message given, prefixed with `"DEPRECATED:"`,
  and print the stacktrace.

  It does not raise an exception, but just displays the error message and
  returns.

  This function is mostly used to flag usage of deprecated functions that are no
  longer available.

@param msg {string}
  The error message to display
</api>
