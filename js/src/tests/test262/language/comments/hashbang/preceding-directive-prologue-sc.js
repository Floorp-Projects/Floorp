"use strict";
#!
/*---
esid: pending
description: >
    Hashbang comments should only be allowed at start of source texts and should not be preceded by DirectivePrologues.
info: |
    HashbangComment::
      #! SingleLineCommentChars[opt]
flags: [raw]
negative:
  phase: parse
  type: SyntaxError
features: [hashbang]
---*/

throw "Test262: This statement should not be evaluated.";

reportCompare(0, 0);
