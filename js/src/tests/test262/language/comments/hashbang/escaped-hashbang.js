\u0023\u0021
/*---
esid: pending
description: >
    Hashbang comments should not be allowed to have encoded characters
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
