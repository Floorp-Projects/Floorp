(function(g) {
  'use strict';

  g.trapIdentityEvents = target => {
    var state = {};
    var identityEvents = ['idpassertionerror', 'idpvalidationerror',
                          'identityresult', 'peeridentity'];
    identityEvents.forEach(function(name) {
      target.addEventListener(name, function(e) {
        state[name] = e;
      }, false);
    });
    return state;
  };
}(this));
