/* import-globals-from ../../../test/manifest.js */

function playAndPostResult(muted, parent_window) {
  let element = document.createElement("video");
  element.preload = "auto";
  element.muted = muted;
  element.src = "short.mp4";
  element.id = "video";
  document.body.appendChild(element);
  element.play().then(
    () => {
      parent_window.postMessage(
        { played: true, allowedToPlay: element.allowedToPlay },
        "*"
      );
    },
    () => {
      parent_window.postMessage(
        { played: false, allowedToPlay: element.allowedToPlay },
        "*"
      );
    }
  );
}

function nextWindowMessage() {
  return nextEvent(window, "message");
}

function log(msg) {
  var log_pane = document.body;
  log_pane.appendChild(document.createTextNode(msg));
  log_pane.appendChild(document.createElement("br"));
}

const autoplayPermission = "autoplay-media";

async function pushAutoplayAllowedPermission() {
  return SpecialPowers.pushPermissions([
    {
      type: autoplayPermission,
      allow: true,
      context: document,
    },
  ]);
}
