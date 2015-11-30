let port;
navigator.mozSetMessageHandler('connection', request => {
  port = request.port;
  port.onmessage = () => {
    port.postMessage('response');
    port.close();
  };
});
alert('READY');
