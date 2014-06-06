/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "data:text/html;charset=utf8,<p>browser_profiler_io</p>";

let temp = {};
Cu.import("resource://gre/modules/FileUtils.jsm", temp);
let FileUtils = temp.FileUtils;
let gTab, gPanel;

let gData = {
  "libs": "[]", // This property is not important for this test.
  "meta": {
    "version": 2,
    "interval": 1,
    "stackwalk": 0,
    "jank": 0,
    "processType": 0,
    "platform": "Macintosh",
    "oscpu": "Intel Mac OS X 10.8",
    "misc": "rv:25.0",
    "abi": "x86_64-gcc3",
    "toolkit": "cocoa",
    "product": "Firefox"
  },
  "threads": [
    {
      "samples": [
        {
          "name": "(root)",
          "frames": [
            {
              "location": "Startup::XRE_Main",
              "line": 3871
            },
            {
              "location": "Events::ProcessGeckoEvents",
              "line": 355
            },
            {
              "location": "Events::ProcessGeckoEvents",
              "line": 355
            }
          ],
          "responsiveness": -0.002963,
          "time": 8.120823
        }
      ]
    }
  ]
};

function test() {
  waitForExplicitFinish();

  setUp(URL, function onSetUp(tab, browser, panel) {
    gTab = tab;
    gPanel = panel;

    let file = FileUtils.getFile("TmpD", ["tmpprofile.json"]);
    file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

    gPanel.saveProfile(file, gData)
      .then(gPanel.loadProfile.bind(gPanel, file))
      .then(checkData);
  });
}

function checkData() {
  let profile = gPanel.activeProfile;
  let item = gPanel.sidebar.getItemByProfile(profile);
  let data = profile.data;

  is(item.attachment.state, PROFILE_COMPLETED, "Profile is COMPLETED");
  is(gData.meta.oscpu, data.meta.oscpu, "Meta data is correct");
  is(gData.threads[0].samples.length, 1, "There's one sample");
  is(gData.threads[0].samples[0].name, "(root)", "Sample is correct");

  tearDown(gTab, () => { gPanel = null; gTab = null; });
}
