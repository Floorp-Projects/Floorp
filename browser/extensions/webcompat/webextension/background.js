/**
 * For detailed information on our policies, and a documention on this format
 * and its possibilites, please check the Mozilla-Wiki at
 *
 * https://wiki.mozilla.org/Compatibility/Go_Faster_Addon/Override_Policies_and_Workflows#User_Agent_overrides
 */
const contentScripts = [
  {
    matches: ["*://webcompat-addon-testcases.schub.io/*"],
    css: [{file: "injections/css/bug0000000-dummy-css-injection.css"}],
    js: [{file: "injections/js/bug0000000-dummy-js-injection.js"}],
    runAt: "document_start"
  }
];

/* globals browser */

let port = browser.runtime.connect();
let registeredContentScripts = [];

function registerContentScripts() {
  contentScripts.forEach(async (contentScript) => {
    try {
      let handle = await browser.contentScripts.register(contentScript);
      registeredContentScripts.push(handle);
    } catch (ex) {
      console.error("Registering WebCompat GoFaster content scripts failed: ", ex);
    }
  });
}

function unregisterContentScripts() {
  registeredContentScripts.forEach((contentScript) => {
    contentScript.unregister();
  });
}

port.onMessage.addListener((message) => {
  switch (message.type) {
    case "injection-pref-changed":
      if (message.prefState) {
        registerContentScripts();
      } else {
        unregisterContentScripts();
      }
      break;
  }
});

/**
 * Note that we reset all preferences on extension startup, so the injections will
 * never be disabled when this loads up. Because of that, we can simply register
 * right away.
 */
registerContentScripts();
