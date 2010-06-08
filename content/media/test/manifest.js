// In each list of tests below, test file types that are not supported should
// be ignored. To make sure tests respect that, we include a file of type
// "bogus/duh" in each list.

// These are small test files, good for just seeing if something loads. We
// really only need one test file per backend here.
var gSmallTests = [
  { name:"r11025_s16_c1.wav", type:"audio/x-wav", duration:1.0 },
  { name:"320x240.ogv", type:"video/ogg", width:320, height:240, duration:0.233 },
  { name:"small-shot.ogg", type:"audio/ogg", duration:0.276 },
  { name:"bogus.duh", type:"bogus/duh" }
];

// Used by test_progress to ensure we get the correct progress information
// during resource download.
var gProgressTests = [
  { name:"r11025_u8_c1.wav", type:"audio/x-wav", duration:1.0, size:11069 },
  { name:"big.wav", type:"audio/x-wav", duration:9.0, size:102444 },
  { name:"seek.ogv", type:"video/ogg", duration:3.966, size:285310 },
  { name:"320x240.ogv", type:"video/ogg", width:320, height:240, duration:0.233, size:28942 },
  { name:"bogus.duh", type:"bogus/duh" }
];

// Used by test_mozLoadFrom.  Need one test file per decoder backend, plus
// anything for testing clone-specific bugs.
var gCloneTests = gSmallTests.concat([
  // Actual duration is ~200ms, we have Content-Duration lie about it.
  { name:"bug520908.ogv", type:"video/ogg", duration:9000 },
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

// These are files that we want to make sure we can play through.  We can
// also check metadata.  Put files of the same type together in this list so if
// something crashes we have some idea of which backend is responsible.
// Used by test_playback, which expects no error event and one ended event.
var gPlayTests = [
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
  // With first frame a "duplicate" (empty) frame.
  { name:"bug500311.ogv", type:"video/ogg", duration:1.96 },
  // Small audio file
  { name:"small-shot.ogg", type:"video/ogg", duration:0.276 },
  // More audio in file than video.
  { name:"short-video.ogv", type:"video/ogg", duration:1.081 },
  // First Theora data packet is zero bytes.
  { name:"bug504613.ogv", type:"video/ogg", duration:Number.NaN },
  // Multiple audio streams.
  { name:"bug516323.ogv", type:"video/ogg", duration:4.208 },

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
  { name:"audio-overhang.ogg", type:"audio/ogg", duration:2.3 },
  { name:"video-overhang.ogg", type:"audio/ogg", duration:3.966 },

  // Test playback/metadata work after a redirect
  { name:"redirect.sjs?domain=mochi.test:8888&file=320x240.ogv",
    type:"video/ogg", duration:0.233 },

  // Test playback of a webm file
  { name:"seek.webm", type:"video/webm", duration:3.966 },

  { name:"bogus.duh", type:"bogus/duh", duration:Number.NaN }
  
];

// Converts a path/filename to a file:// URI which we can load from disk.
// Optionally checks whether the file actually exists on disk at the location
// we've specified.
function fileUriToSrc(path, mustExist) {
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  const Ci = Components.interfaces;
  const Cc = Components.classes;
  const Cr = Components.results;
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

// These are URIs to files that we use to check that we don't leak any state
// or other information such that script can determine stuff about a user's
// environment. Used by test_info_leak.
var gInfoLeakTests = [
  {
    type: 'video/ogg',
    src: fileUriToSrc("tests/content/media/test/320x240.ogv", true),
  },{
    type: 'video/ogg',
    src: fileUriToSrc("tests/content/media/test/404.ogv", false),
  }, {
    type: 'audio/x-wav',
    src: fileUriToSrc("tests/content/media/test/r11025_s16_c1.wav", true),
  }, {
    type: 'audio/x-wav',
    src: fileUriToSrc("tests/content/media/test/404.wav", false),
  }, {
    type: 'audio/ogg',
    src: fileUriToSrc("tests/content/media/test/bug461281.ogg", true),
  }, {
    type: 'audio/ogg',
    src: fileUriToSrc("tests/content/media/test/404.ogg", false),
  }, {
    type: 'video/ogg',
    src: 'http://localhost/404.ogv',
  }, {
    type: 'audio/x-wav',
    src: 'http://localhost/404.wav',
  }, {
    type: 'video/ogg',
    src: 'http://example.com/tests/content/media/test/test_info_leak.html'
  }, {
    type: 'audio/ogg',
    src: 'http://example.com/tests/content/media/test/test_info_leak.html'
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
  { name:"bogus.duh", type:"bogus/duh" }
];

// These are files that have nontrivial duration and are useful for seeking within.
var gSeekTests = [
  { name:"r11025_s16_c1.wav", type:"audio/x-wav", duration:1.0 },
  { name:"seek.ogv", type:"video/ogg", duration:3.966 },
  { name:"320x240.ogv", type:"video/ogg", duration:0.233 },
  { name:"bogus.duh", type:"bogus/duh", duration:123 }
];

// These are files suitable for using with a "new Audio" constructor.
var gAudioTests = [
  { name:"r11025_s16_c1.wav", type:"audio/x-wav", duration:1.0 },
  { name:"sound.ogg", type:"audio/ogg" },
  { name:"bogus.duh", type:"bogus/duh", duration:123 }
];

// These files ensure our hanlding of 404 errors is consistent across the
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
}

// Returns true if all members of array 'v' have their _finished field set to true.
function AllFinished(v) {
  if (v.length == 0) {
    return false;
  }
  for (var i=0; i<v.length; ++i) {
    if (!v[i]._finished) {
      return false;
    }
  }
  return true;
}

// Returns the first test from candidates array which we can play with the
// installed video backends.
function getPlayableVideo(candidates) {
  var v = document.createElement("video");
  var resources = candidates.filter(function(x){return /^video/.test(x.type) && v.canPlayType(x.type);});
  if (resources.length > 0)
    return resources[0];
  return null;
}
