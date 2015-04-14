// In each list of tests below, test file types that are not supported should
// be ignored. To make sure tests respect that, we include a file of type
// "bogus/duh" in each list.

// These are small test files, good for just seeing if something loads. We
// really only need one test file per backend here.
var gSmallTests = [
  { name:"small-shot.ogg", type:"audio/ogg", duration:0.276 },
  { name:"small-shot.m4a", type:"audio/mp4", duration:0.29 },
  { name:"small-shot.mp3", type:"audio/mpeg", duration:0.27 },
  { name:"small-shot-mp3.mp4", type:"audio/mp4; codecs=mp3", duration:0.34 },
  { name:"small-shot.flac", type:"audio/flac", duration:0.197 },
  { name:"r11025_s16_c1.wav", type:"audio/x-wav", duration:1.0 },
  { name:"320x240.ogv", type:"video/ogg", width:320, height:240, duration:0.266 },
  { name:"seek.webm", type:"video/webm", width:320, height:240, duration:3.966 },
  { name:"vp9.webm", type:"video/webm", width:320, height:240, duration:4 },
  { name:"detodos.opus", type:"audio/ogg; codecs=opus", duration:2.9135 },
  { name:"gizmo.mp4", type:"video/mp4", width:560, height:320, duration:5.56 },
  { name:"bogus.duh", type:"bogus/duh" }
];

if (SpecialPowers.Services.appinfo.name != "B2G") {
  // We only run mochitests on b2g desktop and b2g emulator. The 3gp codecs
  // aren't present on desktop, and the emulator codecs (which are different
  // from the real device codecs) don't pass all of our tests, so we need
  // to disable them.

  gSmallTests = gSmallTests.concat([
    { name:"sample.3gp", type:"video/3gpp", duration:4.933 },
    { name:"sample.3g2", type:"video/3gpp2", duration:4.933 }
  ]);
}

// Used by test_bug654550.html, for videoStats preference
var gVideoTests = [
  { name:"320x240.ogv", type:"video/ogg", width:320, height:240, duration:0.266 },
  { name:"seek.webm", type:"video/webm", width:320, height:240, duration:3.966 },
  { name:"bogus.duh", type:"bogus/duh" }
];

// Used by test_progress to ensure we get the correct progress information
// during resource download.
var gProgressTests = [
  { name:"r11025_u8_c1.wav", type:"audio/x-wav", duration:1.0, size:11069 },
  { name:"big.wav", type:"audio/x-wav", duration:9.278981, size:102444 },
  { name:"seek.ogv", type:"video/ogg", duration:3.966, size:285310 },
  { name:"320x240.ogv", type:"video/ogg", width:320, height:240, duration:0.266, size:28942 },
  { name:"seek.webm", type:"video/webm", duration:3.966, size:215529 },
  { name:"gizmo.mp4", type:"video/mp4", duration:5.56, size:383631 },
  { name:"bogus.duh", type:"bogus/duh" }
];

// Used by test_played.html
var gPlayedTests = [
  { name:"big.wav", type:"audio/x-wav", duration:9.0 },
  { name:"seek.ogv", type:"video/ogg", duration:3.966 },
  { name:"seek.webm", type:"video/webm", duration:3.966 },
  { name:"gizmo.mp4", type:"video/mp4", duration:5.56 },
  { name:"owl.mp3", type:"audio/mpeg", duration:3.29 },
  // Disable vbr.mp3 to see if it reduces the error of AUDCLNT_E_CPUUSAGE_EXCEEDED.
  // See bug 1110922 comment 26.
  //{ name:"vbr.mp3", type:"audio/mpeg", duration:10.0 },
  { name:"bug495794.ogg", type:"audio/ogg", duration:0.3 }
];

// Used by test_mozLoadFrom.  Need one test file per decoder backend, plus
// anything for testing clone-specific bugs.
var cloneKey = Math.floor(Math.random()*100000000);
var gCloneTests = gSmallTests.concat([
  // Actual duration is ~200ms, we have Content-Duration lie about it.
  { name:"bug520908.ogv", type:"video/ogg", duration:9000 },
  // short-video is more like 1s, so if you load this twice you'll get an unexpected duration
  { name:"dynamic_resource.sjs?key=" + cloneKey + "&res1=320x240.ogv&res2=short-video.ogv",
    type:"video/ogg", duration:0.266 },
]);

// Used by test_play_twice.  Need one test file per decoder backend, plus
// anything for testing bugs that occur when replying a played file.
var gReplayTests = gSmallTests.concat([
  { name:"bug533822.ogg", type:"audio/ogg" },
]);

// Used by test_paused_after_ended. Need one test file per decoder backend, plus
// anything for testing bugs that occur when replying a played file.
var gPausedAfterEndedTests = gSmallTests.concat([
  { name:"r11025_u8_c1.wav", type:"audio/x-wav", duration:1.0 },
  { name:"small-shot.ogg", type:"video/ogg", duration:0.276 }
]);

// Test the mozHasAudio property, and APIs that detect different kinds of
// tracks
var gTrackTests = [
  { name:"big.wav", type:"audio/x-wav", duration:9.278981, size:102444, hasAudio:true, hasVideo:false },
  { name:"320x240.ogv", type:"video/ogg", width:320, height:240, duration:0.266, size:28942, hasAudio:false, hasVideo:true },
  { name:"short-video.ogv", type:"video/ogg", duration:1.081, hasAudio:true, hasVideo:true },
  { name:"seek.webm", type:"video/webm", duration:3.966, size:215529, hasAudio:false, hasVideo:true },
  { name:"bogus.duh", type:"bogus/duh" }
];

