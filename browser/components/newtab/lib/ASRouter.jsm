/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);
XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  UITour: "resource:///modules/UITour.jsm",
  FxAccounts: "resource://gre/modules/FxAccounts.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  BookmarkPanelHub: "resource://activity-stream/lib/BookmarkPanelHub.jsm",
  SnippetsTestMessageProvider:
    "resource://activity-stream/lib/SnippetsTestMessageProvider.jsm",
  PanelTestProvider: "resource://activity-stream/lib/PanelTestProvider.jsm",
  ToolbarBadgeHub: "resource://activity-stream/lib/ToolbarBadgeHub.jsm",
});
const {
  ASRouterActions: ra,
  actionTypes: at,
  actionCreators: ac,
} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm");
const { CFRMessageProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/CFRMessageProvider.jsm"
);
const { OnboardingMessageProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/OnboardingMessageProvider.jsm"
);
const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { CFRPageActions } = ChromeUtils.import(
  "resource://activity-stream/lib/CFRPageActions.jsm"
);
const { AttributionCode } = ChromeUtils.import(
  "resource:///modules/AttributionCode.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "ASRouterPreferences",
  "resource://activity-stream/lib/ASRouterPreferences.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TARGETING_PREFERENCES",
  "resource://activity-stream/lib/ASRouterPreferences.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ASRouterTargeting",
  "resource://activity-stream/lib/ASRouterTargeting.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "QueryCache",
  "resource://activity-stream/lib/ASRouterTargeting.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ASRouterTriggerListeners",
  "resource://activity-stream/lib/ASRouterTriggerListeners.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TelemetryEnvironment",
  "resource://gre/modules/TelemetryEnvironment.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ClientEnvironment",
  "resource://normandy/lib/ClientEnvironment.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Sampling",
  "resource://gre/modules/components-utils/Sampling.jsm"
);

const TRAILHEAD_CONFIG = {
  OVERRIDE_PREF: "trailhead.firstrun.branches",
  DID_SEE_ABOUT_WELCOME_PREF: "trailhead.firstrun.didSeeAboutWelcome",
  INTERRUPTS_EXPERIMENT_PREF: "trailhead.firstrun.interruptsExperiment",
  TRIPLETS_ENROLLED_PREF: "trailhead.firstrun.tripletsEnrolled",
  BRANCHES: {
    interrupts: [["control"], ["join"], ["sync"], ["nofirstrun"], ["cards"]],
    triplets: [["supercharge"], ["payoff"], ["multidevice"], ["privacy"]],
  },
  LOCALES: ["en-US", "en-GB", "en-CA", "de", "de-DE", "fr", "fr-FR"],
  EXPERIMENT_RATIOS: [["", 0], ["interrupts", 1], ["triplets", 3]],
  // Per bug 1574003, for those who meet the targeting criteria of extended
  // triplets, 95% users (control group) will see the extended triplets, and
  // the rest 5% (holdback group) won't.
  EXPERIMENT_RATIOS_FOR_EXTENDED_TRIPLETS: [["control", 95], ["holdback", 5]],
  EXTENDED_TRIPLETS_EXPERIMENT_PREF: "trailhead.extendedTriplets.experiment",
};

const INCOMING_MESSAGE_NAME = "ASRouter:child-to-parent";
const OUTGOING_MESSAGE_NAME = "ASRouter:parent-to-child";
const ONE_DAY_IN_MS = 24 * 60 * 60 * 1000;
// List of hosts for endpoints that serve router messages.
// Key is allowed host, value is a name for the endpoint host.
const DEFAULT_WHITELIST_HOSTS = {
  "activity-stream-icons.services.mozilla.com": "production",
  "snippets-admin.mozilla.org": "preview",
};
const SNIPPETS_ENDPOINT_WHITELIST =
  "browser.newtab.activity-stream.asrouter.whitelistHosts";
// Max possible impressions cap for any message
const MAX_MESSAGE_LIFETIME_CAP = 100;

const LOCAL_MESSAGE_PROVIDERS = {
  OnboardingMessageProvider,
  CFRMessageProvider,
};
const STARTPAGE_VERSION = "6";

/**
 * chooseBranch<T> -  Choose an item from a list of "branches" pseudorandomly using a seed / ratio configuration
 * @param seed {string} A unique seed for the randomizer
 * @param branches {Array<[T, number?]>} A list of branches, where branch[0] is any item and branch[1] is the ratio
 * @returns {T} An randomly chosen item in a branch
 */
async function chooseBranch(seed, branches) {
  const ratios = branches.map(([item, ratio]) =>
    typeof ratio !== "undefined" ? ratio : 1
  );
  return branches[await Sampling.ratioSample(seed, ratios)][0];
}

