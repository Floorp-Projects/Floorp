self.randomValue = Math.random();

/* global onconnect:true */

onconnect = function(e) {
  let port = e.ports[0];
  port.postMessage(self.randomValue);
  port.start();
};