var gClosingConnectionsTest = [
  { name:"seek.ogv", type:"video/ogg", duration:3.966 }
];

// Used by any media recorder test. Need one test file per decoder backend
// currently supported by the media encoder.
var gMediaRecorderTests = [
  { name:"detodos.opus", type:"audio/ogg; codecs=opus", duration:2.9135 }
];

// These are files that we want to make sure we can play through.  We can
// also check metadata.  Put files of the same type together in this list so if
// something crashes we have some idea of which backend is responsible.
// Used by test_playback, which expects no error event and one ended event.
var gPlayTests = [
  // Test playback of a WebM file with vp9 video
  //{ name:"vp9.webm", type:"video/webm", duration:4 },
  { name:"vp9cake.webm", type:"video/webm", duration:7.966 },

  // 8-bit samples
  { name:"r11025_u8_c1.wav", type:"audio/x-wav", duration:1.0 },
  // 8-bit samples, file is truncated
  { name:"r11025_u8_c1_trunc.wav", type:"audio/x-wav", duration:1.8 },
  // file has trailing non-PCM data
  { name:"r11025_s16_c1_trailing.wav", type:"audio/x-wav", duration:1.0 },
  // file with list chunk
  { name:"r16000_u8_c1_list.wav", type:"audio/x-wav", duration:4.2 },

  // Ogg stream without eof marker
  { name:"bug461281.ogg", type:"application/ogg", duration:2.208 },

  // oggz-chop stream
  { name:"bug482461.ogv", type:"video/ogg", duration:4.34 },
  // Theora only oggz-chop stream
  { name:"bug482461-theora.ogv", type:"video/ogg", duration:4.138 },
  // With first frame a "duplicate" (empty) frame.
  { name:"bug500311.ogv", type:"video/ogg", duration:1.96 },
  // Small audio file
  { name:"small-shot.ogg", type:"audio/ogg", duration:0.276 },
  // More audio in file than video.
  { name:"short-video.ogv", type:"video/ogg", duration:1.081 },
  // First Theora data packet is zero bytes.
  { name:"bug504613.ogv", type:"video/ogg", duration:Number.NaN },
  // Multiple audio streams.
  { name:"bug516323.ogv", type:"video/ogg", duration:4.208 },
  // oggz-chop with non-keyframe as first frame
  { name:"bug556821.ogv", type:"video/ogg", duration:2.551 },

  // Encoded with vorbis beta1, includes unusually sized codebooks
  { name:"beta-phrasebook.ogg", type:"audio/ogg", duration:4.01 },
  // Small file, only 1 frame with audio only.
  { name:"bug520493.ogg", type:"audio/ogg", duration:0.458 },
  // Small file with vorbis comments with 0 length values and names.
  { name:"bug520500.ogg", type:"audio/ogg", duration:0.123 },

  // Various weirdly formed Ogg files
  { name:"bug499519.ogv", type:"video/ogg", duration:0.24 },
  { name:"bug506094.ogv", type:"video/ogg", duration:0 },
  { name:"bug498855-1.ogv", type:"video/ogg", duration:0.24 },
  { name:"bug498855-2.ogv", type:"video/ogg", duration:0.24 },
  { name:"bug498855-3.ogv", type:"video/ogg", duration:0.24 },
  { name:"bug504644.ogv", type:"video/ogg", duration:1.6 },
  { name:"chain.ogv", type:"video/ogg", duration:Number.NaN },
  { name:"bug523816.ogv", type:"video/ogg", duration:0.533 },
  { name:"bug495129.ogv", type:"video/ogg", duration:2.41 },
  { name:"bug498380.ogv", type:"video/ogg", duration:0.533 },
  { name:"bug495794.ogg", type:"audio/ogg", duration:0.3 },
  { name:"bug557094.ogv", type:"video/ogg", duration:0.24 },
  { name:"multiple-bos.ogg", type:"video/ogg", duration:0.431 },
  { name:"audio-overhang.ogg", type:"audio/ogg", duration:2.3 },
  { name:"video-overhang.ogg", type:"audio/ogg", duration:3.966 },

  // bug461281.ogg with the middle second chopped out.
  { name:"audio-gaps.ogg", type:"audio/ogg", duration:2.208 },

  // Test playback/metadata work after a redirect
  { name:"redirect.sjs?domain=mochi.test:8888&file=320x240.ogv",
    type:"video/ogg", duration:0.266 },

  // Test playback of a webm file
  { name:"seek.webm", type:"video/webm", duration:3.966 },

  // Test playback of a WebM file with non-zero start time.
  { name:"split.webm", type:"video/webm", duration:1.967 },

  // Test playback of a raw file
  { name:"seek.yuv", type:"video/x-raw-yuv", duration:1.833 },

  // A really short, low sample rate, single channel file. This tests whether
  // we can handle playing files when only push very little audio data to the
  // hardware.
  { name:"spacestorm-1000Hz-100ms.ogg", type:"audio/ogg", duration:0.099 },

  // Opus data in an ogg container
  { name:"detodos.opus", type:"audio/ogg; codecs=opus", duration:2.9135 },
  // Opus data in a webm container
  { name:"detodos.webm", type:"audio/webm; codecs=opus", duration:2.9135 },
  { name:"bug1066943.webm", type:"audio/webm; codecs=opus", duration:1.383 },

  // Multichannel Opus in an ogg container
  { name:"test-1-mono.opus", type:"audio/ogg; codecs=opus", duration:1.044 },
  { name:"test-2-stereo.opus", type:"audio/ogg; codecs=opus", duration:2.925 },
  { name:"test-3-LCR.opus", type:"audio/ogg; codecs=opus", duration:4.214 },
  { name:"test-4-quad.opus", type:"audio/ogg; codecs=opus", duration:6.234 },
  { name:"test-5-5.0.opus", type:"audio/ogg; codecs=opus", duration:7.558 },
  { name:"test-6-5.1.opus", type:"audio/ogg; codecs=opus", duration:10.333 },
  { name:"test-7-6.1.opus", type:"audio/ogg; codecs=opus", duration:11.690 },
  { name:"test-8-7.1.opus", type:"audio/ogg; codecs=opus", duration:13.478 },

  { name:"gizmo.mp4", type:"video/mp4", duration:5.56 },

  { name:"small-shot.m4a", type:"audio/mp4", duration:0.29 },
  { name:"small-shot.mp3", type:"audio/mpeg", duration:0.27 },
  { name:"owl.mp3", type:"audio/mpeg", duration:3.29 },
  // owl.mp3 as above, but with something funny going on in the ID3v2 tag
  // that causes DirectShow to fail.
  { name:"owl-funny-id3.mp3", type:"audio/mpeg", duration:3.29 },
  // owl.mp3 as above, but with something even funnier going on in the ID3v2 tag
  // that causes DirectShow to fail.
  { name:"owl-funnier-id3.mp3", type:"audio/mpeg", duration:3.29 },
  // One second of silence with ~140KB of ID3 tags. Usually when the first MP3
  // frame is at such a high offset into the file, MP3FrameParser will give up
  // and report that the stream is not MP3. However, it does not count ID3 tags
  // in that offset. This test case makes sure that ID3 exclusion holds.
  { name:"huge-id3.mp3", type:"audio/mpeg", duration:1.00 },
  // A truncated VBR MP3 with just enough frames to keep most decoders happy.
  // The Xing header reports the length of the file to be around 10 seconds, but
  // there is really only one second worth of data. We want MP3FrameParser to
  // trust the header, so this should be reported as 10 seconds.
  { name:"vbr-head.mp3", type:"audio/mpeg", duration:10.00 },

  // Invalid file
  { name:"bogus.duh", type:"bogus/duh", duration:Number.NaN },
];

