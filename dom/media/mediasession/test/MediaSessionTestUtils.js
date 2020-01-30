const gMediaSessionActions = [
  "play",
  "pause",
  "previoustrack",
  "nexttrack",
  "stop",
];

function nextWindowMessage() {
  return new Promise(r => (window.onmessage = event => r(event)));
}
