/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This file expectes the simpletest environment to be available in the scope
// it is loaded into.
/* eslint-env mozilla/simpletest */

// Just a wrapper around SimpleTest related functions for now.
export const Assert = {
  ok,
  equal: is,
};
