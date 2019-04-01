// |reftest| skip-if((function(){try{eval('c=class{x;}');return(false);}catch{return(true);}})()) -- class-fields-public is not enabled unconditionally
// Copyright (C) 2017 Valerie Young. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: ASI test in field declarations -- computed name interpreted as string index
esid: sec-automatic-semicolon-insertion
features: [class, class-fields-public]
---*/

class C {
  x = "lol"
  [1]
}

var c = new C();

assert.sameValue(c.x, 'o');

reportCompare(0, 0);
