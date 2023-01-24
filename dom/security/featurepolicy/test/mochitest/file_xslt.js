let features = new Map(
  document.featurePolicy
    .features()
    .map(f => [f, document.featurePolicy.getAllowlistForFeature(f)])
);

let fullscreen;
document.documentElement
  .requestFullscreen()
  .then(() => {
    fullscreen = true;
    document.exitFullscreen();
  })
  .catch(() => {
    fullscreen = false;
  })
  .finally(() => {
    parent.postMessage({ fullscreen, features }, "*");
  });
