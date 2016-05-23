// Seeks to the given time and then removes the SVG document's class to trigger
// a reftest snapshot. If pauseFlag is true, animations will be paused.
function setTimeAndSnapshot(timeInSeconds, pauseFlag) {
  var svg = document.documentElement;
  if (pauseFlag) {
    svg.pauseAnimations();
  }
  svg.setCurrentTime(timeInSeconds);
  svg.removeAttribute("class");
}

// Seeks to the given time and then removes the SVG document's class to trigger
// a reftest snapshot after waiting at least minWaitTimeInSeconds.
function setTimeAndWaitToSnapshot(seekTimeInSeconds, minWaitTimeInSeconds) {
  var svg = document.documentElement;
  svg.setCurrentTime(seekTimeInSeconds);
  var timeToTakeSnapshot =
    window.performance.now() + minWaitTimeInSeconds * 1000;
  requestAnimationFrame(function takeSnapshot(currentTime) {
    if (currentTime > timeToTakeSnapshot) {
      svg.removeAttribute("class");
    } else {
      requestAnimationFrame(takeSnapshot);
    }
  });
}
