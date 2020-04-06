/*
 * Bug 1264577 - A test case for testing caches of various submodules.
 *   This test case will load two pages that each page loads various resources
 *   within the same third party domain for the same originAttributes or different
 *   originAttributes. And then, it verifies the number of cache entries and
 *   the originAttributes of loading channels. If these two pages belong to
 *   the same originAttributes, the number of cache entries for a certain
 *   resource would be one. Otherwise, it would be two.
 */

const CC = Components.Constructor;

let protocolProxyService = Cc[
  "@mozilla.org/network/protocol-proxy-service;1"
].getService(Ci.nsIProtocolProxyService);

const TEST_DOMAIN = "http://example.net";
const TEST_PATH = "/browser/browser/components/originattributes/test/browser/";
const TEST_PAGE = TEST_DOMAIN + TEST_PATH + "file_cache.html";

let suffixes = [
  "iframe.html",
  "link.css",
  "script.js",
  "img.png",
  "object.png",
  "embed.png",
  "xhr.html",
  "worker.xhr.html",
  "audio.ogg",
  "video.ogv",
  "track.vtt",
  "fetch.html",
  "worker.fetch.html",
  "request.html",
  "worker.request.html",
  "import.js",
  "worker.js",
  "sharedworker.js",
];

// A random value for isolating video/audio elements across different tests.
let randomSuffix;

function clearAllImageCaches() {
  let tools = SpecialPowers.Cc["@mozilla.org/image/tools;1"].getService(
    SpecialPowers.Ci.imgITools
  );
  let imageCache = tools.getImgCacheForDocument(window.document);
  imageCache.clearCache(true); // true=chrome
  imageCache.clearCache(false); // false=content
}

function cacheDataForContext(loadContextInfo) {
  return new Promise(resolve => {
    let cacheEntries = [];
    let cacheVisitor = {
      onCacheStorageInfo(num, consumption) {},
      onCacheEntryInfo(uri, idEnhance) {
        cacheEntries.push({ uri, idEnhance });
      },
      onCacheEntryVisitCompleted() {
        resolve(cacheEntries);
      },
      QueryInterface: ChromeUtils.generateQI(["nsICacheStorageVisitor"]),
    };
    // Visiting the disk cache also visits memory storage so we do not
    // need to use Services.cache2.memoryCacheStorage() here.
    let storage = Services.cache2.diskCacheStorage(loadContextInfo, false);
    storage.asyncVisitStorage(cacheVisitor, true);
  });
}

let countMatchingCacheEntries = function(cacheEntries, domain, fileSuffix) {
  return cacheEntries
    .map(entry => entry.uri.asciiSpec)
    .filter(spec => spec.includes(domain))
    .filter(spec => spec.includes("file_thirdPartyChild." + fileSuffix)).length;
};

function observeChannels(onChannel) {
  // We use a dummy proxy filter to catch all channels, even those that do not
  // generate an "http-on-modify-request" notification, such as link preconnects.
  let proxyFilter = {
    applyFilter(aChannel, aProxy, aCallback) {
      // We have the channel; provide it to the callback.
      onChannel(aChannel);
      // Pass on aProxy unmodified.
      aCallback.onProxyFilterResult(aProxy);
    },
  };
  protocolProxyService.registerChannelFilter(proxyFilter, 0);
  // Return the stop() function:
  return () => protocolProxyService.unregisterChannelFilter(proxyFilter);
}

function startObservingChannels(aMode) {
  let stopObservingChannels = observeChannels(function(channel) {
    let originalURISpec = channel.originalURI.spec;
    if (originalURISpec.includes("example.net")) {
      let loadInfo = channel.loadInfo;

      switch (aMode) {
        case TEST_MODE_FIRSTPARTY:
          ok(
            loadInfo.originAttributes.firstPartyDomain === "example.com" ||
              loadInfo.originAttributes.firstPartyDomain === "example.org",
            "first party for " +
              originalURISpec +
              " is " +
              loadInfo.originAttributes.firstPartyDomain
          );
          break;

        case TEST_MODE_NO_ISOLATION:
          ok(
            ChromeUtils.isOriginAttributesEqual(
              loadInfo.originAttributes,
              ChromeUtils.fillNonDefaultOriginAttributes()
            ),
            "OriginAttributes for " + originalURISpec + " is default."
          );
          break;

        case TEST_MODE_CONTAINERS:
          ok(
            loadInfo.originAttributes.userContextId === 1 ||
              loadInfo.originAttributes.userContextId === 2,
            "userContextId for " +
              originalURISpec +
              " is " +
              loadInfo.originAttributes.userContextId
          );
          break;

        default:
          ok(false, "Unknown test mode.");
      }
    }
  });
  return stopObservingChannels;
}

let stopObservingChannels;

// The init function, which clears image and network caches, and generates
// the random value for isolating video and audio elements across different
// test runs.
async function doInit(aMode) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.predictor.enabled", false],
      ["network.predictor.enable-prefetch", false],
    ],
  });
  clearAllImageCaches();

  Services.cache2.clear();

  randomSuffix = Math.random();
  stopObservingChannels = startObservingChannels(aMode);
}

