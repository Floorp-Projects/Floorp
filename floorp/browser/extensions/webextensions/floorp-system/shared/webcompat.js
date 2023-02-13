const WEBCOMPATS = [
  /*
  {
    "matches": ["*://*.youtube.com/*"],
    "js": [
      { file: "webcompat/fix-youtube-comment.js" }
    ],
    "platforms": ["win"], // "mac", "win", "android", "cros", "linux", "openbsd", "fuchsia"
  }
  */
]

let REGISTED_CONTENT_SCRIPTS = [];

async function regist_webcompat_contentScripts() {
  let platform = (await browser.runtime.getPlatformInfo()).os;
  for (let WEBCOMPAT of WEBCOMPATS) {
    if (WEBCOMPAT.platforms.includes(platform)) {
      let WEBCOMPAT_cloned = Object.assign({}, WEBCOMPAT);
      delete WEBCOMPAT_cloned.platforms;
      let registeredContentScript = await browser.contentScripts.register(WEBCOMPAT_cloned);
      REGISTED_CONTENT_SCRIPTS.push(registeredContentScript);
    }
  }
}

async function unregist_webcompat_contentScripts() {
  for (let REGISTED_CONTENT_SCRIPT of REGISTED_CONTENT_SCRIPTS) {
    REGISTED_CONTENT_SCRIPT.unregister();
  }
  REGISTED_CONTENT_SCRIPTS = [];
}

(async() => {
  //unregist_webcompat_contentScripts();
  await regist_webcompat_contentScripts();
})();
