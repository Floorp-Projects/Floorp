// Test that we can trace a unattached handle.

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

if (!this.hasOwnProperty("TypedObject"))
  quit();

var Object = TypedObject.Object;
var handle0 = Object.handle();
gc();