// A file for each type we can support.
var gSnifferTests = [
  { name:"big.wav", type:"audio/x-wav", duration:9.278981, size:102444 },
  { name:"320x240.ogv", type:"video/ogg", width:320, height:240, duration:0.233, size:28942 },
  { name:"seek.webm", type:"video/webm", duration:3.966, size:215529 },
  { name:"gizmo.mp4", type:"video/mp4", duration:5.56, size:383631 },
  // A mp3 file with id3 tags.
  { name:"id3tags.mp3", type:"audio/mpeg", duration:0.28, size:3530},
  { name:"bogus.duh", type:"bogus/duh" }
];

// Files we must reject as invalid.
var gInvalidTests = [
  { name:"invalid-m0c0.opus", type:"audio/ogg; codecs=opus"},
  { name:"invalid-m0c3.opus", type:"audio/ogg; codecs=opus"},
  { name:"invalid-m1c0.opus", type:"audio/ogg; codecs=opus"},
  { name:"invalid-m1c9.opus", type:"audio/ogg; codecs=opus"},
  { name:"invalid-m2c0.opus", type:"audio/ogg; codecs=opus"},
  { name:"invalid-m2c1.opus", type:"audio/ogg; codecs=opus"},
  { name:"invalid-cmap-short.opus", type:"audio/ogg; codecs=opus"},
  { name:"invalid-cmap-s0c0.opus", type:"audio/ogg; codecs=opus"},
  { name:"invalid-cmap-s0c2.opus", type:"audio/ogg; codecs=opus"},
  { name:"invalid-cmap-s1c2.opus", type:"audio/ogg; codecs=opus"},
  { name:"invalid-preskip.webm", type:"audio/webm; codecs=opus"},
];

var gInvalidPlayTests = [
  { name:"invalid-excess_discard.webm", type:"audio/webm; codecs=opus"},
  { name:"invalid-excess_neg_discard.webm", type:"audio/webm; codecs=opus"},
  { name:"invalid-neg_discard.webm", type:"audio/webm; codecs=opus"},
  { name:"invalid-discard_on_multi_blocks.webm", type:"audio/webm; codecs=opus"},
];

