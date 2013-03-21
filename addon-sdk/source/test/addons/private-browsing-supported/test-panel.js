'use strict';

const { isWindowPBSupported } = require('sdk/private-browsing/utils');

if (isWindowPBSupported) {
  exports.testRequirePanel = function (assert) {
    try {
  	  require('panel');
    }
    catch(e) {
  	  assert.ok(e.message.match(/Bug 816257/), 'Bug 816257 is mentioned');
      return;
    }
    assert.fail('the panel module should throw an error');
  }
}
