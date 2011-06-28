/* Parse correctly. */

function assignToClassListStrict(e) {
  "use strict";
  try {
    e.classList = "foo";
    ok(false, "assigning to classList didn't throw");
  } catch (e) { }
}