// Files to check different cases of ogg skeleton information.
// sample-fisbone-skeleton4.ogv
// - Skeleton v4, w/ Content-Type,Role,Name,Language,Title for both theora/vorbis
// sample-fisbone-wrong-header.ogv
// - Skeleton v4, wrong message field sequence for vorbis
// multiple-bos-more-header-fields.ogg
// - Skeleton v3, w/ Content-Type,Role,Name,Language,Title for both theora/vorbis
// seek.ogv
// - No skeleton, but theora
// audio-overhang.ogg
// - No skeleton, but vorbis
var gMultitrackInfoOggPlayList = [
  { name:"sample-fisbone-skeleton4.ogv", type:"video/ogg", duration:5.049 },
  { name:"sample-fisbone-wrong-header.ogv", type:"video/ogg", duration:5.049 },
  { name:"multiple-bos-more-header-fileds.ogg", type:"video/ogg", duration:0.431 },
  { name:"seek.ogv", type:"video/ogg", duration:3.996 },
  { name:"audio-gaps.ogg", type:"audio/ogg", duration:2.208 }
];
// Pre-parsed results of gMultitrackInfoOggPlayList.
var gOggTrackInfoResults = {
  "sample-fisbone-skeleton4.ogv" : {
    "audio_id":" audio_1",
    "audio_kind":"main",
    "audio_language":" en-US",
    "audio_label":" Audio track for test",
    "video_id":" video_1",
    "video_kind":"main",
    "video_language":" fr",
    "video_label":" Video track for test"
  },
  "sample-fisbone-wrong-header.ogv" : {
    "audio_id":"1",
    "audio_kind":"main",
    "audio_language":"",
    "audio_label":"",
    "video_id":" video_1",
    "video_kind":"main",
    "video_language":" fr",
    "video_label":" Video track for test"
  },
  "multiple-bos-more-header-fileds.ogg" : {
    "audio_id":"1",
    "audio_kind":"main",
    "audio_language":"",
    "audio_label":"",
    "video_id":"2",
    "video_kind":"main",
    "video_language":"",
    "video_label":""
  },
  "seek.ogv" : {
    "video_id":"2",
    "video_kind":"main",
    "video_language":"",
    "video_label":""
  },
  "audio-gaps.ogg" : {
    "audio_id":"1",
    "audio_kind":"main",
    "audio_language":"",
    "audio_label":""
  }
};

// Converts a path/filename to a file:// URI which we can load from disk.
// Optionally checks whether the file actually exists on disk at the location
// we've specified.
function fileUriToSrc(path, mustExist) {
  // android mochitest doesn't support file://
  if (navigator.appVersion.indexOf("Android") != -1 || SpecialPowers.Services.appinfo.name == "B2G")
    return path;

  const Ci = SpecialPowers.Ci;
  const Cc = SpecialPowers.Cc;
  const Cr = SpecialPowers.Cr;
  var dirSvc = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIProperties);
  var f = dirSvc.get("CurWorkD", Ci.nsILocalFile);
  var split = path.split("/");
  for(var i = 0; i < split.length; ++i) {
    f.append(split[i]);
  }
  if (mustExist && !f.exists()) {
    ok(false, "We expected '" + path + "' to exist, but it doesn't!");
  }
  return f.path;
}

// Returns true if two TimeRanges are equal, false otherwise
function range_equals(r1, r2) {
  if (r1.length != r2.length) {
    return false;
  }
  for (var i = 0; i < r1.length; i++) {
    if (r1.start(i) != r2.start(i) || r1.end(i) != r2.end(i)) {
      return false;
    }
  }
  return true;
}

// These are URIs to files that we use to check that we don't leak any state
// or other information such that script can determine stuff about a user's
// environment. Used by test_info_leak.
var gInfoLeakTests = [
  {
    type: 'video/ogg',
    src: fileUriToSrc("tests/dom/media/test/320x240.ogv", true),
  },{
    type: 'video/ogg',
    src: fileUriToSrc("tests/dom/media/test/404.ogv", false),
  }, {
    type: 'audio/x-wav',
    src: fileUriToSrc("tests/dom/media/test/r11025_s16_c1.wav", true),
  }, {
    type: 'audio/x-wav',
    src: fileUriToSrc("tests/dom/media/test/404.wav", false),
  }, {
    type: 'audio/ogg',
    src: fileUriToSrc("tests/dom/media/test/bug461281.ogg", true),
  }, {
    type: 'audio/ogg',
    src: fileUriToSrc("tests/dom/media/test/404.ogg", false),
  }, {
    type: 'video/webm',
    src: fileUriToSrc("tests/dom/media/test/seek.webm", true),
  }, {
    type: 'video/webm',
    src: fileUriToSrc("tests/dom/media/test/404.webm", false),
  }, {
    type: 'video/ogg',
    src: 'http://localhost/404.ogv',
  }, {
    type: 'audio/x-wav',
    src: 'http://localhost/404.wav',
  }, {
    type: 'video/webm',
    src: 'http://localhost/404.webm',
  }, {
    type: 'video/ogg',
    src: 'http://example.com/tests/dom/media/test/test_info_leak.html'
  }, {
    type: 'audio/ogg',
    src: 'http://example.com/tests/dom/media/test/test_info_leak.html'
  }
];

// These are files that must fire an error during load or playback, and do not
// cause a crash.  Put files of the same type together in this list so if
// something crashes we have some idea of which backend is responsible.  Used
// by test_playback_errors, which expects one error event and no ended event.
// Put files of the same type together in this list so if something crashes
// we have some idea of which backend is responsible.
var gErrorTests = [
  { name:"bogus.wav", type:"audio/x-wav" },
  { name:"bogus.ogv", type:"video/ogg" },
  { name:"448636.ogv", type:"video/ogg" },
  { name:"bug504843.ogv", type:"video/ogg" },
  { name:"bug501279.ogg", type:"audio/ogg" },
  { name:"bug580982.webm", type:"video/webm" },
  { name:"bug603918.webm", type:"video/webm" },
  { name:"bug604067.webm", type:"video/webm" },
  { name:"bogus.duh", type:"bogus/duh" }
];

// These are files that have nontrivial duration and are useful for seeking within.
var gSeekTests = [
  { name:"r11025_s16_c1.wav", type:"audio/x-wav", duration:1.0 },
  { name:"audio.wav", type:"audio/x-wav", duration:0.031247 },
  { name:"seek.ogv", type:"video/ogg", duration:3.966 },
  { name:"320x240.ogv", type:"video/ogg", duration:0.266 },
  { name:"seek.webm", type:"video/webm", duration:3.966 },
  { name:"sine.webm", type:"audio/webm", duration:4.001 },
  { name:"bug516323.indexed.ogv", type:"video/ogg", duration:4.208333 },
  { name:"split.webm", type:"video/webm", duration:1.967 },
  { name:"detodos.opus", type:"audio/ogg; codecs=opus", duration:2.9135 },
  { name:"gizmo.mp4", type:"video/mp4", duration:5.56 },
  { name:"owl.mp3", type:"audio/mpeg", duration:3.29 },
  { name:"bogus.duh", type:"bogus/duh", duration:123 }
];

