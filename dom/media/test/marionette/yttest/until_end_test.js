%(force_hd)s

const resolve = arguments[arguments.length - 1];

// this script is injected by marionette to collect metrics
var video = document.getElementsByTagName("video")[0];
if (!video) {
  return "Can't find the video tag";
}

video.addEventListener("ended", () => {
    %(video_playback_quality)s
    %(debug_info)s
    // Pausing after we get the debug info so
    // we can also look at in/out data in buffers
    video.pause();
  }, {once: true}
);

video.play();
