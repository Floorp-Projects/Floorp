"use strict";

function handleRequest(request, response) {
  response.setHeader("Content-Type", "application/javascript");

  let content = `onconnect = function(e) {
    let port = e.ports[0];

    let navigatorObj = self.navigator;
    let result = [];

    // Known ways to generate time stamps, in milliseconds
    const timeStampCodes = [
      'performance.now()',
      'new Date().getTime()',
      'new Event("").timeStamp',
      'new File([], "").lastModified',
    ];

    for (let timeStampCode of timeStampCodes) {
      let timeStamp = eval(timeStampCode);

      result.push({
        'name': 'worker ' + timeStampCode,
        'value': timeStamp
      });
    }

    port.postMessage(result);
    port.start();
  };`;

  response.write(content);
}
