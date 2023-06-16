"use strict";

const { WebVTT } = ChromeUtils.importESModule(
  "resource://gre/modules/vtt.sys.mjs"
);

let fakeWindow = {
  /* eslint-disable object-shorthand */
  VTTCue: function () {},
  VTTRegion: function () {},
  /* eslint-enable object-shorthand */
};

// We have a better parser check in WPT. Here I want to check that incomplete
// lines are correctly parsable.
let tests = [
  // Signature
  { input: ["WEBVTT"], cue: 0, region: 0 },
  { input: ["", "WE", "BVT", "T"], cue: 0, region: 0 },
  { input: ["WEBVTT - This file has no cues."], cue: 0, region: 0 },
  { input: ["WEBVTT", " - ", "This file has no cues."], cue: 0, region: 0 },

  // Body with IDs
  {
    input: [
      "WEB",
      "VTT - This file has cues.\n",
      "\n",
      "14\n",
      "00:01:14",
      ".815 --> 00:0",
      "1:18.114\n",
      "- What?\n",
      "- Where are we now?\n",
      "\n",
      "15\n",
      "00:01:18.171 --> 00:01:20.991\n",
      "- T",
      "his is big bat country.\n",
      "\n",
      "16\n",
      "00:01:21.058 --> 00:01:23.868\n",
      "- [ Bat",
      "s Screeching ]\n",
      "- They won't get in your hair. They're after the bug",
      "s.\n",
    ],
    cue: 3,
    region: 0,
  },

  // Body without IDs
  {
    input: [
      "WEBVTT - This file has c",
      "ues.\n",
      "\n",
      "00:01:14.815 --> 00:01:18.114\n",
      "- What?\n",
      "- Where are we now?\n",
      "\n",
      "00:01:18.171 --> 00:01:2",
      "0.991\n",
      "- ",
      "This is big bat country.\n",
      "\n",
      "00:01:21.058 --> 00:01:23.868\n",
      "- [ Bats S",
      "creeching ]\n",
      "- They won't get in your hair. They're after the bugs.\n",
    ],
    cue: 3,
    region: 0,
  },

  // Note
  {
    input: ["WEBVTT - This file has no cues.\n", "\n", "NOTE what"],
    cue: 0,
    region: 0,
  },

  // Regions - This vtt is taken from a WPT
  {
    input: [
      "WE",
      "BVTT\n",
      "\n",
      "REGION\n",
      "id:0\n",
      "\n",
      "REGION\n",
      "id:1\n",
      "region",
      "an",
      "chor:0%,0%\n",
      "\n",
      "R",
      "EGION\n",
      "id:2\n",
      "regionanchor:18446744073709552000%,18446744",
      "073709552000%\n",
      "\n",
      "REGION\n",
      "id:3\n",
      "regionanchor: 100%,100%\n",
      "regio",
      "nanchor :100%,100%\n",
      "regionanchor:100% ,100%\n",
      "regionanchor:100%, 100%\n",
      "regionanchor:100 %,100%\n",
      "regionanchor:10",
      "0%,100 %\n",
      "\n",
      "00:00:00.000 --> 00:00:01.000",
      " region:0\n",
      "text\n",
      "\n",
      "00:00:00.000 --> 00:00:01.000 region:1\n",
      "text\n",
      "\n",
      "00:00:00.000 --> 00:00:01.000 region:3\n",
      "text\n",
    ],
    cue: 3,
    region: 4,
  },
];

function run_test() {
  tests.forEach(test => {
    let parser = new WebVTT.Parser(fakeWindow, null);
    ok(!!parser, "Ok... this is a good starting point");

    let cue = 0;
    parser.oncue = () => {
      ++cue;
    };

    let region = 0;
    parser.onregion = () => {
      ++region;
    };

    parser.onparsingerror = () => {
      ok(false, "No error accepted");
    };

    test.input.forEach(input => {
      parser.parse(new TextEncoder().encode(input));
    });

    parser.flush();

    equal(cue, test.cue, "Cue value matches");
    equal(region, test.region, "Region value matches");
  });
}
