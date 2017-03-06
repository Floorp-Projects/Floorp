/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const origins = [
    [
      {
        oldPath: "indexedDB/1007+f+app+++system.gaiamobile.org"
      },

      {
        oldPath: "indexedDB/1007+t+https+++developer.cdn.mozilla.net"
      },

      {
        oldPath: "indexedDB/http+++www.mozilla.org",
        newPath: "storage/default/http+++www.mozilla.org",
        url: "http://www.mozilla.org",
        persistence: "default"
      }
    ],

    [
      {
        oldPath: "storage/persistent/1007+f+app+++system.gaiamobile.org"
      },

      {
        oldPath: "storage/persistent/1007+t+https+++developer.cdn.mozilla.net"
      },

      {
        oldPath: "storage/persistent/http+++www.mozilla.org",
        newPath: "storage/default/http+++www.mozilla.org",
        url: "http://www.mozilla.org",
        persistence: "default"
      }
    ]
  ];

  const metadataFileName = ".metadata";

  // Test upgrade from FF 21 (no idb subdirs).

  for (let i = 1; i <= 2; i++) {
    clear(continueToNextStepSync);
    yield undefined;

    installPackage("idbSubdirUpgrade" + i + "_profile");

    info("Checking origin directories");

    for (let origin of origins[i - 1]) {
      let originDir = getRelativeFile(origin.oldPath);
      let exists = originDir.exists();
      ok(exists, "Origin directory does exist");

      let idbDir = originDir.clone();
      idbDir.append("idb");

      exists = idbDir.exists();
      ok(!exists, "idb directory doesn't exist");

      let metadataFile = originDir.clone();
      metadataFile.append(metadataFileName);

      exists = metadataFile.exists();
      ok(!exists, "Metadata file doesn't exist");

      if (origin.newPath) {
        originDir = getRelativeFile(origin.newPath);
        exists = originDir.exists();
        ok(!exists, "Origin directory doesn't exist");
      }
    }

    info("Initializing");

    let request = init(continueToNextStepSync);
    yield undefined;

    ok(request.resultCode == NS_OK, "Initialization succeeded");

    info("Checking origin directories");

    for (let origin of origins[i - 1]) {
      let originDir = getRelativeFile(origin.oldPath);
      let exists = originDir.exists();
      ok(!exists, "Origin directory doesn't exist");

      if (origin.newPath) {
        originDir = getRelativeFile(origin.newPath);
        exists = originDir.exists();
        ok(exists, "Origin directory does exist");

        let idbDir = originDir.clone();
        idbDir.append("idb");

        exists = idbDir.exists();
        ok(exists, "idb directory does exist");

        let metadataFile = originDir.clone();
        metadataFile.append(metadataFileName);

        exists = metadataFile.exists();
        ok(exists, "Metadata file does exist");

        info("Initializing origin");

        let principal = getPrincipal(origin.url);
        request =
          initOrigin(principal, origin.persistence, continueToNextStepSync);
        yield undefined;

        ok(request.resultCode == NS_OK, "Initialization succeeded");

        ok(!request.result, "Origin directory wasn't created");
      }
    }
  }

  finishTest();
}
