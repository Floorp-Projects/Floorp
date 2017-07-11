/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env mozilla/frame-script */

addMessageListener("AboutHome:SearchTriggered", function(msg) {
  sendAsyncMessage("AboutHomeTest:CheckRecordedSearch", msg.data);
});
