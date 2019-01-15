// |reftest| error:SyntaxError
/*---
esid: pending
description: >
    Hashbang comments should only be allowed at the start of source texts and should not be allowed within function bodies.
info: |
    HashbangComment::
      #! SingleLineCommentChars[opt]
negative:
  phase: parse
  type: SyntaxError
features: [hashbang]
---*/

$DONOTEVALUATE();

function fn() {#!
}
