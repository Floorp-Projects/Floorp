/* eslint-env worker */

onconnect = function(e) {
  let port = e.ports[0];

  let navigatorObj = self.navigator;
  let result = {};

  result.appCodeName = navigatorObj.appCodeName;
  result.appName = navigatorObj.appName;
  result.appVersion = navigatorObj.appVersion;
  result.platform = navigatorObj.platform;
  result.userAgent = navigatorObj.userAgent;
  result.product = navigatorObj.product;
  result.hardwareConcurrency = navigatorObj.hardwareConcurrency;

  port.postMessage(JSON.stringify(result));
  port.start();
};
