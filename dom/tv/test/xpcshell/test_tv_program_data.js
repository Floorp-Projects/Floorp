"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

function run_test() {
  run_next_test();
}

add_test(function test_valid_event_id() {
  var eventId = "eventId";

  var data = Cc["@mozilla.org/tv/tvprogramdata;1"].
             createInstance(Ci.nsITVProgramData);
  data.eventId = eventId;

  equal(data.eventId, eventId);

  run_next_test();
});

add_test(function test_empty_event_id() {
  var data = Cc["@mozilla.org/tv/tvprogramdata;1"].
             createInstance(Ci.nsITVProgramData);
  Assert.throws(function() {
    data.eventId = "";
  }, /NS_ERROR_ILLEGAL_VALUE/i);

  run_next_test();
});

add_test(function test_valid_title() {
  var title = "title";

  var data = Cc["@mozilla.org/tv/tvprogramdata;1"].
             createInstance(Ci.nsITVProgramData);
  data.title = title;

  equal(data.title, title);

  run_next_test();
});

add_test(function test_empty_title() {
  var data = Cc["@mozilla.org/tv/tvprogramdata;1"].
             createInstance(Ci.nsITVProgramData);
  Assert.throws(function() {
    data.title = "";
  }, /NS_ERROR_ILLEGAL_VALUE/i);

  run_next_test();
});

add_test(function test_start_time() {
  var startTime = 1;

  var data = Cc["@mozilla.org/tv/tvprogramdata;1"].
             createInstance(Ci.nsITVProgramData);
  data.startTime = startTime;

  equal(data.startTime, startTime);

  run_next_test();
});

add_test(function test_duration() {
  var duration = 1;

  var data = Cc["@mozilla.org/tv/tvprogramdata;1"].
             createInstance(Ci.nsITVProgramData);
  data.duration = duration;

  equal(data.duration, duration);

  run_next_test();
});

add_test(function test_valid_description() {
  var description = "description";

  var data = Cc["@mozilla.org/tv/tvprogramdata;1"].
             createInstance(Ci.nsITVProgramData);
  data.description = description;

  equal(data.description, description);

  run_next_test();
});

add_test(function test_empty_description() {
  var description = "";

  var data = Cc["@mozilla.org/tv/tvprogramdata;1"].
             createInstance(Ci.nsITVProgramData);
  data.description = description;

  equal(data.description, description);

  run_next_test();
});

add_test(function test_valid_rating() {
  var rating = "rating";

  var data = Cc["@mozilla.org/tv/tvprogramdata;1"].
             createInstance(Ci.nsITVProgramData);
  data.rating = rating;

  equal(data.rating, rating);

  run_next_test();
});

add_test(function test_empty_rating() {
  var rating = "";

  var data = Cc["@mozilla.org/tv/tvprogramdata;1"].
             createInstance(Ci.nsITVProgramData);
  data.rating = rating;

  equal(data.rating, rating);

  run_next_test();
});

add_test(function test_valid_audio_languages() {
  var languages = ["eng", "jpn"];

  var data = Cc["@mozilla.org/tv/tvprogramdata;1"].
             createInstance(Ci.nsITVProgramData);
  data.setAudioLanguages(languages.length, languages);

  var audioLanguages = data.getAudioLanguages();
  equal(audioLanguages.length, languages.length);
  for (var i = 0; i < audioLanguages.length; i++) {
    equal(audioLanguages[i], languages[i]);
  }

  run_next_test();
});

add_test(function test_empty_audio_languages() {
  var data = Cc["@mozilla.org/tv/tvprogramdata;1"].
             createInstance(Ci.nsITVProgramData);
  data.setAudioLanguages(0, []);

  var audioLanguages = data.getAudioLanguages();
  equal(audioLanguages.length, 0);

  run_next_test();
});

add_test(function test_mismatched_audio_languages() {
  var data = Cc["@mozilla.org/tv/tvprogramdata;1"].
             createInstance(Ci.nsITVProgramData);
  Assert.throws(function() {
    data.setAudioLanguages(1, []);
  }, /NS_ERROR_XPC_NOT_ENOUGH_ELEMENTS_IN_ARRAY/i);

  run_next_test();
});

add_test(function test_valid_subtitle_languages() {
  var languages = ["eng", "jpn"];

  var data = Cc["@mozilla.org/tv/tvprogramdata;1"].
             createInstance(Ci.nsITVProgramData);
  data.setSubtitleLanguages(languages.length, languages);

  var subtitleLanguages = data.getSubtitleLanguages();
  equal(subtitleLanguages.length, languages.length);
  for (var i = 0; i < subtitleLanguages.length; i++) {
    equal(subtitleLanguages[i], languages[i]);
  }

  run_next_test();
});

add_test(function test_empty_subtitle_languages() {
  var data = Cc["@mozilla.org/tv/tvprogramdata;1"].
             createInstance(Ci.nsITVProgramData);
  data.setSubtitleLanguages(0, []);

  var subtitleLanguages = data.getSubtitleLanguages();
  equal(subtitleLanguages.length, 0);

  run_next_test();
});

add_test(function test_mismatched_subtitle_languages() {
  var data = Cc["@mozilla.org/tv/tvprogramdata;1"].
             createInstance(Ci.nsITVProgramData);
  Assert.throws(function() {
    data.setSubtitleLanguages(1, []);
  }, /NS_ERROR_XPC_NOT_ENOUGH_ELEMENTS_IN_ARRAY/i);

  run_next_test();
});
