Components.utils.import("resource://gre/modules/Services.jsm");
let cm = Services.crashmanager;
var cpIdList = [];

var shutdownObs = {
  observe: function(subject, topic, data) {
    if (topic == 'ipc:content-shutdown') {
      var index = cpIdList.indexOf(parseInt(data));
      if (index  > -1) {
        cpIdList.splice(index, 1);
        sendAsyncMessage('content-shutdown', '');
      }
    }
  }
};

var createdObs = {
  observe: function(subject, topic, data) {
    if (topic == 'ipc:content-created') {
      cpIdList.push(parseInt(data));
      sendAsyncMessage('content-created', '');
    }
  }
};

addMessageListener('crashreporter-status', function(message) {
  sendAsyncMessage('crashreporter-enable', !!cm);
});

Services.obs.addObserver(shutdownObs, 'ipc:content-shutdown', false);
Services.obs.addObserver(createdObs, 'ipc:content-created', false);
