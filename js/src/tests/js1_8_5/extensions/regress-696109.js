/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Dave Herman <dherman@mozilla.com>
 */

// Bug 696109 - fixed a precedence bug in with/while nodes
try {
    Reflect.parse("with({foo})bar");
    throw new Error("supposed to be a syntax error");
} catch (e if e instanceof SyntaxError) { }
try {
    Reflect.parse("while({foo})bar");
    throw new Error("supposed to be a syntax error");
} catch (e if e instanceof SyntaxError) { }

reportCompare(true, true);
