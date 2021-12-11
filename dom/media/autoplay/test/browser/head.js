/**
 * Return a web-based URL for a given file based on the testing directory.
 * @param {String} fileName
 *        file that caller wants its web-based url
 * @param {Boolean} crossOrigin [optional]
 *        if set, then return a url with different origin. The default value is
 *        false.
 */
function GetTestWebBasedURL(fileName, { crossOrigin = false } = {}) {
  const origin = crossOrigin ? "http://example.org" : "http://example.com";
  return (
    getRootDirectory(gTestPath).replace("chrome://mochitests/content", origin) +
    fileName
  );
}

/**
 * Runs a content script that creates an autoplay video.
 * @param {browserElement} browser
 *        the browser to run the script in
 * @param {object} args
 *        test case definition, required members
 *        {
 *          mode: String, "autoplay attribute" or "call play".
 *        }
 */
function loadAutoplayVideo(browser, args) {
  return SpecialPowers.spawn(browser, [args], async args => {
    info("- create a new autoplay video -");
    let video = content.document.createElement("video");
    video.id = "v1";
    video.didPlayPromise = new Promise((resolve, reject) => {
      video.addEventListener(
        "playing",
        e => {
          video.didPlay = true;
          resolve();
        },
        { once: true }
      );
      video.addEventListener(
        "blocked",
        e => {
          video.didPlay = false;
          resolve();
        },
        { once: true }
      );
    });
    if (args.mode == "autoplay attribute") {
      info("autoplay attribute set to true");
      video.autoplay = true;
    } else if (args.mode == "call play") {
      info("will call play() when reached loadedmetadata");
      video.addEventListener(
        "loadedmetadata",
        e => {
          video.play().then(
            () => {
              info("video play() resolved");
            },
            () => {
              info("video play() rejected");
            }
          );
        },
        { once: true }
      );
    } else {
      ok(false, "Invalid 'mode' arg");
    }
    video.src = "gizmo.mp4";
    content.document.body.appendChild(video);
  });
}

/**
 * Runs a content script that checks whether the video created by
 * loadAutoplayVideo() started playing.
 * @param {browserElement} browser
 *        the browser to run the script in
 * @param {object} args
 *        test case definition, required members
 *        {
 *          name: String, description of test.
 *          mode: String, "autoplay attribute" or "call play".
 *          shouldPlay: boolean, whether video should play.
 *        }
 */
function checkVideoDidPlay(browser, args) {
  return SpecialPowers.spawn(browser, [args], async args => {
    let video = content.document.getElementById("v1");
    await video.didPlayPromise;
    is(
      video.didPlay,
      args.shouldPlay,
      args.name +
        " should " +
        (!args.shouldPlay ? "not " : "") +
        "be able to autoplay"
    );
    video.src = "";
    content.document.body.remove(video);
  });
}
