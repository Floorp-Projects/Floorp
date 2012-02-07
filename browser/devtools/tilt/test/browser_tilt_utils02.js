/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test() {
  let l10 = TiltUtils.L10n;
  ok(l10, "The TiltUtils.L10n wasn't found.");


  ok(l10.stringBundle,
    "The necessary string bundle wasn't found.");
  is(l10.get(), null,
    "The get() function shouldn't work if no params are passed.");
  is(l10.format(), null,
    "The format() function shouldn't work if no params are passed.");

  is(typeof l10.get("initWebGL.error"), "string",
    "No valid string was returned from a corect name in the bundle.");
  is(typeof l10.format("linkProgram.error", ["error"]), "string",
    "No valid formatted string was returned from a name in the bundle.");
}
