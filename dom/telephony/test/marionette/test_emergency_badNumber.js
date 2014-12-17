/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

startTest(function() {
  gDialEmergency("not a valid emergency number")
    .catch(cause => {
      is(cause, "BadNumberError");
      return gCheckAll(null, [], "", [], []);
    })
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
