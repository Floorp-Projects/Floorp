/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var sandbox = evalcx('');
var foreign = evalcx('({ get f() this, set x(v) { result = this } })', sandbox);
var local = Object.create(foreign);

reportCompare(local, local.f, "this should be set correctly in getters");
local.x = 42;
reportCompare(local, sandbox.result, "this should be set correctly in setters");
