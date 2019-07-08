onmessage = e => {
  fetch(e.data)
    .then(r => r.blob())
    .then(blob => postMessage(blob));
};
