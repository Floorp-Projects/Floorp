// -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// Don't assert trying to parse this.
let(x = (x(( function() { 
                 return { e: function() { e in x } };
             }))
         for (x in [])))
true;

reportCompare(true, true);
