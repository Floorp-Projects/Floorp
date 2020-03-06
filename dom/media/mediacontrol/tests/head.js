/* eslint-disable no-undef */

async function createTabAndLoad(url) {
  let tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser, url);
  return tab;
}

function checkOrWaitUntilMediaStartedPlaying(tab, elementId) {
  return SpecialPowers.spawn(tab.linkedBrowser, [elementId], Id => {
    return new Promise(resolve => {
      const video = content.document.getElementById(Id);
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

function checkOrWaitUntilMediaStoppedPlaying(tab, elementId) {
  return SpecialPowers.spawn(tab.linkedBrowser, [elementId], Id => {
    return new Promise(resolve => {
      const video = content.document.getElementById(Id);
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

function waitUntilMainMediaControllerPlaybackChanged() {
  return BrowserUtils.promiseObserved("main-media-controller-playback-changed");
}
