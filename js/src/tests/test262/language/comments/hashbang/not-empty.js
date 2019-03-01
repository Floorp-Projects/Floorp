#! these characters should be treated as a comment
/*---
esid: pending
description: >
    Hashbang comments should be allowed in Scripts and should not be required to be empty.
info: |
    HashbangComment::
      #! SingleLineCommentChars[opt]
flags: [raw]
features: [hashbang]
---*/

reportCompare(0, 0);
