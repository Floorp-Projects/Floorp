/*---
esid: pending
description: >
    Hashbang comments should be available in Script evaluator contexts. (direct eval)
info: |
    HashbangComment::
      #! SingleLineCommentChars[opt]
features: [hashbang]
---*/

assert.sameValue(eval('#!\n'), undefined);
assert.sameValue(eval('#!\n1'), 1)
assert.sameValue(eval('#!2\n'), undefined);

reportCompare(0, 0);
