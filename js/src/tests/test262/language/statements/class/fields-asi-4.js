// |reftest| skip error:SyntaxError -- class-fields-public is not supported
// Copyright (C) 2017 Valerie Young. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: ASI test in field declarations -- error when generator interpreted as multiplication
esid: sec-automatic-semicolon-insertion
features: [class, class-fields-public, generators]
negative:
  phase: early
  type: SyntaxError
---*/

throw "Test262: This statement should not be evaluated.";

class C {
  x = 42
  *gen() {}
}
