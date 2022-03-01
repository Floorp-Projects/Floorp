/*
 license: The MIT License, Copyright (c) 2020 YUKI "Piro" Hiroshi
 original:
   https://github.com/piroor/treestyletab/blob/master/webextensions/tests/assert.js
*/
'use strict';

export function is(expected, actual, message = '') {
  if (expected === actual ||
      JSON.stringify(expected) === JSON.stringify(actual))
    return;
  const error = new Error(`AssertionError: ${message || 'unexpected value'}`);
  error.name     = 'AssertionError';
  error.expected = JSON.stringify(expected, null, 2);
  error.actual   = JSON.stringify(actual, null, 2);
  throw error;
}

export function isNot(expectedNot, actual, message = '') {
  if (expectedNot !== actual ||
      JSON.stringify(expectedNot) !== JSON.stringify(actual))
    return;
  const error = new Error(`AssertionError: ${message || 'unexpected same value'}`);
  error.name   = 'AssertionError';
  error.actual = JSON.stringify(actual, null, 2);
  throw error;
}

export function ok(actual, message = '') {
  if (!!actual)
    return;
  const error = new Error(`AssertionError: ${message || 'unexpected non-true value'}`);
  error.name   = 'AssertionError';
  error.actual = actual;
  throw error;
}

export function ng(actual, message = '') {
  if (!actual)
    return;
  const error = new Error(`AssertionError: ${message || 'unexpected non-false value'}`);
  error.name   = 'AssertionError';
  error.actual = actual;
  throw error;
}