var gFastSeekTests = [
  { name:"gizmo.mp4", type:"video/mp4", keyframes:[0, 1.0, 2.0, 3.0, 4.0, 5.0 ] },
  // Note: Not all keyframes in the file are actually referenced in the Cues in this file.
  { name:"seek.webm", type:"video/webm", keyframes:[0, 0.8, 1.6, 2.4, 3.2]},
  // Note: the sync points are the points on both the audio and video streams
  // before the keyframes. You can't just assume that the keyframes are the sync
  // points, as the audio required for that sync point may be before the keyframe.
  { name:"bug516323.indexed.ogv", type:"video/ogg", keyframes:[0, 0.46, 3.06] },
];

function IsWindows8OrLater() {
  var re = /Windows NT (\d.\d)/;
  var winver = navigator.userAgent.match(re);
  return winver && winver.length == 2 && parseFloat(winver[1]) >= 6.2;
}

// These are files that are non seekable, due to problems with the media,
// for example broken or missing indexes.
var gUnseekableTests = [
  { name:"no-cues.webm", type:"video/webm" },
  { name:"bogus.duh", type:"bogus/duh"}
];
// Android supports fragmented MP4 playback from 4.3.
var androidVersion = SpecialPowers.Cc['@mozilla.org/system-info;1']
                                  .getService(SpecialPowers.Ci.nsIPropertyBag2)
                                  .getProperty('version');
// Fragmented MP4.
if (navigator.userAgent.indexOf("Mobile") != -1 && androidVersion >= 18) {
  gUnseekableTests = gUnseekableTests.concat([
    { name:"street.mp4", type:"video/mp4" }
  ]);
}

// These are files suitable for using with a "new Audio" constructor.
var gAudioTests = [
  { name:"r11025_s16_c1.wav", type:"audio/x-wav", duration:1.0 },
  { name:"sound.ogg", type:"audio/ogg" },
  { name:"owl.mp3", type:"audio/mpeg", duration:3.29 },
  { name:"small-shot.m4a", type:"audio/mp4", duration:0.29 },
  { name:"bogus.duh", type:"bogus/duh", duration:123 }
];

// These files ensure our handling of 404 errors is consistent across the
// various backends.
var g404Tests = [
  { name:"404.wav", type:"audio/x-wav" },
  { name:"404.ogv", type:"video/ogg" },
  { name:"404.oga", type:"audio/ogg" },
  { name:"404.webm", type:"video/webm" },
  { name:"bogus.duh", type:"bogus/duh" }
];

// These are files suitable for testing various decoder failures that are
// expected to fire MEDIA_ERR_DECODE.  Used by test_decode_error, which expects
// an error and emptied event, and no loadedmetadata or ended event.
var gDecodeErrorTests = [
  // Valid files with unsupported codecs
  { name:"r11025_msadpcm_c1.wav", type:"audio/x-wav" },
  { name:"dirac.ogg", type:"video/ogg" },
  // Invalid files
  { name:"bogus.wav", type:"audio/x-wav" },
  { name:"bogus.ogv", type:"video/ogg" },

  { name:"bogus.duh", type:"bogus/duh" }
];

// These are files that are used for media fragments tests
var gFragmentTests = [
  { name:"big.wav", type:"audio/x-wav", duration:9.278981, size:102444 }
];

// Used by test_chaining.html. The |links| attributes is the number of links in
// this file that we should be able to play.
var gChainingTests = [
  // Vorbis and Opus chained file. They have user comments |index=n| where `n`
  // is the index of this segment in the file, 0 indexed.
  { name:"chain.ogg", type:"audio/ogg", links: 4},
  { name:"chain.opus", type:"audio/ogg; codec=opus", links: 4},
  // Those files are chained files with a different number of channels in each
  // part. This is not supported and should stop playing after the first part.
  { name:"variable-channel.ogg", type:"audio/ogg", links: 1 },
  { name:"variable-channel.opus", type:"audio/ogg; codec=opus", links: 1 },
  // Those files are chained files with a different sample rate in each
  // part. This is not supported and should stop playing after the first part.
  { name:"variable-samplerate.ogg", type:"audio/ogg", links: 1 },
  // Opus decoding in Firefox outputs 48 kHz PCM despite having a different
  // original sample rate, so we can safely play Opus chained media that have
  // different samplerate accross links.
  { name:"variable-samplerate.opus", type:"audio/ogg; codec=opus", links: 2 },
  // A chained video file. We don't support those, so only one link should be
  // reported.
  { name:"chained-video.ogv", type:"video/ogg", links: 1 },
  // A file that consist in 4 links of audio, then another link that has video.
  // We should stop right after the 4 audio links.
  { name:"chained-audio-video.ogg", type:"video/ogg", links: 4 },
  // An opus file that has two links, with a different preskip value for each
  // link. We should be able to play both links.
  { name:"variable-preskip.opus", type:"audio/ogg; codec=opus", links: 2 },
  { name:"bogus.duh", type:"bogus/duh" }
];

