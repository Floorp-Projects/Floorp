/* eslint-disable no-undef */

async function createTabAndLoad(url) {
  let tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser, url);
  return tab;
}

async function checkOrWaitUntilMediaStartedPlaying(tab) {
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    return new Promise(resolve => {
      const video = content.document.getElementById("autoplay");
      if (!video) {
        ok(false, `can't get the media element!`);
      }
      if (!video.paused) {
        ok(true, `media started playing`);
        resolve();
      } else {
        info(`wait until media starts playing`);
        video.onplaying = () => {
          video.onplaying = null;
          ok(true, `media started playing`);
          resolve();
        };
      }
    });
  });
}

async function checkOrWaitUntilMediaStoppedPlaying(tab) {
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    return new Promise(resolve => {
      const video = content.document.getElementById("autoplay");
      if (!video) {
        ok(false, `can't get the media element!`);
      }
      if (video.paused) {
        ok(true, `media stopped playing`);
        resolve();
      } else {
        info(`wait until media stops playing`);
        video.onpause = () => {
          video.onpause = null;
          ok(true, `media stopped playing`);
          resolve();
        };
      }
    });
  });
}
