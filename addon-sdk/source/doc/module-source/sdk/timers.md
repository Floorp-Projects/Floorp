<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Drew Willcoxon [adw@mozilla.com]  -->
<!-- contributed by Atul Varma [atul@mozilla.com]  -->
<!-- edited by Noelle Murata [fiveinchpixie@gmail.com]  -->
<!-- contributed by Irakli Gozalishvil [gozala@mozilla.com] -->

The `timers` module provides access to web-like timing functionality.

<api name="setTimeout">
@function
  Schedules `callback` to be called in `ms` milliseconds. Any additional
  arguments are passed straight through to the callback.
@returns {integer}
  An ID that can later be used to undo this scheduling, if `callback` hasn't yet
  been called.
@param callback {function}
  Function to be called.
@param ms {integer}
  Interval in milliseconds after which the function will be called.
</api>

<api name="clearTimeout">
@function
  Given an ID returned from `setTimeout()`, prevents the callback with the ID
  from being called (if it hasn't yet been called).
@param ID {integer}
  An ID returned from `setTimeout()`.
</api>

<api name="setInterval">
@function
  Schedules `callback` to be called repeatedly every `ms` milliseconds. Any
  additional arguments are passed straight through to the callback.
@returns {integer}
  An ID that can later be used to unschedule the callback.
@param callback {function}
  Function to be called.
@param ms {integer}
  Interval in milliseconds at which the function will be called.
</api>

<api name="clearInterval">
@function
  Given an ID returned from `setInterval()`, prevents the callback with the ID
  from being called again.
@param ID {integer}
  An ID returned from `setInterval()`.
</api>

