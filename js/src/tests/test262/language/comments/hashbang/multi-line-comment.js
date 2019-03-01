#!/*
throw "Test262: This statement should not be evaluated.";
these characters should not be considered within a comment
*/
/*---
esid: pending
description: >
    Hashbang comments should not interpret multi-line comments.
info: |
    HashbangComment::
      #! SingleLineCommentChars[opt]
flags: [raw]
negative:
  phase: parse
  type: SyntaxError
features: [hashbang]
---*/

reportCompare(0, 0);
