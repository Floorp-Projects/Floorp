function playAndPostResult(muted, parent_window) {
  let element = document.createElement("video");
  element.preload = "auto";
  element.muted = muted;
  element.src = "short.mp4";
  element.id = "video";
  document.body.appendChild(element);

  element.play().then(
      () => {
        parent_window.postMessage({played: true}, "*");
      },
      () => {
        parent_window.postMessage({played: false}, "*");
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
  return new Promise((resolve, reject) => {
    SpecialPowers.pushPermissions([{
      'type': autoplayPermission,
      'allow': true,
      'context': document
    }], resolve);
  });
}
