/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * This module defines dictonaries that are filled with debug information
 * through GetDebugInfo() calls in the media component. To get the information
 * filled and returned, we have two methods that return promises, one in
 * HTMLMediaElement and one in MediaSource.
 *
 * If you need to add some extra info, there's one dictionary per class,
 * following the pattern <ClassName>DebugInfo, where you can add some fields
 * and fill them in the corresponding GetDebugInfo() call.
 *
 * Below is the structures returned.
 *
 * Used by HTMLMediaElement.GetMozRequestDebugInfo(), see HTMLMediaElement.webidl:
 *
 * HTMLMediaElementDebugInfo
 *   EMEDebugInfo
 *   MediaDecoderDebugInfo
 *     MediaFormatReaderDebugInfo
 *       MediaStateDebugInfo
 *       MediaStateDebugInfo
 *       MediaFrameStats
 *     MediaDecoderStateMachineDebugInfo
 *       MediaDecoderStateMachineDecodingStateDebugInfo
 *       MediaSinkDebugInfo
 *         VideoSinkDebugInfo
 *         AudioSinkDebugInfo
 *         DecodedStreamDebugInfo
 *           DecodedStreamDataDebugInfo
 *     MediaResourceDebugInfo
 *       MediaCacheStreamDebugInfo
 *
 * Used by MediaSource.GetMozDebugReaderData(), see MediaSource.webidl:
 *
 * MediaSourceDecoderDebugInfo
 *  MediaFormatReaderDebugInfo
 *    MediaStateDebugInfo
 *    MediaStateDebugInfo
 *    MediaFrameStats
 *  MediaSourceDemuxerDebugInfo
 *    TrackBuffersManagerDebugInfo
 *    TrackBuffersManagerDebugInfo
 */
dictionary MediaCacheStreamDebugInfo {
  long long streamLength = 0;
  long long channelOffset = 0;
  boolean cacheSuspended = false;
  boolean channelEnded = false;
  long loadID = 0;
};

dictionary MediaResourceDebugInfo {
  MediaCacheStreamDebugInfo cacheStream = {};
};

dictionary MediaDecoderDebugInfo {
  DOMString instance = "";
  unsigned long channels = 0;
  unsigned long rate = 0;
  boolean hasAudio = false;
  boolean hasVideo = false;
  DOMString PlayState = "";
  DOMString containerType = "";
  MediaFormatReaderDebugInfo reader = {};
  MediaDecoderStateMachineDebugInfo stateMachine = {};
  MediaResourceDebugInfo resource = {};
};

dictionary AudioSinkDebugInfo {
  long long startTime = 0;
  long long lastGoodPosition = 0;
  boolean isPlaying = false;
  boolean isStarted = false;
  boolean audioEnded = false;
  unsigned long outputRate = 0;
  long long written = 0;
  boolean hasErrored = false;
  boolean playbackComplete = false;
};

dictionary AudioSinkWrapperDebugInfo {
  boolean isPlaying = false;
  boolean isStarted = false;
  boolean audioEnded = false;
  AudioSinkDebugInfo audioSink = {};
};

dictionary VideoSinkDebugInfo {
  boolean isStarted = false;
  boolean isPlaying = false;
  boolean finished = false;
  long size = 0;
  long long videoFrameEndTime = 0;
  boolean hasVideo = false;
  boolean videoSinkEndRequestExists = false;
  boolean endPromiseHolderIsEmpty = false;
};

dictionary DecodedStreamDataDebugInfo {
  DOMString instance = "";
  long long audioFramesWritten = 0;
  long long streamAudioWritten = 0;
  long long streamVideoWritten = 0;
  long long nextAudioTime = 0;
  long long lastVideoStartTime = 0;
  long long lastVideoEndTime = 0;
  boolean haveSentFinishAudio = false;
  boolean haveSentFinishVideo = false;
};