// In the test function, we dynamically generate the video and audio element,
// and assign a random suffix to their URL to isolate them across different
// test runs.
async function doTest(aBrowser) {
  let argObj = {
    randomSuffix,
    urlPrefix: TEST_DOMAIN + TEST_PATH,
  };

  await SpecialPowers.spawn(aBrowser, [argObj], async function(arg) {
    let videoURL = arg.urlPrefix + "file_thirdPartyChild.video.ogv";
    let audioURL = arg.urlPrefix + "file_thirdPartyChild.audio.ogg";
    let trackURL = arg.urlPrefix + "file_thirdPartyChild.track.vtt";
    let URLSuffix = "?r=" + arg.randomSuffix;

    // Create the audio and video elements.
    let audio = content.document.createElement("audio");
    let video = content.document.createElement("video");
    let audioSource = content.document.createElement("source");
    let audioTrack = content.document.createElement("track");

    // Append the audio and track element into the body, and wait until they're finished.
    await new content.Promise(resolve => {
      let audioLoaded = false;
      let trackLoaded = false;

      let audioListener = () => {
        Assert.ok(true, `Audio suspended: ${audioURL + URLSuffix}`);
        audio.removeEventListener("suspend", audioListener);

        audioLoaded = true;
        if (audioLoaded && trackLoaded) {
          resolve();
        }
      };

      let trackListener = () => {
        Assert.ok(true, `Audio track loaded: ${audioURL + URLSuffix}`);
        audioTrack.removeEventListener("load", trackListener);

        trackLoaded = true;
        if (audioLoaded && trackLoaded) {
          resolve();
        }
      };

      Assert.ok(true, `Loading audio: ${audioURL + URLSuffix}`);

      // Add the event listeners before everything in case we lose events.
      audioTrack.addEventListener("load", trackListener);
      audio.addEventListener("suspend", audioListener);

      // Assign attributes for the audio element.
      audioSource.setAttribute("src", audioURL + URLSuffix);
      audioSource.setAttribute("type", "audio/ogg");
      audioTrack.setAttribute("src", trackURL);
      audioTrack.setAttribute("kind", "subtitles");
      audioTrack.setAttribute("default", true);

      audio.appendChild(audioSource);
      audio.appendChild(audioTrack);
      audio.autoplay = true;

      content.document.body.appendChild(audio);
    });

    // Append the video element into the body, and wait until it's finished.
    await new content.Promise(resolve => {
      let listener = () => {
        Assert.ok(true, `Video suspended: ${videoURL + URLSuffix}`);
        video.removeEventListener("suspend", listener);
        resolve();
      };

      Assert.ok(true, `Loading video: ${videoURL + URLSuffix}`);

      // Add the event listener before everything in case we lose the event.
      video.addEventListener("suspend", listener);

      // Assign attributes for the video element.
      video.setAttribute("src", videoURL + URLSuffix);
      video.setAttribute("type", "video/ogg");

      content.document.body.appendChild(video);
    });
  });

  return 0;
}

// The check function, which checks the number of cache entries.
async function doCheck(aShouldIsolate, aInputA, aInputB) {
  let expectedEntryCount = 1;
  let data = [];
  data = data.concat(
    await cacheDataForContext(Services.loadContextInfo.default)
  );
  data = data.concat(
    await cacheDataForContext(Services.loadContextInfo.private)
  );
  data = data.concat(
    await cacheDataForContext(Services.loadContextInfo.custom(true, {}))
  );
  data = data.concat(
    await cacheDataForContext(
      Services.loadContextInfo.custom(false, { userContextId: 1 })
    )
  );
  data = data.concat(
    await cacheDataForContext(
      Services.loadContextInfo.custom(true, { userContextId: 1 })
    )
  );
  data = data.concat(
    await cacheDataForContext(
      Services.loadContextInfo.custom(false, { userContextId: 2 })
    )
  );
  data = data.concat(
    await cacheDataForContext(
      Services.loadContextInfo.custom(true, { userContextId: 2 })
    )
  );
  data = data.concat(
    await cacheDataForContext(
      Services.loadContextInfo.custom(false, {
        firstPartyDomain: "example.com",
      })
    )
  );
  data = data.concat(
    await cacheDataForContext(
      Services.loadContextInfo.custom(true, { firstPartyDomain: "example.com" })
    )
  );
  data = data.concat(
    await cacheDataForContext(
      Services.loadContextInfo.custom(false, {
        firstPartyDomain: "example.org",
      })
    )
  );
  data = data.concat(
    await cacheDataForContext(
      Services.loadContextInfo.custom(true, { firstPartyDomain: "example.org" })
    )
  );

  if (aShouldIsolate) {
    expectedEntryCount = 2;
  }

  for (let suffix of suffixes) {
    let foundEntryCount = countMatchingCacheEntries(
      data,
      "example.net",
      suffix
    );
    let result = expectedEntryCount === foundEntryCount;
    ok(
      result,
      "Cache entries expected for " +
        suffix +
        ": " +
        expectedEntryCount +
        ", and found " +
        foundEntryCount
    );
  }

  stopObservingChannels();
  stopObservingChannels = undefined;
  return true;
}

let testArgs = {
  url: TEST_PAGE,
  firstFrameSetting: DEFAULT_FRAME_SETTING,
  secondFrameSetting: [TEST_TYPE_FRAME],
};

IsolationTestTools.runTests(testArgs, doTest, doCheck, doInit);
