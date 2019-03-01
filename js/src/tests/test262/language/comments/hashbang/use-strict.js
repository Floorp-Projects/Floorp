#!"use strict"
/*---
esid: pending
description: >
    Hashbang comments should not be interpreted and should not generate DirectivePrologues.
info: |
    HashbangComment::
      #! SingleLineCommentChars[opt]
flags: [raw, noStrict]
features: [hashbang]
---*/

with ({}) {}

reportCompare(0, 0);
