/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

// Don't crash
serialize(evalcx("Set(['x', 'y'])"));
serialize(evalcx("Map([['x', 1]])"));
