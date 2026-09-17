/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { ActorManagerParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ActorManagerParent.sys.mjs",
);

function localPathToResourceURI(path: string) {
  const re = new RegExp(/\.\.\/([a-zA-Z0-9-_/]+)\.sys\.mts/);
  const result = re.exec(path);
  if (!result || result.length != 2) {
    throw Error(
      `[nora-browserGlue] localPathToResource URI match failed : ${path}`,
    );
  }
  const resourceURI = `resource://noraneko/${result[1]}.sys.mjs`;
  return resourceURI;
}

const STARTUP_MODE = Services.prefs.getStringPref("nora.startup.mode", "");
const IS_LOCAL_DEVELOPMENT_MODE = STARTUP_MODE === "dev" ||
  STARTUP_MODE === "test";
const DEVELOPMENT_LOCALHOST_MATCHES = IS_LOCAL_DEVELOPMENT_MODE
  ? ["*://localhost/*"]
  : [];
const DEVELOPMENT_LOOPBACK_MATCHES = IS_LOCAL_DEVELOPMENT_MODE
  ? ["*://localhost/*", "*://127.0.0.1/*"]
  : [];
const WEB_REMOTE_TYPES = ["web", "webIsolated", "webCOOP+COEP"];
const WEB_FILE_AND_ABOUT_REMOTE_TYPES = [
  ...WEB_REMOTE_TYPES,
  "file",
  "privilegedabout",
  "parent",
];
const DEVELOPMENT_WEB_ACTOR_OPTIONS: Partial<WindowActorOptions> =
  IS_LOCAL_DEVELOPMENT_MODE
    ? {
      // Firefox 154 treats even loopback documents as untrusted web-process
      // content. These options exist only in local development/test modes,
      // where the
      // matching Vite pages are part of the local Floorp development setup.
      remoteTypes: WEB_FILE_AND_ABOUT_REMOTE_TYPES,
      safeForUntrustedWebProcess: true,
    }
    : {};

