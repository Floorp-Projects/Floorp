"use strict";

this.makeUuid = (function() {

  // generates a v4 UUID
  return function makeUuid() { // eslint-disable-line no-unused-vars
    // get sixteen unsigned 8 bit random values
    var randomValues = window
      .crypto
      .getRandomValues(new Uint8Array(36));

    return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
      var i = Array.prototype.slice.call(arguments).slice(-2)[0]; // grab the `offset` parameter
      var r = randomValues[i] % 16|0, v = c === 'x' ? r : (r & 0x3 | 0x8);
      return v.toString(16);
    });
  };
})();
null;