// Videos with an aspect ratio. Used for testing that displaying frames
// on a canvas works correctly in the case of non-standard aspect ratios.
// See bug 874897 for an example.
var gAspectRatioTests = [
  { name:"VID_0001.ogg", type:"video/ogg", duration:19.966 }
];

// These are files with non-trivial tag sets.
// Used by test_metadata.html.
var gMetadataTests = [
  // Ogg Vorbis files
  { name:"short-video.ogv", tags: {
      TITLE:"Lepidoptera",
      ARTIST:"Epoq",
      ALBUM:"Kahvi Collective",
      DATE:"2002",
      COMMENT:"http://www.kahvi.org",
    }
  },
  { name:"bug516323.ogv", tags: {
      GENRE:"Open Movie",
      ENCODER:"Audacity",
      TITLE:"Elephants Dream",
      ARTIST:"Silvia Pfeiffer",
      COMMENTS:"Audio Description"
    }
  },
  { name:"bug516323.indexed.ogv", tags: {
      GENRE:"Open Movie",
      ENCODER:"Audacity",
      TITLE:"Elephants Dream",
      ARTIST:"Silvia Pfeiffer",
      COMMENTS:"Audio Description"
    }
  },
  { name:"detodos.opus", tags: {
      title:"De todos. Para todos.",
      artist:"Mozilla.org"
    }
  },
  { name:"sound.ogg", tags: { } },
  { name:"small-shot.ogg", tags: {
      title:"Pew SFX"
    }
  },
  { name:"badtags.ogg", tags: {
      // We list only the valid tags here, and verify
      // the invalid ones are filtered out.
      title:"Invalid comments test file",
      empty:"",
      "":"empty",
      "{- [(`!@\"#$%^&')] -}":"valid tag name, surprisingly"
      // The file also includes the following invalid tags.
      // "A description with no separator is a common problem.",
      // "雨":"Likely, but an invalid key (non-ascii).",
      // "not\nval\x1fid":"invalid tag name",
      // "not~valid":"this isn't a valid name either",
      // "not-utf-8":"invalid sequences: \xff\xfe\xfa\xfb\0eol"
    }
  },
  { name:"wave_metadata.wav", tags: {
      name:"Track Title",
      artist:"Artist Name",
      comments:"Comments",
    }
  },
  { name:"wave_metadata_utf8.wav", tags: {
      name:"歌曲名稱",
      artist:"作曲者",
      comments:"註解",
    }
  },
  { name:"wave_metadata_unknown_tag.wav", tags: {
      name:"Track Title",
      comments:"Comments",
    }
  },
  { name:"wave_metadata_bad_len.wav", tags: {
      name:"Track Title",
      artist:"Artist Name",
    }
  },
  { name:"wave_metadata_bad_no_null.wav", tags: {
      name:"Track Title",
      artist:"Artist Name",
      comments:"Comments!!",
    }
  },
  { name:"wave_metadata_bad_utf8.wav", tags: {
      name:"歌曲名稱",
      comments:"註解",
    }
  },
  { name:"wavedata_u8.wav", tags: { }
  },
];

// Test files for Encrypted Media Extensions
var gEMETests = [
  {
    name:"video-only with 2 keys",
    tracks: [
      {
        name:"video",
        type:"video/mp4; codecs=\"avc1.64000d\"",
        fragments:[ "bipbop-cenc-videoinit.mp4",
                    "bipbop-cenc-video1.m4s",
                    "bipbop-cenc-video2.m4s",
                  ]
      }
    ],
    keys: {
      // "keyid" : "key"
      "7e571d037e571d037e571d037e571d03" : "7e5733337e5733337e5733337e573333",
      "7e571d047e571d047e571d047e571d04" : "7e5744447e5744447e5744447e574444",
    },
    sessionType:"temporary",
    sessionCount:1,
    duration:1.60,
  },
  {
    name:"video-only with 2 keys, CORS",
    tracks: [
      {
        name:"video",
        type:"video/mp4; codecs=\"avc1.64000d\"",
        fragments:[ "bipbop-cenc-videoinit.mp4",
                    "bipbop-cenc-video1.m4s",
                    "bipbop-cenc-video2.m4s",
                  ]
      }
    ],
    keys: {
      // "keyid" : "key"
      "7e571d037e571d037e571d037e571d03" : "7e5733337e5733337e5733337e573333",
      "7e571d047e571d047e571d047e571d04" : "7e5744447e5744447e5744447e574444",
    },
    sessionType:"temporary",
    sessionCount:1,
    crossOrigin:true,
    duration:1.60,
  },
  {
    name:"audio&video tracks, both with all keys",
    tracks: [
      {
        name:"audio",
        type:"audio/mp4; codecs=\"mp4a.40.2\"",
        fragments:[ "bipbop-cenc-audioinit.mp4",
                    "bipbop-cenc-audio1.m4s",
                    "bipbop-cenc-audio2.m4s",
                    "bipbop-cenc-audio3.m4s",
                  ],
      },
      {
        name:"video",
        type:"video/mp4; codecs=\"avc1.64000d\"",
        fragments:[ "bipbop-cenc-videoinit.mp4",
                    "bipbop-cenc-video1.m4s",
                    "bipbop-cenc-video2.m4s",
                  ],
      },
    ],
    keys: {
      // "keyid" : "key"
      "7e571d037e571d037e571d037e571d03" : "7e5733337e5733337e5733337e573333",
      "7e571d047e571d047e571d047e571d04" : "7e5744447e5744447e5744447e574444",
    },
    sessionType:"temporary",
    sessionCount:2,
    duration:1.60,
  },
  {
    name:"audio&video tracks, both with all keys, CORS",
    tracks: [
      {
        name:"audio",
        type:"audio/mp4; codecs=\"mp4a.40.2\"",
        fragments:[ "bipbop-cenc-audioinit.mp4",
                    "bipbop-cenc-audio1.m4s",
                    "bipbop-cenc-audio2.m4s",
                    "bipbop-cenc-audio3.m4s",
                  ],
      },
      {
        name:"video",
        type:"video/mp4; codecs=\"avc1.64000d\"",
        fragments:[ "bipbop-cenc-videoinit.mp4",
                    "bipbop-cenc-video1.m4s",
                    "bipbop-cenc-video2.m4s",
                  ],
      },
    ],
    keys: {
      // "keyid" : "key"
      "7e571d037e571d037e571d037e571d03" : "7e5733337e5733337e5733337e573333",
      "7e571d047e571d047e571d047e571d04" : "7e5744447e5744447e5744447e574444",
    },
    sessionType:"temporary",
    sessionCount:2,
    crossOrigin:true,
    duration:1.60,
  },
  {
    name:"audio&video tracks, each with its key",
    type:"video/mp4; codecs=\"avc1.64000d,mp4a.40.2\"",
    tracks: [
      {
        name:"audio",
        type:"audio/mp4; codecs=\"mp4a.40.2\"",
        fragments:[ "bipbop-cenc1-audioinit.mp4",
                    "bipbop-cenc1-audio1.m4s",
                    "bipbop-cenc1-audio2.m4s",
                    "bipbop-cenc1-audio3.m4s",
                  ],
      },
      {
        name:"video",
        type:"video/mp4; codecs=\"avc1.64000d\"",
        fragments:[ "bipbop-cenc1-videoinit.mp4",
                    "bipbop-cenc1-video1.m4s",
                    "bipbop-cenc1-video2.m4s",
                  ],
      },
    ],
    keys: {
      // "keyid" : "key"
      "7e571d037e571d037e571d037e571d03" : "7e5733337e5733337e5733337e573333",
      "7e571d047e571d047e571d047e571d04" : "7e5744447e5744447e5744447e574444",
    },
    sessionType:"temporary",
    sessionCount:2,
    duration:1.60,
  },
];

