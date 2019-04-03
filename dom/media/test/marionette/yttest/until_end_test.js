%(force_hd)s

const resolve = arguments[arguments.length - 1];

// this script is injected by marionette to collect metrics
var video = document.getElementsByTagName("video")[0];
if (!video) {
  return "Can't find the video tag";
}

video.addEventListener("ended", () => {
    video.pause();
    %(video_playback_quality)s
    %(debug_info)s
  }, {once: true}
);

video.play();