const MessageLoaderUtils = {
  STARTPAGE_VERSION,
  REMOTE_LOADER_CACHE_KEY: "RemoteLoaderCache",
  _errors: [],

  reportError(e) {
    Cu.reportError(e);
    this._errors.push({
      timestamp: new Date(),
      error: { message: e.toString(), stack: e.stack },
    });
  },

  get errors() {
    const errors = this._errors;
    this._errors = [];
    return errors;
  },

  /**
   * _localLoader - Loads messages for a local provider (i.e. one that lives in mozilla central)
   *
   * @param {obj} provider An AS router provider
   * @param {Array} provider.messages An array of messages
   * @returns {Array} the array of messages
   */
  _localLoader(provider) {
    return provider.messages;
  },

  async _remoteLoaderCache(storage) {
    let allCached;
    try {
      allCached =
        (await storage.get(MessageLoaderUtils.REMOTE_LOADER_CACHE_KEY)) || {};
    } catch (e) {
      // istanbul ignore next
      MessageLoaderUtils.reportError(e);
      // istanbul ignore next
      allCached = {};
    }
    return allCached;
  },

  /**
   * _remoteLoader - Loads messages for a remote provider
   *
   * @param {obj} provider An AS router provider
   * @param {string} provider.url An endpoint that returns an array of messages as JSON
   * @param {obj} options.storage A storage object with get() and set() methods for caching.
   * @returns {Promise} resolves with an array of messages, or an empty array if none could be fetched
   */
  async _remoteLoader(provider, options) {
    let remoteMessages = [];
    if (provider.url) {
      const allCached = await MessageLoaderUtils._remoteLoaderCache(
        options.storage
      );
      const cached = allCached[provider.id];
      let etag;

      if (
        cached &&
        cached.url === provider.url &&
        cached.version === STARTPAGE_VERSION
      ) {
        const { lastFetched, messages } = cached;
        if (
          !MessageLoaderUtils.shouldProviderUpdate({
            ...provider,
            lastUpdated: lastFetched,
          })
        ) {
          // Cached messages haven't expired, return early.
          return messages;
        }
        etag = cached.etag;
        remoteMessages = messages;
      }

      let headers = new Headers();
      if (etag) {
        headers.set("If-None-Match", etag);
      }

      let response;
      try {
        response = await fetch(provider.url, { headers, credentials: "omit" });
      } catch (e) {
        MessageLoaderUtils.reportError(e);
      }
      if (
        response &&
        response.ok &&
        (response.status >= 200 && response.status < 400)
      ) {
        let jsonResponse;
        try {
          jsonResponse = await response.json();
        } catch (e) {
          MessageLoaderUtils.reportError(e);
          return remoteMessages;
        }
        if (jsonResponse && jsonResponse.messages) {
          remoteMessages = jsonResponse.messages.map(msg => ({
            ...msg,
            provider_url: provider.url,
          }));

          // Cache the results if this isn't a preview URL.
          if (provider.updateCycleInMs > 0) {
            etag = response.headers.get("ETag");
            const cacheInfo = {
              messages: remoteMessages,
              etag,
              lastFetched: Date.now(),
              version: STARTPAGE_VERSION,
            };

            options.storage.set(MessageLoaderUtils.REMOTE_LOADER_CACHE_KEY, {
              ...allCached,
              [provider.id]: cacheInfo,
            });
          }
        } else {
          MessageLoaderUtils.reportError(
            `No messages returned from ${provider.url}.`
          );
        }
      } else if (response) {
        MessageLoaderUtils.reportError(
          `Invalid response status ${response.status} from ${provider.url}.`
        );
      }
    }
    return remoteMessages;
  },

  /**
   * _remoteSettingsLoader - Loads messages for a RemoteSettings provider
   *
   * @param {obj} provider An AS router provider
   * @param {string} provider.id The id of the provider
   * @param {string} provider.bucket The name of the Remote Settings bucket
   * @param {func} options.dispatchToAS dispatch an action the main AS Store
   * @returns {Promise} resolves with an array of messages, or an empty array if none could be fetched
   */
  async _remoteSettingsLoader(provider, options) {
    let messages = [];
    if (provider.bucket) {
      try {
        messages = await MessageLoaderUtils._getRemoteSettingsMessages(
          provider.bucket
        );
        if (!messages.length) {
          MessageLoaderUtils._handleRemoteSettingsUndesiredEvent(
            "ASR_RS_NO_MESSAGES",
            provider.id,
            options.dispatchToAS
          );
        }
      } catch (e) {
        MessageLoaderUtils._handleRemoteSettingsUndesiredEvent(
          "ASR_RS_ERROR",
          provider.id,
          options.dispatchToAS
        );
        MessageLoaderUtils.reportError(e);
      }
    }
    return messages;
  },

  _getRemoteSettingsMessages(bucket) {
    return RemoteSettings(bucket).get();
  },

  _handleRemoteSettingsUndesiredEvent(event, providerId, dispatchToAS) {
    if (dispatchToAS) {
      dispatchToAS(
        ac.ASRouterUserEvent({
          action: "asrouter_undesired_event",
          event,
          message_id: "n/a",
          value: providerId,
        })
      );
    }
  },

  /**
   * _getMessageLoader - return the right loading function given the provider's type
   *
   * @param {obj} provider An AS Router provider
   * @returns {func} A loading function
   */
  _getMessageLoader(provider) {
    switch (provider.type) {
      case "remote":
        return this._remoteLoader;
      case "remote-settings":
        return this._remoteSettingsLoader;
      case "local":
      default:
        return this._localLoader;
    }
  },

  /**
   * shouldProviderUpdate - Given the current time, should a provider update its messages?
   *
   * @param {any} provider An AS Router provider
   * @param {int} provider.updateCycleInMs The number of milliseconds we should wait between updates
   * @param {Date} provider.lastUpdated If the provider has been updated, the time the last update occurred
   * @param {Date} currentTime The time we should check against. (defaults to Date.now())
   * @returns {bool} Should an update happen?
   */
  shouldProviderUpdate(provider, currentTime = Date.now()) {
    return (
      !(provider.lastUpdated >= 0) ||
      currentTime - provider.lastUpdated > provider.updateCycleInMs
    );
  },

  /**
   * loadMessagesForProvider - Load messages for a provider, given the provider's type.
   *
   * @param {obj} provider An AS Router provider
   * @param {string} provider.type An AS Router provider type (defaults to "local")
   * @param {obj} options.storage A storage object with get() and set() methods for caching.
   * @param {func} options.dispatchToAS dispatch an action the main AS Store
   * @returns {obj} Returns an object with .messages (an array of messages) and .lastUpdated (the time the messages were updated)
   */
  async loadMessagesForProvider(provider, options) {
    const loader = this._getMessageLoader(provider);
    let messages = await loader(provider, options);
    // istanbul ignore if
    if (!messages) {
      messages = [];
      MessageLoaderUtils.reportError(
        new Error(
          `Tried to load messages for ${
            provider.id
          } but the result was not an Array.`
        )
      );
    }
    // Filter out messages we temporarily want to exclude
    if (provider.exclude && provider.exclude.length) {
      messages = messages.filter(
        message => !provider.exclude.includes(message.id)
      );
    }
    const lastUpdated = Date.now();
    return {
      messages: messages
        .map(msg => ({ weight: 100, ...msg, provider: provider.id }))
        .filter(message => message.weight > 0),
      lastUpdated,
      errors: MessageLoaderUtils.errors,
    };
  },

  /**
   * _loadAddonIconInURLBar - load addons-notification icon by displaying
   * box containing addons icon in urlbar. See Bug 1513882
   *
   * @param  {XULElement} Target browser element for showing addons icon
   */
  _loadAddonIconInURLBar(browser) {
    if (!browser) {
      return;
    }
    const chromeDoc = browser.ownerDocument;
    let notificationPopupBox = chromeDoc.getElementById(
      "notification-popup-box"
    );
    if (!notificationPopupBox) {
      return;
    }
    if (
      notificationPopupBox.style.display === "none" ||
      notificationPopupBox.style.display === ""
    ) {
      notificationPopupBox.style.display = "block";
    }
  },

  async installAddonFromURL(browser, url, telemetrySource = "amo") {
    try {
      MessageLoaderUtils._loadAddonIconInURLBar(browser);
      const aUri = Services.io.newURI(url);
      const systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();

      // AddonManager installation source associated to the addons installed from activitystream's CFR
      // and RTAMO (source is going to be "amo" if not configured explicitly in the message provider).
      const telemetryInfo = { source: telemetrySource };
      const install = await AddonManager.getInstallForURL(aUri.spec, {
        telemetryInfo,
      });
      await AddonManager.installAddonFromWebpage(
        "application/x-xpinstall",
        browser,
        systemPrincipal,
        install
      );
    } catch (e) {
      Cu.reportError(e);
    }
  },

  /**
   * cleanupCache - Removes cached data of removed providers.
   *
   * @param {Array} providers A list of activer AS Router providers
   */
  async cleanupCache(providers, storage) {
    const ids = providers.filter(p => p.type === "remote").map(p => p.id);
    const cache = await MessageLoaderUtils._remoteLoaderCache(storage);
    let dirty = false;
    for (let id in cache) {
      if (!ids.includes(id)) {
        delete cache[id];
        dirty = true;
      }
    }
    if (dirty) {
      await storage.set(MessageLoaderUtils.REMOTE_LOADER_CACHE_KEY, cache);
    }
  },
};

this.MessageLoaderUtils = MessageLoaderUtils;

/**
 * @class _ASRouter - Keeps track of all messages, UI surfaces, and
 * handles blocking, rotation, etc. Inspecting ASRouter.state will
 * tell you what the current displayed message is in all UI surfaces.
 *
 * Note: This is written as a constructor rather than just a plain object
 * so that it can be more easily unit tested.
 */
class _ASRouter {
  constructor(localProviders = LOCAL_MESSAGE_PROVIDERS) {
    this.initialized = false;
    this.messageChannel = null;
    this.dispatchToAS = null;
    this._storage = null;
    this._resetInitialization();
    this._state = {
      lastMessageId: null,
      providers: [],
      messageBlockList: [],
      providerBlockList: [],
      messageImpressions: {},
      providerImpressions: {},
      trailheadInitialized: false,
      trailheadInterrupt: "",
      trailheadTriplet: "",
      messages: [],
      errors: [],
      extendedTripletsInitialized: false,
      showExtendedTriplets: true,
    };
    this._triggerHandler = this._triggerHandler.bind(this);
    this._localProviders = localProviders;
    this.blockMessageById = this.blockMessageById.bind(this);
    this.unblockMessageById = this.unblockMessageById.bind(this);
    this.onMessage = this.onMessage.bind(this);
    this.handleMessageRequest = this.handleMessageRequest.bind(this);
    this.addImpression = this.addImpression.bind(this);
    this._handleTargetingError = this._handleTargetingError.bind(this);
    this.onPrefChange = this.onPrefChange.bind(this);
    this.dispatch = this.dispatch.bind(this);
  }