var gEMENonMSEFailTests = [
  {
    name:"short-cenc.mp4",
    type:"video/mp4; codecs=\"avc1.64000d,mp4a.40.2\"",
    duration:0.47,
  },
];

function checkMetadata(msg, e, test) {
  if (test.width) {
    is(e.videoWidth, test.width, msg + " video width");
  }
  if (test.height) {
    is(e.videoHeight, test.height, msg + " video height");
  }
  if (test.duration) {
    ok(Math.abs(e.duration - test.duration) < 0.1,
       msg + " duration (" + e.duration + ") should be around " + test.duration);
  }
  is(!!test.keys, SpecialPowers.do_lookupGetter(e, "isEncrypted").apply(e),
     msg + " isEncrypted should be true if we have decryption keys");
}

// Returns the first test from candidates array which we can play with the
// installed video backends.
function getPlayableVideo(candidates) {
  var resources = getPlayableVideos(candidates);
  if (resources.length > 0)
    return resources[0];
  return null;
}

function getPlayableVideos(candidates) {
  var v = document.createElement("video");
  return candidates.filter(function(x){return /^video/.test(x.type) && v.canPlayType(x.type);});
}

function getPlayableAudio(candidates) {
  var v = document.createElement("audio");
  var resources = candidates.filter(function(x){return /^audio/.test(x.type) && v.canPlayType(x.type);});
  if (resources.length > 0)
    return resources[0];
  return null;
}

// Returns the type of element that should be created for the given mimetype.
function getMajorMimeType(mimetype) {
  if (/^video/.test(mimetype)) {
    return "video";
  } else {
    return "audio";
  }
}

// Force releasing decoder to avoid timeout in waiting for decoding resource.
function removeNodeAndSource(n) {
  n.remove();
  // Clearing mozSrcObject and/or src will actually set them to some default
  // URI that will fail to load, so make sure we don't produce a spurious
  // bailing error.
  n.onerror = null;
  // reset |mozSrcObject| first since it takes precedence over |src|.
  n.mozSrcObject = null;
  n.src = "";
  while (n.firstChild) {
    n.removeChild(n.firstChild);
  }
}

function once(target, name, cb) {
  var p = new Promise(function(resolve, reject) {
    target.addEventListener(name, function() {
      target.removeEventListener(name, cb);
      resolve();
    });
  });
  if (cb) {
    p.then(cb);
  }
  return p;
}

function TimeStamp(token) {
  function pad(x) {
    return (x < 10) ? "0" + x : x;
  }
  var now = new Date();
  var ms = now.getMilliseconds();
  var time = "[" +
             pad(now.getHours()) + ":" +
             pad(now.getMinutes()) + ":" +
             pad(now.getSeconds()) + "." +
             ms +
             "]" +
             (ms < 10 ? "  " : (ms < 100 ? " " : ""));
  return token ? (time + " " + token) : time;
}

function Log(token, msg) {
  info(TimeStamp(token) + " " + msg);
}

// Number of tests to run in parallel.
var PARALLEL_TESTS = 2;

// Prefs to set before running tests.  Use this to improve coverage of
// conditions that might not otherwise be encountered on the test data.
var gTestPrefs = [
  ['media.recorder.max_memory', 1024],
  ["media.preload.default", 2], // default preload = metadata
  ["media.preload.auto", 3] // auto preload = enough
];