dictionary DecodedStreamDebugInfo {
  DOMString instance = "";
  long long startTime = 0;
  long long lastOutputTime = 0;
  long playing = 0;
  long long lastAudio = 0;
  boolean audioQueueFinished = false;
  long audioQueueSize = 0;
  DecodedStreamDataDebugInfo data = {};
};

dictionary MediaSinkDebugInfo {
  AudioSinkWrapperDebugInfo audioSinkWrapper = {};
  VideoSinkDebugInfo videoSink = {};
  DecodedStreamDebugInfo decodedStream = {};
};

dictionary MediaDecoderStateMachineDecodingStateDebugInfo {
  boolean isPrerolling = false;
};

dictionary MediaDecoderStateMachineDebugInfo {
  long long duration = 0;
  long long mediaTime = 0;
  long long clock = 0;
  DOMString state = "";
  long playState = 0;
  boolean sentFirstFrameLoadedEvent = false;
  boolean isPlaying = false;
  DOMString audioRequestStatus = "";
  DOMString videoRequestStatus = "";
  long long decodedAudioEndTime = 0;
  long long decodedVideoEndTime = 0;
  boolean audioCompleted = false;
  boolean videoCompleted = false;
  MediaDecoderStateMachineDecodingStateDebugInfo stateObj = {};
  MediaSinkDebugInfo mediaSink = {};
};

dictionary MediaStateDebugInfo {
  boolean needInput = false;
  boolean hasPromise = false;
  boolean waitingPromise = false;
  boolean hasDemuxRequest = false;
  long demuxQueueSize = 0;
  boolean hasDecoder = false;
  double timeTreshold = 0.0;
  boolean timeTresholdHasSeeked = false;
  long long numSamplesInput = 0;
  long long numSamplesOutput = 0;
  long queueSize = 0;
  long pending = 0;
  boolean waitingForData = false;
  long demuxEOS = 0;
  long drainState = 0;
  boolean waitingForKey = false;
  long long lastStreamSourceID = 0;
};

dictionary MediaFrameStats {
  long long droppedDecodedFrames = 0;
  long long droppedSinkFrames = 0;
  long long droppedCompositorFrames = 0;
};

dictionary MediaFormatReaderDebugInfo {
  DOMString videoType = "";
  DOMString videoDecoderName = "";
  long videoWidth = 0;
  long videoHeight = 0;
  double videoRate = 0.0;
  DOMString audioType = "";
  DOMString audioDecoderName = "";
  boolean videoHardwareAccelerated = false;
  long long videoNumSamplesOutputTotal = 0;
  long long videoNumSamplesSkippedTotal = 0;
  long audioChannels = 0;
  double audioRate = 0.0;
  long long audioFramesDecoded = 0;
  MediaStateDebugInfo audioState = {};
  MediaStateDebugInfo videoState = {};
  MediaFrameStats frameStats = {};
};

dictionary BufferRange {
  double start = 0;
  double end = 0;
};

dictionary TrackBuffersManagerDebugInfo {
  DOMString type = "";
  double nextSampleTime = 0.0;
  long numSamples = 0;
  long bufferSize = 0;
  long evictable = 0;
  long nextGetSampleIndex = 0;
  long nextInsertionIndex = 0;
  sequence<BufferRange> ranges = [];
};

dictionary MediaSourceDemuxerDebugInfo {
  TrackBuffersManagerDebugInfo audioTrack = {};
  TrackBuffersManagerDebugInfo videoTrack = {};
};

dictionary MediaSourceDecoderDebugInfo {
  MediaFormatReaderDebugInfo reader = {};
  MediaSourceDemuxerDebugInfo demuxer = {};
};

dictionary EMEDebugInfo {
  DOMString keySystem = "";
  DOMString sessionsInfo = "";
};

dictionary HTMLMediaElementDebugInfo {
  unsigned long compositorDroppedFrames = 0;
  EMEDebugInfo EMEInfo = {};
  MediaDecoderDebugInfo decoder = {};
};
