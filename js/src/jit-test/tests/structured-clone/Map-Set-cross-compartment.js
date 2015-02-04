/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

// Don't crash
serialize(evalcx("new Set(['x', 'y'])"));
serialize(evalcx("new Map([['x', 1]])"));