const JS_WINDOW_ACTORS: {
  [k: string]: WindowActorOptions;
} = {
  NRAboutPreferences: {
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRAboutPreferencesChild.sys.mts",
      ),
      events: {
        DOMContentLoaded: {},
        DOMDocElementInserted: {},
      },
    },
    matches: ["about:preferences*", "about:settings*"],
  },
  NRSettings: {
    parent: {
      esModuleURI: localPathToResourceURI("../actors/NRSettingsParent.sys.mts"),
    },
    child: {
      esModuleURI: localPathToResourceURI("../actors/NRSettingsChild.sys.mts"),
      // Vite dev pages run in a webIsolated remote type (e.g.
      // "webIsolated=http://localhost"); without this the actor is never
      // instantiated and window.NRSettingsRegisterReceiveCallback is never
      // exported (see rpc.ts:56 "is not a function").
      events: {
        /**
         * actorCreated seems to require any of events for init
         */
        DOMDocElementInserted: {},
        DOMContentLoaded: {},
        load: {},
        pageshow: {},
      },
    },
    //* port seems to not be supported
    //https://searchfox.org/mozilla-central/rev/3966e5534ddf922b186af4777051d579fd052bad/dom/chrome-webidl/JSWindowActor.webidl#99
    //https://searchfox.org/mozilla-central/rev/3966e5534ddf922b186af4777051d579fd052bad/dom/chrome-webidl/MatchPattern.webidl#17
    matches: [
      ...DEVELOPMENT_LOOPBACK_MATCHES,
      // Keep settings actor matching limited to loopback development pages.
      // Ordinary HTTP pages must not instantiate this privileged bridge.
      // The packaged settings chrome route remains available for production.
      "chrome://noraneko-settings/*",
    ],
    ...DEVELOPMENT_WEB_ACTOR_OPTIONS,
  },
  NRExperimemmt: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRExperimemmtParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRExperimemmtChild.sys.mts",
      ),
      events: {
        DOMDocElementInserted: {},
      },
    },
    matches: [
      ...DEVELOPMENT_LOCALHOST_MATCHES,
      "chrome://noraneko-settings/*",
      "about:*",
    ],
    ...DEVELOPMENT_WEB_ACTOR_OPTIONS,
  },
  NRPanelSidebar: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRPanelSidebarParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRPanelSidebarChild.sys.mts",
      ),
      events: {
        DOMDocElementInserted: {},
      },
    },
    matches: [
      ...DEVELOPMENT_LOCALHOST_MATCHES,
      "chrome://noraneko-settings/*",
      "about:*",
    ],
    ...DEVELOPMENT_WEB_ACTOR_OPTIONS,
  },
  NRTabManager: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRTabManagerParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRTabManagerChild.sys.mts",
      ),
      events: {
        DOMDocElementInserted: {},
      },
    },
    matches: [
      ...DEVELOPMENT_LOCALHOST_MATCHES,
      "chrome://noraneko-settings/*",
      "about:*",
    ],
    ...DEVELOPMENT_WEB_ACTOR_OPTIONS,
  },
  NRSyncManager: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRSyncManagerParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRSyncManagerChild.sys.mts",
      ),
      events: {
        DOMDocElementInserted: {},
      },
    },
    matches: [
      ...DEVELOPMENT_LOCALHOST_MATCHES,
      "chrome://noraneko-settings/*",
      "about:*",
    ],
    ...DEVELOPMENT_WEB_ACTOR_OPTIONS,
  },
  NRAppConstants: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRAppConstantsParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRAppConstantsChild.sys.mts",
      ),
      events: {
        DOMDocElementInserted: {},
      },
    },
    matches: [
      ...DEVELOPMENT_LOCALHOST_MATCHES,
      "chrome://noraneko-settings/*",
      "about:*",
    ],
    ...DEVELOPMENT_WEB_ACTOR_OPTIONS,
  },
  NRRestartBrowser: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRRestartBrowserParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRRestartBrowserChild.sys.mts",
      ),
      events: {
        DOMDocElementInserted: {},
      },
    },
    matches: [
      ...DEVELOPMENT_LOCALHOST_MATCHES,
      "chrome://noraneko-settings/*",
      "about:*",
    ],
    ...DEVELOPMENT_WEB_ACTOR_OPTIONS,
  },
  NRWorkspaces: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRWorkspacesParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRWorkspacesChild.sys.mts",
      ),
      events: {
        DOMDocElementInserted: {},
      },
    },
    matches: [
      ...DEVELOPMENT_LOCALHOST_MATCHES,
      "chrome://noraneko-settings/*",
      "about:*",
    ],
    ...DEVELOPMENT_WEB_ACTOR_OPTIONS,
  },
  NRProgressiveWebApp: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRProgressiveWebAppParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRProgressiveWebAppChild.sys.mts",
      ),
      events: {
        pageshow: {},
      },
    },
    matches: ["http://*/*", "https://*/*"],
    remoteTypes: WEB_REMOTE_TYPES,
    safeForUntrustedWebProcess: true,
    allFrames: true,
  },
  NRPwaManager: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRPwaManagerParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRPwaManagerChild.sys.mts",
      ),
      events: {
        DOMDocElementInserted: {},
      },
    },
    matches: [
      ...DEVELOPMENT_LOCALHOST_MATCHES,
      "chrome://noraneko-settings/*",
      "about:hub*",
    ],
    ...DEVELOPMENT_WEB_ACTOR_OPTIONS,
  },
  NRChromeModal: {
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRChromeModalChild.sys.mts",
      ),
      events: {
        DOMContentLoaded: {},
      },
    },
    matches: [
      ...DEVELOPMENT_LOCALHOST_MATCHES,
      "chrome://noraneko-modal-child/*",
    ],
    ...DEVELOPMENT_WEB_ACTOR_OPTIONS,
  },
  NRProfileManager: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRProfileManagerParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRProfileManagerChild.sys.mts",
      ),
      events: {
        DOMDocElementInserted: {},
      },
    },
    matches: [
      ...DEVELOPMENT_LOCALHOST_MATCHES,
      "chrome://noraneko-settings/*",
      "chrome://noraneko-profile-manager/*",
      "about:*",
    ],
    ...DEVELOPMENT_WEB_ACTOR_OPTIONS,
  },

  NRStartPage: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRStartPageParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI("../actors/NRStartPageChild.sys.mts"),
      events: {
        DOMContentLoaded: {},
      },
    },
    matches: [
      ...DEVELOPMENT_LOCALHOST_MATCHES,
      "chrome://noraneko-newtab/*",
      "about:*",
    ],
    ...DEVELOPMENT_WEB_ACTOR_OPTIONS,
  },

  NRWelcomePage: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRWelcomePageParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRWelcomePageChild.sys.mts",
      ),
      events: {
        DOMContentLoaded: {},
      },
    },
    matches: [
      ...DEVELOPMENT_LOCALHOST_MATCHES,
      "chrome://noraneko-welcome/*",
      "about:*",
    ],
    ...DEVELOPMENT_WEB_ACTOR_OPTIONS,
  },

  NRSearchEngine: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRSearchEngineParent.sys.mts",
      ),
    },

    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRSearchEngineChild.sys.mts",
      ),
      events: {
        DOMContentLoaded: {},
      },
    },

    matches: [
      ...DEVELOPMENT_LOCALHOST_MATCHES,
      "chrome://noraneko-welcome/*",
      "chrome://noraneko-newtab/*",
      "about:*",
    ],
    ...DEVELOPMENT_WEB_ACTOR_OPTIONS,
  },

  NRWebScraper: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRWebScraperParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRWebScraperChild.sys.mts",
      ),
      events: {
        DOMContentLoaded: {},
        DOMDocElementInserted: {},
      },
    },
    matches: ["http://*/*", "https://*/*", "file:///*", "about:*"],
    remoteTypes: WEB_FILE_AND_ABOUT_REMOTE_TYPES,
    safeForUntrustedWebProcess: true,
    allFrames: true,
  },
  NROSAutomotor: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NROSAutomotorParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NROSAutomotorChild.sys.mts",
      ),
      events: {
        DOMContentLoaded: {},
        DOMDocElementInserted: {},
      },
    },
    matches: [
      ...DEVELOPMENT_LOCALHOST_MATCHES,
      "chrome://noraneko-settings/*",
      "about:*",
    ],
    ...DEVELOPMENT_WEB_ACTOR_OPTIONS,
  },
  NRI18n: {
    parent: {
      esModuleURI: localPathToResourceURI("../actors/NRI18nParent.sys.mts"),
    },
    child: {
      esModuleURI: localPathToResourceURI("../actors/NRI18nChild.sys.mts"),
      events: {
        DOMContentLoaded: {},
      },
    },
    matches: [
      ...DEVELOPMENT_LOCALHOST_MATCHES,
      "chrome://noraneko-settings/*",
      "chrome://noraneko-profile-manager/*",
      "about:*",
    ],
    ...DEVELOPMENT_WEB_ACTOR_OPTIONS,
  },
  NRChromeWebStore: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRChromeWebStoreParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRChromeWebStoreChild.sys.mts",
      ),
      events: {
        DOMContentLoaded: {},
      },
    },
    matches: [
      "https://chromewebstore.google.com/*",
      "https://chrome.google.com/webstore/*",
    ],
    // Firefox 154 blocks privileged actors in web content processes unless
    // they explicitly opt in. Limit this actor to web remote types and the
    // Chrome Web Store origins above before enabling it for those processes.
    remoteTypes: WEB_REMOTE_TYPES,
    safeForUntrustedWebProcess: true,
    allFrames: true,
  },
  NRPluginStore: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRPluginStoreParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRPluginStoreChild.sys.mts",
      ),
      events: {
        DOMContentLoaded: {},
      },
    },
    matches: [
      // Floorp OS Plugin Store domains
      "https://plugins.floorp.app/*",
      "https://store.floorp.app/*",
      // Development domains are omitted in production startup mode.
      ...DEVELOPMENT_LOOPBACK_MATCHES,
    ],
    remoteTypes: WEB_REMOTE_TYPES,
    safeForUntrustedWebProcess: true,
  },
  NRKeyboardShortcutFocus: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRKeyboardShortcutFocusParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRKeyboardShortcutFocusChild.sys.mts",
      ),
      events: {
        DOMContentLoaded: {},
        focusin: { capture: true },
        focusout: { capture: true },
        blur: { capture: true },
        pageshow: {},
        pagehide: {},
      },
    },
    matches: ["http://*/*", "https://*/*", "file:///*", "about:*"],
    remoteTypes: WEB_FILE_AND_ABOUT_REMOTE_TYPES,
    safeForUntrustedWebProcess: true,
    allFrames: true,
  },
  NRMouseGestureScroll: {
    parent: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRMouseGestureScrollParent.sys.mts",
      ),
    },
    child: {
      esModuleURI: localPathToResourceURI(
        "../actors/NRMouseGestureScrollChild.sys.mts",
      ),
    },
    matches: ["http://*/*", "https://*/*", "file:///*", "about:*"],
    remoteTypes: WEB_FILE_AND_ABOUT_REMOTE_TYPES,
    allFrames: true,
    // This actor only performs validated DOM scrolling in content. Runtime
    // 154 rejects actors without this opt-in from web/webIsolated processes.
    safeForUntrustedWebProcess: true,
  },
};

ActorManagerParent.addJSWindowActors(JS_WINDOW_ACTORS);
