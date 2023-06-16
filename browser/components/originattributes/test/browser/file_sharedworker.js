self.randomValue = Math.random();

/* eslint-env worker */

onconnect = function (e) {
  let port = e.ports[0];
  port.postMessage(self.randomValue);
  port.start();
};
