/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;
const { Services } = Cu.import('resource://gre/modules/Services.jsm');
const { SystemAppProxy } = Cu.import('resource://gre/modules/SystemAppProxy.jsm');

addMessageListener('init-chrome-event', function(message) {
  // listen mozChromeEvent and forward to content process.
  let type = message.type;

  SystemAppProxy.addEventListener('mozChromeEvent', function(event) {
    let details = event.detail;
    if (details.type === type) {
      sendAsyncMessage('chrome-event', details);
    }
  }, true);
});
