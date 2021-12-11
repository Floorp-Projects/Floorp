const gMediaSessionActions = [
  "play",
  "pause",
  "seekbackward",
  "seekforward",
  "previoustrack",
  "nexttrack",
  "skipad",
  "seekto",
  "stop",
];

// gCommands and gResults are used in `test_active_mediasession_within_page.html`
const gCommands = {
  createMainFrameSession: "create-main-frame-session",
  createChildFrameSession: "create-child-frame-session",
  destroyChildFrameSessions: "destroy-child-frame-sessions",
  destroyActiveChildFrameSession: "destroy-active-child-frame-session",
  destroyInactiveChildFrameSession: "destroy-inactive-child-frame-session",
};

const gResults = {
  mainFrameSession: "main-frame-session",
  childFrameSession: "child-session-unchanged",
  childFrameSessionUpdated: "child-session-changed",
};

function nextWindowMessage() {
  return new Promise(r => (window.onmessage = event => r(event)));
}
