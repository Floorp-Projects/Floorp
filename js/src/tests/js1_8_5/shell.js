/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

// NOTE: This only turns on 1.8.5 in shell builds.  The browser requires the
//       futzing in js/src/tests/browser.js (which only turns on 1.8, the most
//       the browser supports).
if (typeof version != 'undefined')
{
  version(185);
}