  async onPrefChange(prefName) {
    if (TARGETING_PREFERENCES.includes(prefName)) {
      // Notify all tabs of messages that have become invalid after pref change
      const invalidMessages = [];
      for (const msg of this._getUnblockedMessages()) {
        if (!msg.targeting) {
          continue;
        }
        const isMatch = await ASRouterTargeting.isMatch(msg.targeting);
        if (!isMatch) {
          invalidMessages.push(msg.id);
        }
      }
      this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {
        type: at.AS_ROUTER_TARGETING_UPDATE,
        data: invalidMessages,
      });
    } else {
      // Update message providers and fetch new messages on pref change
      this._loadLocalProviders();
      this._updateMessageProviders();
      await this.loadMessagesFromAllProviders();
    }
  }

  // Replace all frequency time period aliases with their millisecond values
  // This allows us to avoid accounting for special cases later on
  normalizeItemFrequency({ frequency }) {
    if (frequency && frequency.custom) {
      for (const setting of frequency.custom) {
        if (setting.period === "daily") {
          setting.period = ONE_DAY_IN_MS;
        }
      }
    }
  }

  // Fetch and decode the message provider pref JSON, and update the message providers
  _updateMessageProviders() {
    const previousProviders = this.state.providers;
    const providers = [
      // If we have added a `preview` provider, hold onto it
      ...previousProviders.filter(p => p.id === "preview"),
      // The provider should be enabled and not have a user preference set to false
      ...ASRouterPreferences.providers.filter(
        p =>
          p.enabled &&
          (ASRouterPreferences.getUserPreference(p.id) !== false &&
            // Provider is enabled or if provider has multiple categories
            // check that at least one category is enabled
            (!p.categories ||
              p.categories.some(
                c => ASRouterPreferences.getUserPreference(c) !== false
              )))
      ),
    ].map(_provider => {
      // make a copy so we don't modify the source of the pref
      const provider = { ..._provider };

      if (provider.type === "local" && !provider.messages) {
        // Get the messages from the local message provider
        const localProvider = this._localProviders[provider.localProvider];
        provider.messages = localProvider ? localProvider.getMessages() : [];
      }
      if (provider.type === "remote" && provider.url) {
        provider.url = provider.url.replace(
          /%STARTPAGE_VERSION%/g,
          STARTPAGE_VERSION
        );
        provider.url = Services.urlFormatter.formatURL(provider.url);
      }
      this.normalizeItemFrequency(provider);
      // Reset provider update timestamp to force message refresh
      provider.lastUpdated = undefined;
      return provider;
    });

    const providerIDs = providers.map(p => p.id);

    // Clear old messages for providers that are no longer enabled
    for (const prevProvider of previousProviders) {
      if (!providerIDs.includes(prevProvider.id)) {
        this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {
          type: "CLEAR_PROVIDER",
          data: { id: prevProvider.id },
        });
      }
    }

    this.setState(prevState => ({
      providers,
      // Clear any messages from removed providers
      messages: [
        ...prevState.messages.filter(message =>
          providerIDs.includes(message.provider)
        ),
      ],
    }));
  }

  get state() {
    return this._state;
  }

  set state(value) {
    throw new Error(
      "Do not modify this.state directy. Instead, call this.setState(newState)"
    );
  }

  /**
   * _resetInitialization - adds the following to the instance:
   *  .initialized {bool}            Has AS Router been initialized?
   *  .waitForInitialized {Promise}  A promise that resolves when initializion is complete
   *  ._finishInitializing {func}    A function that, when called, resolves the .waitForInitialized
   *                                 promise and sets .initialized to true.
   * @memberof _ASRouter
   */
  _resetInitialization() {
    this.initialized = false;
    this.waitForInitialized = new Promise(resolve => {
      this._finishInitializing = () => {
        this.initialized = true;
        resolve();
      };
    });
  }

  /**
   * loadMessagesFromAllProviders - Loads messages from all providers if they require updates.
   *                                Checks the .lastUpdated field on each provider to see if updates are needed
   * @memberof _ASRouter
   */
  async loadMessagesFromAllProviders() {
    const needsUpdate = this.state.providers.filter(provider =>
      MessageLoaderUtils.shouldProviderUpdate(provider)
    );
    // Don't do extra work if we don't need any updates
    if (needsUpdate.length) {
      let newState = { messages: [], providers: [] };
      for (const provider of this.state.providers) {
        if (needsUpdate.includes(provider)) {
          let {
            messages,
            lastUpdated,
            errors,
          } = await MessageLoaderUtils.loadMessagesForProvider(provider, {
            storage: this._storage,
            dispatchToAS: this.dispatchToAS,
          });
          messages = messages.filter(
            ({ content }) =>
              !content ||
              !content.category ||
              ASRouterPreferences.getUserPreference(content.category)
          );
          newState.providers.push({ ...provider, lastUpdated, errors });
          newState.messages = [...newState.messages, ...messages];
        } else {
          // Skip updating this provider's messages if no update is required
          let messages = this.state.messages.filter(
            msg => msg.provider === provider.id
          );
          newState.providers.push(provider);
          newState.messages = [...newState.messages, ...messages];
        }
      }

      for (const message of newState.messages) {
        this.normalizeItemFrequency(message);
      }

      // Some messages have triggers that require us to initalise trigger listeners
      const unseenListeners = new Set(ASRouterTriggerListeners.keys());
      for (const { trigger } of newState.messages) {
        if (trigger && ASRouterTriggerListeners.has(trigger.id)) {
          await ASRouterTriggerListeners.get(trigger.id).init(
            this._triggerHandler,
            trigger.params,
            trigger.patterns
          );
          unseenListeners.delete(trigger.id);
        }
      }
      // We don't need these listeners, but they may have previously been
      // initialised, so uninitialise them
      for (const triggerID of unseenListeners) {
        ASRouterTriggerListeners.get(triggerID).uninit();
      }

      // We don't want to cache preview endpoints, remove them after messages are fetched
      await this.setState(this._removePreviewEndpoint(newState));
      await this.cleanupImpressions();
    }
  }

  /**
   * init - Initializes the MessageRouter.
   * It is ready when it has been connected to a RemotePageManager instance.
   *
   * @param {RemotePageManager} channel a RemotePageManager instance
   * @param {obj} storage an AS storage instance
   * @param {func} dispatchToAS dispatch an action the main AS Store
   * @memberof _ASRouter
   */
  async init(channel, storage, dispatchToAS) {
    this.messageChannel = channel;
    this.messageChannel.addMessageListener(
      INCOMING_MESSAGE_NAME,
      this.onMessage
    );
    this._storage = storage;
    this.WHITELIST_HOSTS = this._loadSnippetsWhitelistHosts();
    this.dispatchToAS = dispatchToAS;

    ASRouterPreferences.init();
    ASRouterPreferences.addListener(this.onPrefChange);
    BookmarkPanelHub.init(
      this.handleMessageRequest,
      this.addImpression,
      this.dispatch
    );
    ToolbarBadgeHub.init(this.waitForInitialized, {
      handleMessageRequest: this.handleMessageRequest,
      addImpression: this.addImpression,
      blockMessageById: this.blockMessageById,
      unblockMessageById: this.unblockMessageById,
      dispatch: this.dispatch,
    });

    this._loadLocalProviders();

    // Instead of setupTrailhead, which adds experiments, just load override pref values
    await this.setFirstRunStateFromPref();

    const messageBlockList =
      (await this._storage.get("messageBlockList")) || [];
    const providerBlockList =
      (await this._storage.get("providerBlockList")) || [];
    const messageImpressions =
      (await this._storage.get("messageImpressions")) || {};
    const providerImpressions =
      (await this._storage.get("providerImpressions")) || {};
    const previousSessionEnd =
      (await this._storage.get("previousSessionEnd")) || 0;
    await this.setState({
      messageBlockList,
      providerBlockList,
      messageImpressions,
      providerImpressions,
      previousSessionEnd,
    });
    this._updateMessageProviders();
    await this.loadMessagesFromAllProviders();
    await MessageLoaderUtils.cleanupCache(this.state.providers, storage);

    // set necessary state in the rest of AS
    this.dispatchToAS(
      ac.BroadcastToContent({
        type: at.AS_ROUTER_INITIALIZED,
        data: ASRouterPreferences.specialConditions,
      })
    );

    // sets .initialized to true and resolves .waitForInitialized promise
    this._finishInitializing();
  }

  uninit() {
    this._storage.set("previousSessionEnd", Date.now());

    this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {
      type: "CLEAR_ALL",
    });
    this.messageChannel.removeMessageListener(
      INCOMING_MESSAGE_NAME,
      this.onMessage
    );
    this.messageChannel = null;
    this.dispatchToAS = null;

    ASRouterPreferences.removeListener(this.onPrefChange);
    ASRouterPreferences.uninit();
    BookmarkPanelHub.uninit();
    ToolbarBadgeHub.uninit();

    // Uninitialise all trigger listeners
    for (const listener of ASRouterTriggerListeners.values()) {
      listener.uninit();
    }
    // If we added any CFR recommendations, they need to be removed
    CFRPageActions.clearRecommendations();
    this._resetInitialization();
  }

  setState(callbackOrObj) {
    const newState =
      typeof callbackOrObj === "function"
        ? callbackOrObj(this.state)
        : callbackOrObj;
    this._state = { ...this.state, ...newState };
    return new Promise(resolve => {
      this._onStateChanged(this.state);
      resolve();
    });
  }

  getMessageById(id) {
    return this.state.messages.find(message => message.id === id);
  }

  _onStateChanged(state) {
    if (ASRouterPreferences.devtoolsEnabled) {
      this._updateAdminState();
    }
  }

  _loadLocalProviders() {
    // If we're in ASR debug mode add the local test providers
    if (ASRouterPreferences.devtoolsEnabled) {
      this._localProviders = {
        ...this._localProviders,
        SnippetsTestMessageProvider,
        PanelTestProvider,
      };
    }
  }

  /**
   * Used by ASRouter Admin returns all ASRouterTargeting.Environment
   * and ASRouter._getMessagesContext parameters and values
   */
  async getTargetingParameters(environment, localContext) {
    const targetingParameters = {};
    for (const param of Object.keys(environment)) {
      targetingParameters[param] = await environment[param];
    }
    for (const param of Object.keys(localContext)) {
      targetingParameters[param] = await localContext[param];
    }

    return targetingParameters;
  }

  async _updateAdminState(target) {
    const channel = target || this.messageChannel;
    channel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {
      type: "ADMIN_SET_STATE",
      data: {
        ...this.state,
        providerPrefs: ASRouterPreferences.providers,
        userPrefs: ASRouterPreferences.getAllUserPreferences(),
        targetingParameters: await this.getTargetingParameters(
          ASRouterTargeting.Environment,
          this._getMessagesContext()
        ),
        errors: this.errors,
      },
    });
  }

  _handleTargetingError(type, error, message) {
    Cu.reportError(error);
    if (this.dispatchToAS) {
      this.dispatchToAS(
        ac.ASRouterUserEvent({
          message_id: message.id,
          action: "asrouter_undesired_event",
          event: "TARGETING_EXPRESSION_ERROR",
          value: type,
        })
      );
    }
  }

  async _hasAddonAttributionData() {
    try {
      const data = (await AttributionCode.getAttrDataAsync()) || {};
      return data.source === "addons.mozilla.org";
    } catch (e) {
      return false;
    }
  }

  async setFirstRunStateFromPref() {
    let interrupt;
    let triplet;

    const overrideValue = Services.prefs.getStringPref(
      TRAILHEAD_CONFIG.OVERRIDE_PREF,
      ""
    );

    if (overrideValue) {
      [interrupt, triplet] = overrideValue.split("-");
    }

    await this.setState({
      trailheadInterrupt: interrupt,
      trailheadTriplet: triplet,
    });
  }

  /**
   * _generateTrailheadBranches - Generates and returns Trailhead configuration and chooses an experiment
   *                             based on clientID and locale.
   * @returns {{experiment: string, interrupt: string, triplet: string}}
   */
  async _generateTrailheadBranches() {
    let experiment = "";
    let interrupt;
    let triplet;

    const overrideValue = Services.prefs.getStringPref(
      TRAILHEAD_CONFIG.OVERRIDE_PREF,
      ""
    );
    if (overrideValue) {
      [interrupt, triplet] = overrideValue.split("-");
    }

    // Use control Trailhead Branch (for cards) if we are showing RTAMO.
    if (await this._hasAddonAttributionData()) {
      return {
        experiment,
        interrupt: "control",
        triplet: triplet || "privacy",
      };
    }

    // If a value is set in TRAILHEAD_OVERRIDE_PREF, it will be returned and no experiment will be set.
    if (overrideValue) {
      return { experiment, interrupt, triplet: triplet || "" };
    }

    const locale = Services.locale.appLocaleAsLangTag;

    if (TRAILHEAD_CONFIG.LOCALES.includes(locale)) {
      const { userId } = ClientEnvironment;
      experiment = await chooseBranch(
        `${userId}-trailhead-experiments`,
        TRAILHEAD_CONFIG.EXPERIMENT_RATIOS
      );

      // For the interrupts experiment,
      // we randomly assign an interrupt and always use the "supercharge" triplet.
      if (experiment === "interrupts") {
        interrupt = await chooseBranch(
          `${userId}-interrupts-branch`,
          TRAILHEAD_CONFIG.BRANCHES.interrupts
        );
        if (["join", "sync", "cards"].includes(interrupt)) {
          triplet = "supercharge";
        }

        // For the triplets experiment or non-experiment experience,
        // we randomly assign a triplet and always use the "join" interrupt.
      } else {
        interrupt = "join";
        triplet = await chooseBranch(
          `${userId}-triplets-branch`,
          TRAILHEAD_CONFIG.BRANCHES.triplets
        );
      }
    } else {
      // If the user is not in a trailhead-compabtible locale, return the control experience and no experiment.
      interrupt = "control";
    }

    return { experiment, interrupt, triplet };
  }

  // Dispatch a TRAILHEAD_ENROLL_EVENT action
  _sendTrailheadEnrollEvent(data) {
    this.dispatchToAS({
      type: at.TRAILHEAD_ENROLL_EVENT,
      data,
    });
  }

  async setupExtendedTriplets() {
    // Don't re-initialize
    if (this.state.extendedTripletsInitialized) {
      return;
    }

    let branch = Services.prefs.getStringPref(
      TRAILHEAD_CONFIG.EXTENDED_TRIPLETS_EXPERIMENT_PREF,
      ""
    );
    if (!branch) {
      const { userId } = ClientEnvironment;
      branch = await chooseBranch(
        `${userId}-extended-triplets-experiment`,
        TRAILHEAD_CONFIG.EXPERIMENT_RATIOS_FOR_EXTENDED_TRIPLETS
      );
      Services.prefs.setStringPref(
        TRAILHEAD_CONFIG.EXTENDED_TRIPLETS_EXPERIMENT_PREF,
        branch
      );
    }

    // In order for ping centre to pick this up, it MUST contain a substring activity-stream
    const experimentName = `activity-stream-extended-triplets`;
    TelemetryEnvironment.setExperimentActive(experimentName, branch);

    const state = { extendedTripletsInitialized: true };
    // Disable the extended triplets for the "holdback" group.
    if (branch === "holdback") {
      state.showExtendedTriplets = false;
    }
    await this.setState(state);
  }

  async setupTrailhead() {
    // Don't initialize
    if (
      this.state.trailheadInitialized ||
      !Services.prefs.getBoolPref(
        TRAILHEAD_CONFIG.DID_SEE_ABOUT_WELCOME_PREF,
        false
      )
    ) {
      return;
    }

    const {
      experiment,
      interrupt,
      triplet,
    } = await this._generateTrailheadBranches();

    await this.setState({
      trailheadInitialized: true,
      trailheadInterrupt: interrupt,
      trailheadTriplet: triplet,
    });

    if (experiment) {
      // In order for ping centre to pick this up, it MUST contain a substring activity-stream
      const experimentName = `activity-stream-firstrun-trailhead-${experiment}`;

      TelemetryEnvironment.setExperimentActive(
        experimentName,
        experiment === "interrupts" ? interrupt : triplet,
        { type: "as-firstrun" }
      );

      // On the first time setting the interrupts experiment, expose the branch
      // for normandy to target for survey study, and send out the enrollment ping.
      if (
        experiment === "interrupts" &&
        !Services.prefs.prefHasUserValue(
          TRAILHEAD_CONFIG.INTERRUPTS_EXPERIMENT_PREF
        )
      ) {
        Services.prefs.setStringPref(
          TRAILHEAD_CONFIG.INTERRUPTS_EXPERIMENT_PREF,
          interrupt
        );
        this._sendTrailheadEnrollEvent({
          experiment: experimentName,
          type: "as-firstrun",
          branch: interrupt,
        });
      }

      // On the first time setting the triplets experiment, send out the enrollment ping.
      if (
        experiment === "triplets" &&
        !Services.prefs.getBoolPref(
          TRAILHEAD_CONFIG.TRIPLETS_ENROLLED_PREF,
          false
        )
      ) {
        Services.prefs.setBoolPref(
          TRAILHEAD_CONFIG.TRIPLETS_ENROLLED_PREF,
          true
        );
        this._sendTrailheadEnrollEvent({
          experiment: experimentName,
          type: "as-firstrun",
          branch: triplet,
        });
      }
    }
  }

  // Return an object containing targeting parameters used to select messages
  _getMessagesContext() {
    const {
      messageImpressions,
      previousSessionEnd,
      trailheadInterrupt,
      trailheadTriplet,
    } = this.state;

    return {
      get messageImpressions() {
        return messageImpressions;
      },
      get previousSessionEnd() {
        return previousSessionEnd;
      },
      get trailheadInterrupt() {
        return trailheadInterrupt;
      },
      get trailheadTriplet() {
        return trailheadTriplet;
      },
    };
  }

  _findMessage(candidateMessages, trigger) {
    const messages = candidateMessages.filter(m =>
      this.isBelowFrequencyCaps(m)
    );
    const context = this._getMessagesContext();

    // Find a message that matches the targeting context as well as the trigger context (if one is provided)
    // If no trigger is provided, we should find a message WITHOUT a trigger property defined.
    return ASRouterTargeting.findMatchingMessage({
      messages,
      trigger,
      context,
      onError: this._handleTargetingError,
    });
  }

  async evaluateExpression(target, { expression, context }) {
    const channel = target || this.messageChannel;
    let evaluationStatus;
    try {
      evaluationStatus = {
        result: await ASRouterTargeting.isMatch(expression, context),
        success: true,
      };
    } catch (e) {
      evaluationStatus = { result: e.message, success: false };
    }

    channel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {
      type: "ADMIN_SET_STATE",
      data: { ...this.state, evaluationStatus },
    });
  }

  _orderBundle(bundle) {
    return bundle.sort((a, b) => a.order - b.order);
  }

  // Work out if a message can be shown based on its and its provider's frequency caps.
  isBelowFrequencyCaps(message) {
    const { providers, messageImpressions, providerImpressions } = this.state;

    const provider = providers.find(p => p.id === message.provider);
    const impressionsForMessage = messageImpressions[message.id];
    const impressionsForProvider = providerImpressions[message.provider];

    return (
      this._isBelowItemFrequencyCap(
        message,
        impressionsForMessage,
        MAX_MESSAGE_LIFETIME_CAP
      ) && this._isBelowItemFrequencyCap(provider, impressionsForProvider)
    );
  }

  // Helper for isBelowFrecencyCaps - work out if the frequency cap for the given
  //                                  item has been exceeded or not
  _isBelowItemFrequencyCap(item, impressions, maxLifetimeCap = Infinity) {
    if (item && item.frequency && impressions && impressions.length) {
      if (
        item.frequency.lifetime &&
        impressions.length >= Math.min(item.frequency.lifetime, maxLifetimeCap)
      ) {
        return false;
      }
      if (item.frequency.custom) {
        const now = Date.now();
        for (const setting of item.frequency.custom) {
          let { period } = setting;
          const impressionsInPeriod = impressions.filter(t => now - t < period);
          if (impressionsInPeriod.length >= setting.cap) {
            return false;
          }
        }
      }
    }
    return true;
  }

  async _getBundledMessages(originalMessage, target, trigger, force = false) {
    let result = [];
    let bundleLength;
    let bundleTemplate;
    let originalId;

    if (originalMessage.includeBundle) {
      // The original message is not part of the bundle, so don't include it
      bundleLength = originalMessage.includeBundle.length;
      bundleTemplate = originalMessage.includeBundle.template;
    } else {
      // The original message is part of the bundle
      bundleLength = originalMessage.bundled;
      bundleTemplate = originalMessage.template;
      originalId = originalMessage.id;
      // Add in a copy of the first message
      result.push({
        content: originalMessage.content,
        id: originalMessage.id,
        order: originalMessage.order || 0,
      });
    }

    // First, find all messages of same template. These are potential matching targeting candidates
    let bundledMessagesOfSameTemplate = this._getUnblockedMessages().filter(
      msg =>
        msg.bundled && msg.template === bundleTemplate && msg.id !== originalId
    );

    if (force) {
      // Forcefully show the messages without targeting matching - this is for about:newtab#asrouter to show the messages
      for (const message of bundledMessagesOfSameTemplate) {
        result.push({ content: message.content, id: message.id });
        // Stop once we have enough messages to fill a bundle
        if (result.length === bundleLength) {
          break;
        }
      }
    } else {
      while (bundledMessagesOfSameTemplate.length) {
        // Find a message that matches the targeting context - or break if there are no matching messages
        const message = await this._findMessage(
          bundledMessagesOfSameTemplate,
          trigger
        );
        if (!message) {
          /* istanbul ignore next */ // Code coverage in mochitests
          break;
        }
        // Only copy the content of the message (that's what the UI cares about)
        // Also delete the message we picked so we don't pick it again
        result.push({
          content: message.content,
          id: message.id,
          order: message.order || 0,
        });
        bundledMessagesOfSameTemplate.splice(
          bundledMessagesOfSameTemplate.findIndex(msg => msg.id === message.id),
          1
        );
        // Stop once we have enough messages to fill a bundle
        if (result.length === bundleLength) {
          break;
        }
      }
    }

    // If we did not find enough messages to fill the bundle, do not send the bundle down
    if (result.length < bundleLength) {
      return null;
    }

    // The bundle may have some extra attributes, like a header, or a dismiss button, so attempt to get those strings now
    // This is a temporary solution until we can use Fluent strings in the content process, in which case the content can
    // handle finding these strings on its own. See bug 1488973
    const extraTemplateStrings = await this._extraTemplateStrings(
      originalMessage
    );

    return {
      bundle: this._orderBundle(result),
      ...(extraTemplateStrings && { extraTemplateStrings }),
      provider: originalMessage.provider,
      template: originalMessage.template,
    };
  }

  async _extraTemplateStrings(originalMessage) {
    let extraTemplateStrings;
    let localProvider = this._findProvider(originalMessage.provider);
    if (localProvider && localProvider.getExtraAttributes) {
      extraTemplateStrings = await localProvider.getExtraAttributes();
    }

    return extraTemplateStrings;
  }

  _findProvider(providerID) {
    return this._localProviders[
      this.state.providers.find(i => i.id === providerID).localProvider
    ];
  }

  _getUnblockedMessages() {
    let { state } = this;
    return state.messages.filter(
      item =>
        !state.messageBlockList.includes(item.id) &&
        (!item.campaign || !state.messageBlockList.includes(item.campaign)) &&
        !state.providerBlockList.includes(item.provider)
    );
  }

  /**
   * Route messages based on template to the correct module that can display them
   */
  routeMessageToTarget(message, target, trigger, force = false) {
    switch (message.template) {
      case "cfr_doorhanger":
        if (force) {
          CFRPageActions.forceRecommendation(target, message, this.dispatch);
        } else {
          CFRPageActions.addRecommendation(
            target,
            trigger.param.host,
            message,
            this.dispatch
          );
        }
        break;
      case "fxa_bookmark_panel":
        if (force) {
          BookmarkPanelHub._forceShowMessage(target, message);
        }
        break;
      case "toolbar_badge":
      case "update_action":
        ToolbarBadgeHub.registerBadgeNotificationListener(message, { force });
        break;
      default:
        target.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {
          type: "SET_MESSAGE",
          data: message,
        });
        break;
    }
  }

  async _sendMessageToTarget(message, target, trigger, force = false) {
    // No message is available, so send CLEAR_ALL.
    if (!message) {
      try {
        target.sendAsyncMessage(OUTGOING_MESSAGE_NAME, { type: "CLEAR_ALL" });
      } catch (e) {}

      // For bundled messages, look for the rest of the bundle or else send CLEAR_ALL
    } else if (message.bundled) {
      const bundledMessages = await this._getBundledMessages(
        message,
        target,
        trigger,
        force
      );
      const action = bundledMessages
        ? { type: "SET_BUNDLED_MESSAGES", data: bundledMessages }
        : { type: "CLEAR_ALL" };
      try {
        target.sendAsyncMessage(OUTGOING_MESSAGE_NAME, action);
      } catch (e) {}

      // For nested bundled messages, look for the desired bundle
    } else if (message.includeBundle) {
      const bundledMessages = await this._getBundledMessages(
        message,
        target,
        message.includeBundle.trigger,
        force
      );
      try {
        target.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {
          type: "SET_MESSAGE",
          data: {
            ...message,
            bundle: bundledMessages && bundledMessages.bundle,
          },
        });
      } catch (e) {}
    } else {
      try {
        this.routeMessageToTarget(message, target, trigger, force);
      } catch (e) {}
    }
  }

  async addImpression(message) {
    const provider = this.state.providers.find(p => p.id === message.provider);
    // We only need to store impressions for messages that have frequency, or
    // that have providers that have frequency
    if (message.frequency || (provider && provider.frequency)) {
      const time = Date.now();
      await this.setState(state => {
        const messageImpressions = this._addImpressionForItem(
          state,
          message,
          "messageImpressions",
          time
        );
        const providerImpressions = this._addImpressionForItem(
          state,
          provider,
          "providerImpressions",
          time
        );
        return { messageImpressions, providerImpressions };
      });
    }
  }

  // Helper for addImpression - calculate the updated impressions object for the given
  //                            item, then store it and return it
  _addImpressionForItem(state, item, impressionsString, time) {
    // The destructuring here is to avoid mutating existing objects in state as in redux
    // (see https://redux.js.org/recipes/structuring-reducers/prerequisite-concepts#immutable-data-management)
    const impressions = { ...state[impressionsString] };
    if (item.frequency) {
      impressions[item.id] = impressions[item.id]
        ? [...impressions[item.id]]
        : [];
      impressions[item.id].push(time);
      this._storage.set(impressionsString, impressions);
    }
    return impressions;
  }

  /**
   * getLongestPeriod
   *
   * @param {obj} item Either an ASRouter message or an ASRouter provider
   * @returns {int|null} if the item has custom frequency caps, the longest period found in the list of caps.
                         if the item has no custom frequency caps, null
   * @memberof _ASRouter
   */
  getLongestPeriod(item) {
    if (!item.frequency || !item.frequency.custom) {
      return null;
    }
    return item.frequency.custom.sort((a, b) => b.period - a.period)[0].period;
  }

  /**
   * cleanupImpressions - this function cleans up obsolete impressions whenever
   * messages are refreshed or fetched. It will likely need to be more sophisticated in the future,
   * but the current behaviour for when both message impressions and provider impressions are
   * cleared is as follows (where `item` is either `message` or `provider`):
   *
   * 1. If the item id for a list of item impressions no longer exists in the ASRouter state, it
   *    will be cleared.
   * 2. If the item has time-bound frequency caps but no lifetime cap, any item impressions older
   *    than the longest time period will be cleared.
   */
  async cleanupImpressions() {
    await this.setState(state => {
      const messageImpressions = this._cleanupImpressionsForItems(
        state,
        state.messages,
        "messageImpressions"
      );
      const providerImpressions = this._cleanupImpressionsForItems(
        state,
        state.providers,
        "providerImpressions"
      );
      return { messageImpressions, providerImpressions };
    });
  }

  // Helper for cleanupImpressions - calculate the updated impressions object for
  //                                 the given items, then store it and return it
  _cleanupImpressionsForItems(state, items, impressionsString) {
    const impressions = { ...state[impressionsString] };
    let needsUpdate = false;
    Object.keys(impressions).forEach(id => {
      const [item] = items.filter(x => x.id === id);
      // Don't keep impressions for items that no longer exist
      if (!item || !item.frequency || !Array.isArray(impressions[id])) {
        delete impressions[id];
        needsUpdate = true;
        return;
      }
      if (!impressions[id].length) {
        return;
      }
      // If we don't want to store impressions older than the longest period
      if (item.frequency.custom && !item.frequency.lifetime) {
        const now = Date.now();
        impressions[id] = impressions[id].filter(
          t => now - t < this.getLongestPeriod(item)
        );
        needsUpdate = true;
      }
    });
    if (needsUpdate) {
      this._storage.set(impressionsString, impressions);
    }
    return impressions;
  }

  handleMessageRequest({
    triggerId,
    triggerParam,
    triggerContext,
    template,
    provider,
  }) {
    const msgs = this._getUnblockedMessages().filter(m => {
      if (provider && m.provider !== provider) {
        return false;
      }
      if (template && m.template !== template) {
        return false;
      }
      if (m.trigger && m.trigger.id !== triggerId) {
        return false;
      }

      return true;
    });

    return this._findMessage(
      msgs,
      triggerId && {
        id: triggerId,
        param: triggerParam,
        context: triggerContext,
      }
    );
  }

  async setMessageById(id, target, force = true, action = {}) {
    await this.setState({ lastMessageId: id });
    const newMessage = this.getMessageById(id);

    await this._sendMessageToTarget(newMessage, target, action.data, force);
  }

  async blockMessageById(idOrIds) {
    const idsToBlock = Array.isArray(idOrIds) ? idOrIds : [idOrIds];

    await this.setState(state => {
      const messageBlockList = [...state.messageBlockList];
      const messageImpressions = { ...state.messageImpressions };

      idsToBlock.forEach(id => {
        const message = state.messages.find(m => m.id === id);
        const idToBlock = message && message.campaign ? message.campaign : id;
        if (!messageBlockList.includes(idToBlock)) {
          messageBlockList.push(idToBlock);
        }

        // When a message is blocked, its impressions should be cleared as well
        delete messageImpressions[id];
      });

      this._storage.set("messageBlockList", messageBlockList);
      this._storage.set("messageImpressions", messageImpressions);
      return { messageBlockList, messageImpressions };
    });
  }

  unblockMessageById(id) {
    return this.setState(state => {
      const messageBlockList = [...state.messageBlockList];
      const message = state.messages.find(m => m.id === id);
      const idToUnblock = message && message.campaign ? message.campaign : id;
      messageBlockList.splice(messageBlockList.indexOf(idToUnblock), 1);
      this._storage.set("messageBlockList", messageBlockList);
      return { messageBlockList };
    });
  }

  async blockProviderById(idOrIds) {
    const idsToBlock = Array.isArray(idOrIds) ? idOrIds : [idOrIds];

    await this.setState(state => {
      const providerBlockList = [...state.providerBlockList, ...idsToBlock];
      // When a provider is blocked, its impressions should be cleared as well
      const providerImpressions = { ...state.providerImpressions };
      idsToBlock.forEach(id => delete providerImpressions[id]);
      this._storage.set("providerBlockList", providerBlockList);
      return { providerBlockList, providerImpressions };
    });
  }

  _validPreviewEndpoint(url) {
    try {
      const endpoint = new URL(url);
      if (!this.WHITELIST_HOSTS[endpoint.host]) {
        Cu.reportError(
          `The preview URL host ${endpoint.host} is not in the whitelist.`
        );
      }
      if (endpoint.protocol !== "https:") {
        Cu.reportError("The URL protocol is not https.");
      }
      return (
        endpoint.protocol === "https:" && this.WHITELIST_HOSTS[endpoint.host]
      );
    } catch (e) {
      return false;
    }
  }

  // Ensure we switch to the Onboarding message after RTAMO addon was installed
  _updateOnboardingState() {
    let addonInstallObs = (subject, topic) => {
      Services.obs.removeObserver(
        addonInstallObs,
        "webextension-install-notify"
      );
      this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {
        type: "CLEAR_INTERRUPT",
      });
    };
    Services.obs.addObserver(addonInstallObs, "webextension-install-notify");
  }

  _loadSnippetsWhitelistHosts() {
    let additionalHosts = [];
    const whitelistPrefValue = Services.prefs.getStringPref(
      SNIPPETS_ENDPOINT_WHITELIST,
      ""
    );
    try {
      additionalHosts = JSON.parse(whitelistPrefValue);
    } catch (e) {
      if (whitelistPrefValue) {
        Cu.reportError(
          `Pref ${SNIPPETS_ENDPOINT_WHITELIST} value is not valid JSON`
        );
      }
    }

    if (!additionalHosts.length) {
      return DEFAULT_WHITELIST_HOSTS;
    }

    // If there are additional hosts we want to whitelist, add them as
    // `preview` so that the updateCycle is 0
    return additionalHosts.reduce(
      (whitelist_hosts, host) => {
        whitelist_hosts[host] = "preview";
        Services.console.logStringMessage(`Adding ${host} to whitelist hosts.`);
        return whitelist_hosts;
      },
      { ...DEFAULT_WHITELIST_HOSTS }
    );
  }

  // To be passed to ASRouterTriggerListeners
  async _triggerHandler(target, trigger) {
    await this.onMessage({
      target,
      data: { type: "TRIGGER", data: { trigger } },
    });
  }

  _removePreviewEndpoint(state) {
    state.providers = state.providers.filter(p => p.id !== "preview");
    return state;
  }

  async _addPreviewEndpoint(url, portID) {
    // When you view a preview snippet we want to hide all real content
    const providers = [...this.state.providers];
    if (
      this._validPreviewEndpoint(url) &&
      !providers.find(p => p.url === url)
    ) {
      this.dispatchToAS(
        ac.OnlyToOneContent({ type: at.SNIPPETS_PREVIEW_MODE }, portID)
      );
      providers.push({
        id: "preview",
        type: "remote",
        url,
        updateCycleInMs: 0,
      });
      await this.setState({ providers });
    }
  }

  // Windows specific calls to write attribution data
  // Used by `forceAttribution` to set required targeting attributes for
  // RTAMO messages. This should only be called from within about:newtab#asrouter
  /* istanbul ignore next */
  async _writeAttributionFile(data) {
    let appDir = Services.dirsvc.get("LocalAppData", Ci.nsIFile);
    let file = appDir.clone();
    file.append(Services.appinfo.vendor || "mozilla");
    file.append(AppConstants.MOZ_APP_NAME);

    await OS.File.makeDir(file.path, {
      from: appDir.path,
      ignoreExisting: true,
    });

    file.append("postSigningData");
    await OS.File.writeAtomic(file.path, data);
  }

  /**
   * forceAttribution - this function should only be called from within about:newtab#asrouter.
   * It forces the browser attribution to be set to something specified in asrouter admin
   * tools, and reloads the providers in order to get messages that are dependant on this
   * attribution data (see Return to AMO flow in bug 1475354 for example). Note - OSX and Windows only
   * @param {data} Object an object containing the attribtion data that came from asrouter admin page
   */
  /* istanbul ignore next */
  async forceAttribution(data) {
    // Extract the parameters from data that will make up the referrer url
    const { source, campaign, content } = data;
    if (AppConstants.platform === "win") {
      const attributionData = `source=${source}&campaign=${campaign}&content=${content}`;
      this._writeAttributionFile(encodeURIComponent(attributionData));
    } else if (AppConstants.platform === "macosx") {
      let appPath = Services.dirsvc.get("GreD", Ci.nsIFile).parent.parent.path;
      let attributionSvc = Cc["@mozilla.org/mac-attribution;1"].getService(
        Ci.nsIMacAttributionService
      );

      let referrer = `https://www.mozilla.org/anything/?utm_campaign=${campaign}&utm_source=${source}&utm_content=${encodeURIComponent(
        content
      )}`;

      // This sets the Attribution to be the referrer
      attributionSvc.setReferrerUrl(appPath, referrer, true);
    }

    // Clear cache call is only possible in a testing environment
    let env = Cc["@mozilla.org/process/environment;1"].getService(
      Ci.nsIEnvironment
    );
    env.set("XPCSHELL_TEST_PROFILE_DIR", "testing");

    // Clear and refresh Attribution, and then fetch the messages again to update
    AttributionCode._clearCache();
    await AttributionCode.getAttrDataAsync();
    this._updateMessageProviders();
    await this.loadMessagesFromAllProviders();
  }

  async handleUserAction({ data: action, target }) {
    switch (action.type) {
      case ra.OPEN_PRIVATE_BROWSER_WINDOW:
        // Forcefully open about:privatebrowsing
        target.browser.ownerGlobal.OpenBrowserWindow({ private: true });
        break;
      case ra.OPEN_URL:
        target.browser.ownerGlobal.openLinkIn(
          action.data.args,
          action.data.where || "current",
          {
            private: false,
            triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
              {}
            ),
            csp: null,
          }
        );
        break;
      case ra.OPEN_ABOUT_PAGE:
        target.browser.ownerGlobal.openTrustedLinkIn(
          `about:${action.data.args}`,
          "tab"
        );
        break;
      case ra.OPEN_PREFERENCES_PAGE:
        target.browser.ownerGlobal.openPreferences(action.data.category);
        break;
      case ra.OPEN_APPLICATIONS_MENU:
        UITour.showMenu(target.browser.ownerGlobal, action.data.args);
        break;
      case ra.INSTALL_ADDON_FROM_URL:
        this._updateOnboardingState();
        await MessageLoaderUtils.installAddonFromURL(
          target.browser,
          action.data.url,
          action.data.telemetrySource
        );
        break;
      case ra.PIN_CURRENT_TAB:
        let tab = target.browser.ownerGlobal.gBrowser.selectedTab;
        target.browser.ownerGlobal.gBrowser.pinTab(tab);
        target.browser.ownerGlobal.ConfirmationHint.show(tab, "pinTab", {
          showDescription: true,
        });
        break;
      case ra.SHOW_FIREFOX_ACCOUNTS:
        const url = await FxAccounts.config.promiseSignUpURI("snippets");
        // We want to replace the current tab.
        target.browser.ownerGlobal.openLinkIn(url, "current", {
          private: false,
          triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
            {}
          ),
          csp: null,
        });
        break;
    }
  }

  dispatch(action, target) {
    this.onMessage({ data: action, target });
  }

  async sendNewTabMessage(target, options = {}) {
    const { endpoint } = options;
    let message;

    // Load preview endpoint for snippets if one is sent
    if (endpoint) {
      await this._addPreviewEndpoint(endpoint.url, target.portID);
    }

    // Load all messages
    await this.loadMessagesFromAllProviders();

    if (endpoint) {
      message = await this.handleMessageRequest({ provider: "preview" });
      // We don't want to cache preview messages, remove them after we selected the message to show
      await this.setState(state => ({
        lastMessageId: message ? message.id : null,
        messages: message
          ? state.messages.filter(m => m.id !== message.id)
          : state.messages,
      }));
    } else {
      // On new tab, send cards if they match; othwerise send a snippet
      message = await this.handleMessageRequest({
        provider: "onboarding",
        template: "extended_triplets",
      });

      // Set up the experiment for extended triplets. It's done here because we
      // only want to enroll users (for both control and holdback) if they meet
      // the targeting criteria.
      if (message) {
        await this.setupExtendedTriplets();
      }

      // If no extended triplets message was returned, or the holdback experiment
      // is active, show snippets instead
      if (!message || !this.state.showExtendedTriplets) {
        message = await this.handleMessageRequest({ provider: "snippets" });
      }

      await this.setState({ lastMessageId: message ? message.id : null });
    }

    await this._sendMessageToTarget(message, target);
  }

  async sendTriggerMessage(target, trigger) {
    await this.loadMessagesFromAllProviders();

    if (trigger.id === "firstRun") {
      // On about welcome, set up trailhead experiments
      if (!this.state.trailheadInitialized) {
        Services.prefs.setBoolPref(
          TRAILHEAD_CONFIG.DID_SEE_ABOUT_WELCOME_PREF,
          true
        );
        await this.setupTrailhead();
      }
    }

    const message = await this.handleMessageRequest({
      triggerId: trigger.id,
      triggerParam: trigger.param,
      triggerContext: trigger.context,
    });

    await this.setState({ lastMessageId: message ? message.id : null });
    await this._sendMessageToTarget(message, target, trigger);
  }

  /* eslint-disable complexity */
  async onMessage({ data: action, target }) {
    switch (action.type) {
      case "USER_ACTION":
        if (action.data.type in ra) {
          await this.handleUserAction({ data: action.data, target });
        }
        break;
      case "NEWTAB_MESSAGE_REQUEST":
        await this.waitForInitialized;
        await this.sendNewTabMessage(target, action.data);
        break;
      case "TRIGGER":
        await this.waitForInitialized;
        await this.sendTriggerMessage(
          target,
          action.data && action.data.trigger
        );
      case "BLOCK_MESSAGE_BY_ID":
        await this.blockMessageById(action.data.id);
        // Block the message but don't dismiss it in case the action taken has
        // another state that needs to be visible
        if (action.data.preventDismiss) {
          break;
        }
        this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {
          type: "CLEAR_MESSAGE",
          data: { id: action.data.id },
        });
        break;
      case "DISMISS_MESSAGE_BY_ID":
        this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {
          type: "CLEAR_MESSAGE",
          data: { id: action.data.id },
        });
        break;
      case "BLOCK_PROVIDER_BY_ID":
        await this.blockProviderById(action.data.id);
        this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {
          type: "CLEAR_PROVIDER",
          data: { id: action.data.id },
        });
        break;
      case "BLOCK_BUNDLE":
        await this.blockMessageById(action.data.bundle.map(b => b.id));
        this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {
          type: "CLEAR_BUNDLE",
        });
        break;
      case "UNBLOCK_MESSAGE_BY_ID":
        this.unblockMessageById(action.data.id);
        break;
      case "UNBLOCK_PROVIDER_BY_ID":
        await this.setState(state => {
          const providerBlockList = [...state.providerBlockList];
          providerBlockList.splice(
            providerBlockList.indexOf(action.data.id),
            1
          );
          this._storage.set("providerBlockList", providerBlockList);
          return { providerBlockList };
        });
        break;
      case "UNBLOCK_BUNDLE":
        await this.setState(state => {
          const messageBlockList = [...state.messageBlockList];
          for (let message of action.data.bundle) {
            messageBlockList.splice(messageBlockList.indexOf(message.id), 1);
          }
          this._storage.set("messageBlockList", messageBlockList);
          return { messageBlockList };
        });
        break;
      case "OVERRIDE_MESSAGE":
        await this.setMessageById(action.data.id, target, true, action);
        break;
      case "ADMIN_CONNECT_STATE":
        if (action.data && action.data.endpoint) {
          this._addPreviewEndpoint(action.data.endpoint.url, target.portID);
          await this.loadMessagesFromAllProviders();
        } else {
          await this._updateAdminState(target);
        }
        break;
      case "IMPRESSION":
        await this.addImpression(action.data);
        break;
      case "DOORHANGER_TELEMETRY":
      case "TOOLBAR_BADGE_TELEMETRY":
        if (this.dispatchToAS) {
          this.dispatchToAS(ac.ASRouterUserEvent(action.data));
        }
        break;
      case "EXPIRE_QUERY_CACHE":
        QueryCache.expireAll();
        break;
      case "ENABLE_PROVIDER":
        ASRouterPreferences.enableOrDisableProvider(action.data, true);
        break;
      case "DISABLE_PROVIDER":
        ASRouterPreferences.enableOrDisableProvider(action.data, false);
        break;
      case "RESET_PROVIDER_PREF":
        ASRouterPreferences.resetProviderPref();
        break;
      case "SET_PROVIDER_USER_PREF":
        ASRouterPreferences.setUserPreference(
          action.data.id,
          action.data.value
        );
        break;
      case "EVALUATE_JEXL_EXPRESSION":
        this.evaluateExpression(target, action.data);
        break;
      case "FORCE_ATTRIBUTION":
        this.forceAttribution(action.data);
        break;
      default:
        Cu.reportError("Unknown message received");
        break;
    }
  }
}
this._ASRouter = _ASRouter;
this.chooseBranch = chooseBranch;
this.TRAILHEAD_CONFIG = TRAILHEAD_CONFIG;

/**
 * ASRouter - singleton instance of _ASRouter that controls all messages
 * in the new tab page.
 */
this.ASRouter = new _ASRouter();

const EXPORTED_SYMBOLS = [
  "_ASRouter",
  "ASRouter",
  "MessageLoaderUtils",
  "chooseBranch",
  "TRAILHEAD_CONFIG",
];
