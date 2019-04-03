var vpq = video.getVideoPlaybackQuality();
var result = {"currentTime": video.currentTime};
result["creationTime"] = vpq.creationTime;
result["corruptedVideoFrames"] = vpq.corruptedVideoFrames;
result["droppedVideoFrames"] = vpq.droppedVideoFrames;
result["totalVideoFrames"] = vpq.totalVideoFrames;
result["defaultPlaybackRate"] = video.playbackRate;
