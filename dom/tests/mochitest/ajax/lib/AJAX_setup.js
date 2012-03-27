var AJAXtests = [];

function runAJAXTest() {
  if (AJAXtests.length == 0) {
    SimpleTest.finish();
    return;
  }

  var test = AJAXtests.shift();
  var testframe = document.getElementById("testframe");
  testframe.src = test;
}

function onManifestLoad(manifest) {
  if (manifest.testcases) {
    AJAXtests = manifest.testcases;
    runAJAXTest();    
  } else {
    ok(false, "manifest check", "no manifest!?!");
    SimpleTest.finish();
  }
}

function fetchManifest() {
  var d = loadJSONDoc("manifest.json");
  d.addBoth(onManifestLoad);
}

SimpleTest.waitForExplicitFinish();
addLoadEvent(fetchManifest);
