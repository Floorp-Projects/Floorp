onmessage = e => {
  let file = e.data.file;
  let port = e.data.port;
  port.postMessage(e.data.message);
};
