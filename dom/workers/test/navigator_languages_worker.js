var active = true;
onmessage = function(e) {
  if (e.data == 'finish') {
    active = false;
    return;
  }

  if (active) {
    postMessage(navigator.languages);
  }
}
