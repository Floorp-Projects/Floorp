/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;
const { Services } = Cu.import('resource://gre/modules/Services.jsm');

var GamepadService = (function() {
  return Cc["@mozilla.org/gamepad-test;1"].getService(Ci.nsIGamepadServiceTest);
})();


addMessageListener('add-gamepad', function(message) {
  var index = GamepadService.addGamepad(message.name,
                                        message.mapping,
                                        message.buttons,
                                        message.axes);
  sendAsyncMessage('new-gamepad-index', index);
});

addMessageListener('new-button-event', function(message) {
  GamepadService.newButtonEvent(message.index,
                                message.button,
                                message.status);
});

addMessageListener('new-button-value-event', function(message) {
  GamepadService.newButtonValueEvent(message.index,
                                     message.button,
                                     message.status,
                                     message.value);
});

addMessageListener('remove-gamepad', function(message) {
  GamepadService.removeGamepad(message.index);
});

