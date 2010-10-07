/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var f = function(){};
for (var y in f);
f.j = 0;
Object.defineProperty(f, "k", ({configurable: true}));
delete f.j;
Object.defineProperty(f, "k", ({get: function() {}}));

reportCompare(0, 0, "ok");
