function onmessage(event) {
  throw event.data;
}

function onerror(event) {
  postMessage(event.message);
  event.preventDefault();
}
