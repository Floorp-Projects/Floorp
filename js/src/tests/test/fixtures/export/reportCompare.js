// Copyright (C) 2017 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: reportCompare
---*/


var a = 42;

reportCompare(0, 0)
reportCompare(0, 0);
reportCompare(0, 0, "ok")
reportCompare(true, true, "ok"); // comment
reportCompare(trueish, true, "ok");

reportCompare ( 0 , 0  ) ;
reportCompare ( 0    , 0, "ok")
reportCompare ( true, /*lol*/true, "ok");

reportCompare(null, null, "test");
reportCompare(true, f instanceof Function);
reportCompare(true, true)
reportCompare(true, true);
reportCompare(true, true, "don't crash");
reportCompare(true,true);
this.reportCompare && reportCompare(0, 0, "ok");
this.reportCompare && reportCompare(true, true);
this.reportCompare && reportCompare(true,true);

reportCompare(42, foo);

    reportCompare(0, 0); // this was a reportCompare Line

reportCompare(
    true,
    true
);