// When true, we'll loop forever on whatever test we run. Use this to debug
// intermittent test failures.
const DEBUG_TEST_LOOP_FOREVER = false;

// Manages a run of media tests. Runs them in chunks in order to limit
// the number of media elements/threads running in parallel. This limits peak
// memory use, particularly on Linux x86 where thread stacks use 10MB of
// virtual address space.
// Usage:
//   1. Create a new MediaTestManager object.
//   2. Create a test startTest function. This takes a test object and a token,
//      and performs anything necessary to start the test. The test object is an
//      element in one of the g*Tests above. Your startTest function must call
//      MediaTestManager.start(token) if it starts a test. The test object is
//      guaranteed to be playable by our supported decoders; you don't need to
//      check canPlayType.
//   3. When your tests finishes, call MediaTestManager.finished(), passing
//      the token back to the manager. The manager may either start the next run
//      or end the mochitest if all the tests are done.
function MediaTestManager() {

  // Sets up a MediaTestManager to runs through the 'tests' array, which needs
  // to be one of, or have the same fields as, the g*Test arrays of tests. Uses
  // the user supplied 'startTest' function to initialize the test. This
  // function must accept two arguments, the test entry from the 'tests' array,
  // and a token. Call MediaTestManager.started(token) if you start the test,
  // and MediaTestManager.finished(token) when the test finishes. You don't have
  // to start every test, but if you call started() you *must* call finish()
  // else you'll timeout.
  this.runTests = function(tests, startTest) {
    this.startTime = new Date();
    SimpleTest.info("Started " + this.startTime + " (" + this.startTime.getTime()/1000 + "s)");
    this.testNum = 0;
    this.tests = tests;
    this.startTest = startTest;
    this.tokens = [];
    this.isShutdown = false;
    this.numTestsRunning = 0;
    // Always wait for explicit finish.
    SimpleTest.waitForExplicitFinish();
    SpecialPowers.pushPrefEnv({'set': gTestPrefs}, (function() {
      this.nextTest();
    }).bind(this));
  }

  // Registers that the test corresponding to 'token' has been started.
  // Don't call more than once per token.
  this.started = function(token) {
    this.tokens.push(token);
    this.numTestsRunning++;
    is(this.numTestsRunning, this.tokens.length, "[started " + token + "] Length of array should match number of running tests");
  }

  this.watchdog = null;

  this.watchdogFn = function() {
    if (this.tokens.length > 0) {
      info("Watchdog remaining tests= " + this.tokens);
    }
  }

  // Registers that the test corresponding to 'token' has finished. Call when
  // you've finished your test. If all tests are complete this will finish the
  // run, otherwise it may start up the next run. It's ok to call multiple times
  // per token.
  this.finished = function(token) {
    var i = this.tokens.indexOf(token);
    if (i != -1) {
      // Remove the element from the list of running tests.
      this.tokens.splice(i, 1);
    }

    if (this.watchdog) {
      clearTimeout(this.watchdog);
      this.watchdog = null;
    }

    info("[finished " + token + "] remaining= " + this.tokens);
    this.numTestsRunning--;
    is(this.numTestsRunning, this.tokens.length, "[finished " + token + "] Length of array should match number of running tests");
    if (this.tokens.length < PARALLEL_TESTS) {
      this.nextTest();
      this.watchdog = setTimeout(this.watchdogFn.bind(this), 10000);
    }
  }

  // Starts the next batch of tests, or finishes if they're all done.
  // Don't call this directly, call finished(token) when you're done.
  this.nextTest = function() {
    while (this.testNum < this.tests.length && this.tokens.length < PARALLEL_TESTS) {
      var test = this.tests[this.testNum];
      var token = (test.name ? (test.name + "-"): "") + this.testNum;
      this.testNum++;

      if (DEBUG_TEST_LOOP_FOREVER && this.testNum == this.tests.length) {
        this.testNum = 0;
      }

      // Ensure we can play the resource type.
      if (test.type && !document.createElement('video').canPlayType(test.type))
        continue;

      // Do the init. This should start the test.
      this.startTest(test, token);
    }

    if (this.testNum == this.tests.length &&
        !DEBUG_TEST_LOOP_FOREVER &&
        this.tokens.length == 0 &&
        !this.isShutdown)
    {
      this.isShutdown = true;
      if (this.onFinished) {
        this.onFinished();
      }
      var onCleanup = function() {
        var end = new Date();
        SimpleTest.info("Finished at " + end + " (" + (end.getTime() / 1000) + "s)");
        SimpleTest.info("Running time: " + (end.getTime() - this.startTime.getTime())/1000 + "s");
        SimpleTest.finish();
      }.bind(this);
      mediaTestCleanup(onCleanup);
      return;
    }
  }
}

// Ensures we've got no active video or audio elements in the document, and
// forces a GC to release the address space reserved by the decoders' threads'
// stacks.
function mediaTestCleanup(callback) {
    var V = document.getElementsByTagName("video");
    for (i=0; i<V.length; i++) {
      removeNodeAndSource(V[i]);
      V[i] = null;
    }
    var A = document.getElementsByTagName("audio");
    for (i=0; i<A.length; i++) {
      removeNodeAndSource(A[i]);
      A[i] = null;
    }
    SpecialPowers.exactGC(window, callback);
}

function setMediaTestsPrefs(callback, extraPrefs) {
  var prefs = gTestPrefs;
  if (extraPrefs) {
    prefs = prefs.concat(extraPrefs);
  }
  SpecialPowers.pushPrefEnv({"set": prefs}, callback);
}

SimpleTest.requestFlakyTimeout("untriaged");