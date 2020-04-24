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
  ToolbarPanelHub: "resource://activity-stream/lib/ToolbarPanelHub.jsm",
  MomentsPageHub: "resource://activity-stream/lib/MomentsPageHub.jsm",
  ASRouterTargeting: "resource://activity-stream/lib/ASRouterTargeting.jsm",
  QueryCache: "resource://activity-stream/lib/ASRouterTargeting.jsm",
  ASRouterPreferences: "resource://activity-stream/lib/ASRouterPreferences.jsm",
  TARGETING_PREFERENCES:
    "resource://activity-stream/lib/ASRouterPreferences.jsm",
  ASRouterTriggerListeners:
    "resource://activity-stream/lib/ASRouterTriggerListeners.jsm",
  CFRMessageProvider: "resource://activity-stream/lib/CFRMessageProvider.jsm",
  GroupsConfigurationProvider:
    "resource://activity-stream/lib/GroupsConfigurationProvider.jsm",
  KintoHttpClient: "resource://services-common/kinto-http-client.js",
  Downloader: "resource://services-settings/Attachments.jsm",
  RemoteL10n: "resource://activity-stream/lib/RemoteL10n.jsm",
  MigrationUtils: "resource:///modules/MigrationUtils.jsm",
  ExperimentAPI: "resource://messaging-system/experiments/ExperimentAPI.jsm",
});
XPCOMUtils.defineLazyServiceGetters(this, {
  BrowserHandler: ["@mozilla.org/browser/clh;1", "nsIBrowserHandler"],
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

const TRAILHEAD_CONFIG = {
  DID_SEE_ABOUT_WELCOME_PREF: "trailhead.firstrun.didSeeAboutWelcome",
  DYNAMIC_TRIPLET_BUNDLE_LENGTH: 3,
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

// Remote Settings
const RS_SERVER_PREF = "services.settings.server";
const RS_MAIN_BUCKET = "main";
const RS_COLLECTION_L10N = "ms-language-packs"; // "ms" stands for Messaging System
const RS_PROVIDERS_WITH_L10N = ["cfr", "cfr-fxa", "whats-new-panel"];
const RS_FLUENT_VERSION = "v1";
const RS_FLUENT_RECORD_PREFIX = `cfr-${RS_FLUENT_VERSION}`;
const RS_DOWNLOAD_MAX_RETRIES = 2;
// This is the list of providers for which we want to cache the targeting
// expression result and reuse between calls. Cache duration is defined in
// ASRouterTargeting where evaluation takes place.
const JEXL_PROVIDER_CACHE = new Set(["snippets"]);

// To observe the app locale change notification.
const TOPIC_INTL_LOCALE_CHANGED = "intl:app-locales-changed";
// To observe the pref that controls if ASRouter should use the remote Fluent files for l10n.
const USE_REMOTE_L10N_PREF =
  "browser.newtabpage.activity-stream.asrouter.useRemoteL10n";

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

  async _localJsonLoader(provider) {
    let payload;
    try {
      payload = await (
        await fetch(provider.location, {
          credentials: "omit",
        })
      ).json();
    } catch (e) {
      return [];
    }

    return payload.messages;
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
        response.status >= 200 &&
        response.status < 400
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
   * Note:
   * 1). Both "cfr" and "cfr-fxa" require the Fluent file for l10n, so there is
   * another file downloading phase for those two providers after their messages
   * are successfully fetched from Remote Settings. Currently, they share the same
   * attachment of the record "${RS_FLUENT_RECORD_PREFIX}-${locale}" in the
   * "ms-language-packs" collection. E.g. for "en-US" with version "v1",
   * the Fluent file is attched to the record with ID "cfr-v1-en-US".
   *
   * 2). The Remote Settings downloader is able to detect the duplicate download
   * requests for the same attachment and ignore the redundent requests automatically.
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
        } else if (RS_PROVIDERS_WITH_L10N.includes(provider.id)) {
          const locale = Services.locale.appLocaleAsBCP47;
          const recordId = `${RS_FLUENT_RECORD_PREFIX}-${locale}`;
          const kinto = new KintoHttpClient(
            Services.prefs.getStringPref(RS_SERVER_PREF)
          );
          const record = await kinto
            .bucket(RS_MAIN_BUCKET)
            .collection(RS_COLLECTION_L10N)
            .getRecord(recordId);
          if (record && record.data) {
            const downloader = new Downloader(
              RS_MAIN_BUCKET,
              RS_COLLECTION_L10N
            );
            // Await here in order to capture the exceptions for reporting.
            await downloader.download(record.data, {
              retries: RS_DOWNLOAD_MAX_RETRIES,
            });
            RemoteL10n.reloadL10n();
          } else {
            MessageLoaderUtils._handleRemoteSettingsUndesiredEvent(
              "ASR_RS_NO_MESSAGES",
              RS_COLLECTION_L10N,
              options.dispatchToAS
            );
          }
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

  async _experimentsAPILoader(provider, options) {
    try {
      await ExperimentAPI.ready();
    } catch (e) {
      MessageLoaderUtils.reportError(e);
      return [];
    }
    return provider.messageGroups
      .map(group => {
        let experimentData;
        try {
          experimentData = ExperimentAPI.getExperiment({ group });
        } catch (e) {
          MessageLoaderUtils.reportError(e);
          return [];
        }
        if (experimentData && experimentData.branch) {
          return experimentData.branch.value;
        }

        return [];
      })
      .flat();
  },

  _handleRemoteSettingsUndesiredEvent(event, providerId, dispatchToAS) {
    if (dispatchToAS) {
      dispatchToAS(
        ac.ASRouterUserEvent({
          action: "asrouter_undesired_event",
          event,
          message_id: "n/a",
          event_context: providerId,
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
      case "json":
        return this._localJsonLoader;
      case "remote-experiments":
        return this._experimentsAPILoader;
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

  async _loadDataForProvider(provider, options) {
    const loader = this._getMessageLoader(provider);
    let messages = await loader(provider, options);
    // istanbul ignore if
    if (!messages) {
      messages = [];
      MessageLoaderUtils.reportError(
        new Error(
          `Tried to load messages for ${provider.id} but the result was not an Array.`
        )
      );
    }

    return { messages };
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
    let { messages } = await this._loadDataForProvider(provider, options);
    // Filter out messages we temporarily want to exclude
    if (provider.exclude && provider.exclude.length) {
      messages = messages.filter(
        message => !provider.exclude.includes(message.id)
      );
    }
    const lastUpdated = Date.now();
    return {
      messages: messages
        .map(messageData => {
          const message = {
            weight: 100,
            ...messageData,
            groups: [...(messageData.groups || []), provider.id],
            provider: provider.id,
          };

          // This is to support a personalization experiment
          if (provider.personalized) {
            const score = ASRouterPreferences.personalizedCfrScores[message.id];
            if (score) {
              message.score = score;
            }
            message.personalizedModelVersion =
              provider.personalizedModelVersion;
          }

          return message;
        })
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
      providers: [],
      messageBlockList: [],
      groupBlockList: [],
      providerBlockList: [],
      messageImpressions: {},
      trailheadInitialized: false,
      messages: [],
      groups: [],
      errors: [],
      localeInUse: Services.locale.appLocaleAsBCP47,
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
    this._onLocaleChanged = this._onLocaleChanged.bind(this);
    this.isUnblockedMessage = this.isUnblockedMessage.bind(this);
    this.renderWNMessages = this.renderWNMessages.bind(this);
    this.forceWNPanel = this.forceWNPanel.bind(this);
  }

  async onPrefChange(prefName) {
    if (TARGETING_PREFERENCES.includes(prefName)) {
      // Notify all tabs of messages that have become invalid after pref change
      const invalidMessages = [];
      const context = this._getMessagesContext();

      for (const msg of this.state.messages.filter(this.isUnblockedMessage)) {
        if (!msg.targeting) {
          continue;
        }
        const isMatch = await ASRouterTargeting.isMatch(msg.targeting, context);
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
          ASRouterPreferences.getUserPreference(p.id) !== false &&
          // Provider is enabled or if provider has multiple categories
          // check that at least one category is enabled
          (!p.categories ||
            p.categories.some(
              c => ASRouterPreferences.getUserPreference(c) !== false
            ))
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

    return this.setState(prevState => ({
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
   * Check all provided groups are enabled
   * @param groups Set of groups to verify
   * @returns bool
   */
  hasGroupsEnabled(groups = []) {
    return this.state.groups
      .filter(({ id }) => groups.includes(id))
      .every(({ enabled }) => enabled);
  }

  /**
   * Verify that the provider block the message through the `exclude` field
   * @param message Message to verify
   * @returns bool
   */
  isExcludedByProvider(message) {
    // preview snippets are never excluded
    if (message.provider === "preview") {
      return false;
    }
    const provider = this.state.providers.find(p => p.id === message.provider);
    if (!provider) {
      return true;
    }
    if (provider.exclude) {
      return provider.exclude.includes(message.id);
    }
    return false;
  }

  /**
   * Fetch all message groups and update Router.state.groups
   * There are 3 types of groups:
   * - auto generated groups based on existing providers
   * - locally defined groups
   * - remotely defined groups
   * The override logic is as follows:
   * 1. Auto generated groups can be overriden by local or remote group configs.
   *    When generating a default group we check local and remote and merge all the options.
   * 2. Locally defined groups can be overriden by remotely defined group configs.
   *    When generating groups based on remote messages we merge with the local
   *    configuration.
   * @param provider RS messages provider for message groups
   */
  async loadAllMessageGroups() {
    const LOCAL_GROUP_CONFIGURATIONS = GroupsConfigurationProvider.getMessages();
    const [provider] = this.state.providers.filter(
      p =>
        p.id === "message-groups" && MessageLoaderUtils.shouldProviderUpdate(p)
    );
    if (!provider) {
      return;
    }
    let { messages } = await MessageLoaderUtils._loadDataForProvider(provider, {
      storage: this._storage,
      dispatchToAS: this.dispatchToAS,
    });
    const providerGroups = this.state.providers.map(
      ({ id, frequency = null, enabled }) => {
        const defaultGroup = { id, enabled, type: "default" };
        if (frequency) {
          defaultGroup.frequency = frequency;
        }
        const localGroup =
          LOCAL_GROUP_CONFIGURATIONS.find(g => g.id === id) || {};
        const remoteGroup = messages.find(g => g.id === id) || {};
        return { ...defaultGroup, ...localGroup, ...remoteGroup };
      }
    );
    const messageGroups = messages
      .filter(m => !providerGroups.find(g => g.id === m.id))
      .map(remoteGroup => {
        const localGroup =
          LOCAL_GROUP_CONFIGURATIONS.find(g => g.id === remoteGroup.id) || {};
        return { ...localGroup, ...remoteGroup };
      });
    const localGroups = LOCAL_GROUP_CONFIGURATIONS.filter(
      local =>
        !providerGroups.find(g => g.id === local.id) &&
        !messageGroups.find(g => g.id === local.id)
    );
    // Groups consist of automatically generated groups based on each message provider
    // merged with message defined groups fetched from Remote Settings.
    // A message defined group can override a provider group is it has the same name.
    await this.setState(state => ({
      groups: [...providerGroups, ...messageGroups, ...localGroups].map(
        group => ({
          ...group,
          enabled:
            group.enabled &&
            // Enabled if the group is not preset in the block list
            !state.groupBlockList.includes(group.id) &&
            (Array.isArray(group.userPreferences)
              ? group.userPreferences.every(
                  ASRouterPreferences.getUserPreference
                )
              : true),
        })
      ),
    }));
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
    await this.loadAllMessageGroups();
    // Don't do extra work if we don't need any updates
    if (needsUpdate.length) {
      let newState = { messages: [], providers: [] };
      for (const provider of this.state.providers) {
        if (needsUpdate.includes(provider)) {
          const {
            messages,
            lastUpdated,
            errors,
          } = await MessageLoaderUtils.loadMessagesForProvider(provider, {
            storage: this._storage,
            dispatchToAS: this.dispatchToAS,
          });
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
          ASRouterTriggerListeners.get(trigger.id).init(
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

  async _maybeUpdateL10nAttachment() {
    const { localeInUse } = this.state.localeInUse;
    const newLocale = Services.locale.appLocaleAsBCP47;
    if (newLocale !== localeInUse) {
      const providers = [...this.state.providers];
      let needsUpdate = false;
      providers.forEach(provider => {
        if (RS_PROVIDERS_WITH_L10N.includes(provider.id)) {
          // Force to refresh the messages as well as the attachment.
          provider.lastUpdated = undefined;
          needsUpdate = true;
        }
      });
      if (needsUpdate) {
        await this.setState({
          localeInUse: newLocale,
          providers,
        });
        await this.loadMessagesFromAllProviders();
      }
    }
  }

  async _onLocaleChanged(subject, topic, data) {
    await this._maybeUpdateL10nAttachment();
  }

  observe(aSubject, aTopic, aPrefName) {
    switch (aPrefName) {
      case USE_REMOTE_L10N_PREF:
        CFRPageActions.reloadL10n();
        break;
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
    ToolbarPanelHub.init(this.waitForInitialized, {
      getMessages: this.handleMessageRequest,
      dispatch: this.dispatch,
      handleUserAction: this.handleUserAction,
    });
    MomentsPageHub.init(this.waitForInitialized, {
      handleMessageRequest: this.handleMessageRequest,
      addImpression: this.addImpression,
      blockMessageById: this.blockMessageById,
      dispatch: this.dispatch,
    });

    this._loadLocalProviders();

    const messageBlockList =
      (await this._storage.get("messageBlockList")) || [];
    const providerBlockList =
      (await this._storage.get("providerBlockList")) || [];
    const messageImpressions =
      (await this._storage.get("messageImpressions")) || {};
    const groupImpressions =
      (await this._storage.get("groupImpressions")) || {};
    // Combine the existing providersBlockList into the groupBlockList
    const groupBlockList = (
      (await this._storage.get("groupBlockList")) || []
    ).concat(providerBlockList);

    // Merge any existing provider impressions into the corresponding group
    // Don't keep providerImpressions in state anymore
    const providerImpressions =
      (await this._storage.get("providerImpressions")) || {};
    for (const provider of Object.keys(providerImpressions)) {
      groupImpressions[provider] = [
        ...(groupImpressions[provider] || []),
        ...providerImpressions[provider],
      ];
    }

    const previousSessionEnd =
      (await this._storage.get("previousSessionEnd")) || 0;
    await this.setState({
      messageBlockList,
      groupBlockList,
      providerBlockList,
      groupImpressions,
      messageImpressions,
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

    Services.obs.addObserver(this._onLocaleChanged, TOPIC_INTL_LOCALE_CHANGED);
    Services.prefs.addObserver(USE_REMOTE_L10N_PREF, this);
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
    ToolbarPanelHub.uninit();
    ToolbarBadgeHub.uninit();
    MomentsPageHub.uninit();

    // Uninitialise all trigger listeners
    for (const listener of ASRouterTriggerListeners.values()) {
      listener.uninit();
    }
    Services.obs.removeObserver(
      this._onLocaleChanged,
      TOPIC_INTL_LOCALE_CHANGED
    );
    Services.prefs.removeObserver(USE_REMOTE_L10N_PREF, this);
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
        trailhead: ASRouterPreferences.trailhead,
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
          event_context: type,
        })
      );
    }
  }

  async setTrailHeadMessageSeen() {
    if (!this.state.trailheadInitialized) {
      Services.prefs.setBoolPref(
        TRAILHEAD_CONFIG.DID_SEE_ABOUT_WELCOME_PREF,
        true
      );
      await this.setState({
        trailheadInitialized: true,
      });
    }
  }

  // Return an object containing targeting parameters used to select messages
  _getMessagesContext() {
    const { messageImpressions, previousSessionEnd } = this.state;

    return {
      get messageImpressions() {
        return messageImpressions;
      },
      get previousSessionEnd() {
        return previousSessionEnd;
      },
    };
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
      data: {
        ...this.state,
        evaluationStatus,
      },
    });
  }

  _orderBundle(bundle) {
    return bundle.sort((a, b) => a.order - b.order);
  }

  isUnblockedMessage(message) {
    let { state } = this;
    return (
      !state.messageBlockList.includes(message.id) &&
      (!message.campaign ||
        !state.messageBlockList.includes(message.campaign)) &&
      !state.providerBlockList.includes(message.provider) &&
      this.hasGroupsEnabled(message.groups) &&
      !this.isExcludedByProvider(message)
    );
  }

  // Work out if a message can be shown based on its and its provider's frequency caps.
  isBelowFrequencyCaps(message) {
    const { messageImpressions, groupImpressions } = this.state;
    const impressionsForMessage = messageImpressions[message.id];

    return (
      this._isBelowItemFrequencyCap(
        message,
        impressionsForMessage,
        MAX_MESSAGE_LIFETIME_CAP
      ) &&
      message.groups.every(messageGroup =>
        this._isBelowItemFrequencyCap(
          this.state.groups.find(({ id }) => id === messageGroup),
          groupImpressions[messageGroup]
        )
      )
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
    let bundledMessagesOfSameTemplate = this.state.messages.filter(
      msg =>
        msg.bundled &&
        msg.template === bundleTemplate &&
        msg.id !== originalId &&
        this.isUnblockedMessage(msg)
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
      // Find all messages that matches the targeting context
      const allMessages = await this.handleMessageRequest({
        messages: bundledMessagesOfSameTemplate,
        triggerId: trigger && trigger.id,
        triggerContext: trigger && trigger.context,
        triggerParam: trigger && trigger.param,
        ordered: true,
        returnAll: true,
      });

      if (allMessages && allMessages.length) {
        // Retrieve enough messages needed to fill a bundle
        // Only copy the content of the message (that's what the UI cares about)
        result = result.concat(
          allMessages.slice(0, bundleLength).map(message => ({
            content: message.content,
            id: message.id,
            order: message.order || 0,
            // This is used to determine whether to block when action is triggered
            // Only block for dynamic triplets experiment and when there are more messages available
            blockOnClick:
              ASRouterPreferences.trailhead.trailheadTriplet.startsWith(
                "dynamic"
              ) &&
              allMessages.length >
                TRAILHEAD_CONFIG.DYNAMIC_TRIPLET_BUNDLE_LENGTH,
          }))
        );
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

  /**
   * Route messages based on template to the correct module that can display them
   */
  routeMessageToTarget(message, target, trigger, force = false) {
    switch (message.template) {
      case "whatsnew_panel_message":
        if (force) {
          ToolbarPanelHub.forceShowMessage(target, message);
        }
        break;
      case "cfr_doorhanger":
        if (force) {
          CFRPageActions.forceRecommendation(target, message, this.dispatch);
        } else {
          CFRPageActions.addRecommendation(
            target,
            trigger.param && trigger.param.host,
            message,
            this.dispatch
          );
        }
        break;
      case "cfr_urlbar_chiclet":
        if (force) {
          CFRPageActions.forceRecommendation(target, message, this.dispatch);
        } else {
          CFRPageActions.addRecommendation(
            target,
            null,
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
        ToolbarBadgeHub.registerBadgeNotificationListener(message, { force });
        break;
      case "update_action":
        MomentsPageHub.executeAction(message);
        break;
      case "milestone_message":
        CFRPageActions.showMilestone(target, message, this.dispatch, { force });
        break;
      default:
        try {
          target.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {
            type: "SET_MESSAGE",
            data: message,
          });
        } catch (e) {}
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
            trailheadTriplet:
              ASRouterPreferences.trailhead.trailheadTriplet || "",
            bundle: bundledMessages && bundledMessages.bundle,
          },
        });
      } catch (e) {}
    } else {
      this.routeMessageToTarget(message, target, trigger, force);
    }
  }

  async addImpression(message) {
    const groupsWithFrequency = this.state.groups.filter(
      ({ frequency, id }) => frequency && message.groups.includes(id)
    );
    // We only need to store impressions for messages that have frequency, or
    // that have providers that have frequency
    if (message.frequency || groupsWithFrequency.length) {
      const time = Date.now();
      await this.setState(state => {
        const messageImpressions = this._addImpressionForItem(
          state,
          message,
          "messageImpressions",
          time
        );
        let { groupImpressions } = this.state;
        for (const group of groupsWithFrequency) {
          groupImpressions = this._addImpressionForItem(
            state,
            group,
            "groupImpressions",
            time
          );
        }
        return { messageImpressions, groupImpressions };
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
      const groupImpressions = this._cleanupImpressionsForItems(
        state,
        state.groups,
        "groupImpressions"
      );
      this._cleanupImpressionsForItems(
        state,
        state.providers,
        "providerImpressions"
      );
      return { messageImpressions, groupImpressions };
    });
  }

  /** _cleanupImpressionsForItems - Helper for cleanupImpressions - calculate the updated
  /*                                impressions object for the given items, then store it and return it
   *
   * @param {obj} state Reference to ASRouter internal state
   * @param {array} items Can be messages, providers or groups that we count impressions for
   * @param {string} impressionsString Key name for entry in state where impressions are stored
   */
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
    messages: candidates,
    triggerId,
    triggerParam,
    triggerContext,
    template,
    provider,
    ordered = false,
    returnAll = false,
  }) {
    let shouldCache;
    const messages =
      candidates ||
      this.state.messages.filter(m => {
        if (provider && m.provider !== provider) {
          return false;
        }
        if (template && m.template !== template) {
          return false;
        }
        if (triggerId && !m.trigger) {
          return false;
        }
        if (triggerId && m.trigger.id !== triggerId) {
          return false;
        }
        if (!this.isUnblockedMessage(m)) {
          return false;
        }
        if (!this.isBelowFrequencyCaps(m)) {
          return false;
        }

        if (shouldCache !== false) {
          shouldCache = JEXL_PROVIDER_CACHE.has(m.provider);
        }

        return true;
      });

    if (!messages.length) {
      return returnAll ? messages : null;
    }

    const context = this._getMessagesContext();

    // Find a message that matches the targeting context as well as the trigger context (if one is provided)
    // If no trigger is provided, we should find a message WITHOUT a trigger property defined.
    return ASRouterTargeting.findMatchingMessage({
      messages,
      trigger: triggerId && {
        id: triggerId,
        param: triggerParam,
        context: triggerContext,
      },
      context,
      onError: this._handleTargetingError,
      ordered,
      shouldCache,
      returnAll,
    });
  }

  async setMessageById(id, target, force = true, action = {}) {
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

  unblockMessageById(idOrIds) {
    const idsToUnblock = Array.isArray(idOrIds) ? idOrIds : [idOrIds];

    return this.setState(state => {
      const messageBlockList = [...state.messageBlockList];
      idsToUnblock
        .map(id => state.messages.find(m => m.id === id))
        // Remove all `id`s (or `campaign`s for snippets) from the message
        // block list
        .forEach(message => {
          const idToUnblock =
            message && message.campaign ? message.campaign : message.id;
          messageBlockList.splice(messageBlockList.indexOf(idToUnblock), 1);
        });

      this._storage.set("messageBlockList", messageBlockList);
      return { messageBlockList };
    });
  }

  /**
   * Sets `group.enabled` to false, blocks associated messages and persists
   * the information in indexedDB
   * @param id {string} - identifier for group
   */
  blockGroupById(id) {
    if (!id) {
      return false;
    }
    const groupBlockList = [...this.state.groupBlockList, id];
    this._storage.set("groupBlockList", groupBlockList);
    return this.setGroupState({ id, value: false });
  }

  /**
   * Sets `group.enabled` to true, unblocks associated messages and persists
   * the information in indexedDB
   * @param id {string} - identifier for group
   */
  unblockGroupById(id) {
    if (!id) {
      return false;
    }
    const groupBlockList = [
      ...this.state.groupBlockList.filter(groupId => groupId !== id),
    ];
    this._storage.set("groupBlockList", groupBlockList);
    return this.setGroupState({ id, value: true });
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

  setGroupState({ id, value }) {
    const newGroupState = {
      ...this.state.groups.find(group => group.id === id),
      enabled: value,
    };
    const newGroupImpressions = { ...this.state.groupImpressions };
    delete newGroupImpressions[id];
    return this.setState(({ groups }) => ({
      groups: [...groups.filter(group => group.id !== id), newGroupState],
      groupImpressions: newGroupImpressions,
    }));
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
    // Disable ASRouterTriggerListeners in kiosk mode.
    if (BrowserHandler.kiosk) {
      return;
    }
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
  async forceAttribution(data) {
    // Extract the parameters from data that will make up the referrer url
    const attributionData = AttributionCode.allowedCodeKeys
      .map(key => `${key}=${encodeURIComponent(data[key] || "")}`)
      .join("&");
    if (AppConstants.platform === "win") {
      // The whole attribution data is encoded (again) for windows
      this._writeAttributionFile(encodeURIComponent(attributionData));
    } else if (AppConstants.platform === "macosx") {
      let appPath = Services.dirsvc.get("GreD", Ci.nsIFile).parent.parent.path;
      let attributionSvc = Cc["@mozilla.org/mac-attribution;1"].getService(
        Ci.nsIMacAttributionService
      );

      // The attribution data is treated as a url query for mac
      let referrer = `https://www.mozilla.org/anything/?${attributionData}`;

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
      case ra.SHOW_MIGRATION_WIZARD:
        MigrationUtils.showMigrationWizard(target.browser.ownerGlobal, [
          MigrationUtils.MIGRATION_ENTRYPOINT_NEWTAB,
        ]);
        break;
      case ra.OPEN_PRIVATE_BROWSER_WINDOW:
        // Forcefully open about:privatebrowsing
        target.browser.ownerGlobal.OpenBrowserWindow({ private: true });
        break;
      case ra.OPEN_URL:
        target.browser.ownerGlobal.openLinkIn(
          Services.urlFormatter.formatURL(action.data.args),
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
        target.browser.ownerGlobal.openPreferences(
          action.data.category,
          action.data.entrypoint && {
            urlParams: { entrypoint: action.data.entrypoint },
          }
        );
        break;
      case ra.OPEN_APPLICATIONS_MENU:
        UITour.showMenu(target.browser.ownerGlobal, action.data.args);
        break;
      case ra.HIGHLIGHT_FEATURE:
        const highlight = await UITour.getTarget(
          target.browser.ownerGlobal,
          action.data.args
        );
        if (highlight) {
          await UITour.showHighlight(
            target.browser.ownerGlobal,
            highlight,
            "none",
            { autohide: true }
          );
        }
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
        const url = await FxAccounts.config.promiseConnectAccountURI(
          "snippets"
        );
        // We want to replace the current tab.
        target.browser.ownerGlobal.openLinkIn(url, "current", {
          private: false,
          triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
            {}
          ),
          csp: null,
        });
        break;
      case ra.OPEN_PROTECTION_PANEL:
        let { gProtectionsHandler } = target.browser.ownerGlobal;
        gProtectionsHandler.showProtectionsPopup({});
        break;
      case ra.OPEN_PROTECTION_REPORT:
        target.browser.ownerGlobal.gProtectionsHandler.openProtections();
        break;
      case ra.DISABLE_STP_DOORHANGERS:
        await this.blockMessageById([
          "SOCIAL_TRACKING_PROTECTION",
          "FINGERPRINTERS_PROTECTION",
          "CRYPTOMINERS_PROTECTION",
        ]);
        break;
    }
  }

  /**
   * sendAsyncMessageToPreloaded - Sends an action to each preloaded browser, if any
   *
   * @param  {obj} action An action to be sent to content
   */
  sendAsyncMessageToPreloaded(action) {
    const preloadedBrowsers = this.getPreloadedBrowser();
    if (preloadedBrowsers) {
      for (let preloadedBrowser of preloadedBrowsers) {
        try {
          preloadedBrowser.sendAsyncMessage(OUTGOING_MESSAGE_NAME, action);
        } catch (e) {
          // The preloaded page is no longer available, so just ignore.
        }
      }
    }
  }

  /**
   * getPreloadedBrowser - Retrieve the port of any preloaded browsers
   *
   * @return {Array|null} An array of ports belonging to the preloaded browsers, or null
   *                      if there aren't any preloaded browsers
   */
  getPreloadedBrowser() {
    let preloadedPorts = [];
    for (let port of this.messageChannel.messagePorts) {
      if (this.isPreloadedBrowser(port.browser)) {
        preloadedPorts.push(port);
      }
    }
    return preloadedPorts.length ? preloadedPorts : null;
  }

  /**
   * isPreloadedBrowser - Returns true if the passed browser has been preloaded
   *                      for faster rendering of new tabs.
   *
   * @param {<browser>} A <browser> to check.
   * @return {boolean} True if the browser is preloaded.
   *                   False if there aren't any preloaded browsers
   */
  isPreloadedBrowser(browser) {
    return browser.getAttribute("preloadedState") === "preloaded";
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
      if (message) {
        await this.setState(state => ({
          messages: state.messages.filter(m => m.id !== message.id),
        }));
      }
    } else {
      const telemetryObject = { port: target.portID };
      TelemetryStopwatch.start("MS_MESSAGE_REQUEST_TIME_MS", telemetryObject);
      // On new tab, send cards if they match; othwerise send a snippet
      message = await this.handleMessageRequest({
        template: "extended_triplets",
      });

      // If no extended triplets message was returned, show snippets instead
      if (!message) {
        message = await this.handleMessageRequest({ provider: "snippets" });
      }
      TelemetryStopwatch.finish("MS_MESSAGE_REQUEST_TIME_MS", telemetryObject);
    }

    await this._sendMessageToTarget(message, target);
  }

  async sendTriggerMessage(target, trigger) {
    await this.loadMessagesFromAllProviders();

    if (trigger.id === "firstRun") {
      // On about welcome, set trailhead message seen on receiving firstrun trigger
      await this.setTrailHeadMessageSeen();
    }

    const telemetryObject = { port: target.portID };
    TelemetryStopwatch.start("MS_MESSAGE_REQUEST_TIME_MS", telemetryObject);
    const message = await this.handleMessageRequest({
      triggerId: trigger.id,
      triggerParam: trigger.param,
      triggerContext: trigger.context,
    });
    TelemetryStopwatch.finish("MS_MESSAGE_REQUEST_TIME_MS", telemetryObject);

    await this._sendMessageToTarget(message, target, trigger);
  }

  renderWNMessages(browserWindow, messageIds) {
    let messages = messageIds.map(msgId => this.getMessageById(msgId));

    ToolbarPanelHub.forceShowMessage(browserWindow, messages);
  }

  async forceWNPanel(browserWindow) {
    await ToolbarPanelHub.enableToolbarButton();

    browserWindow.PanelUI.showSubView(
      "PanelUI-whatsNew",
      browserWindow.document.getElementById("whats-new-menu-button")
    );
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
        break;
      case "BLOCK_MESSAGE_BY_ID":
        await this.blockMessageById(action.data.id);
        // Block the message but don't dismiss it in case the action taken has
        // another state that needs to be visible
        if (action.data.preventDismiss) {
          break;
        }

        const outgoingMessage = {
          type: "CLEAR_MESSAGE",
          data: { id: action.data.id },
        };
        if (action.data.preloadedOnly) {
          this.sendAsyncMessageToPreloaded(outgoingMessage);
        } else {
          this.messageChannel.sendAsyncMessage(
            OUTGOING_MESSAGE_NAME,
            outgoingMessage
          );
        }
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
      case "TOOLBAR_PANEL_TELEMETRY":
      case "MOMENTS_PAGE_TELEMETRY":
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
      case "SET_GROUP_STATE":
        await this.setGroupState(action.data);
        await this.loadMessagesFromAllProviders();
        break;
      case "BLOCK_GROUP_BY_ID":
        await this.blockGroupById(action.data.id);
        await this.loadMessagesFromAllProviders();
        break;
      case "UNBLOCK_GROUP_BY_ID":
        await this.unblockGroupById(action.data.id);
        await this.loadMessagesFromAllProviders();
        break;
      case "EVALUATE_JEXL_EXPRESSION":
        this.evaluateExpression(target, action.data);
        break;
      case "FORCE_ATTRIBUTION":
        this.forceAttribution(action.data);
        break;
      case "FORCE_WHATSNEW_PANEL":
        this.forceWNPanel(target.browser.ownerGlobal);
        break;
      case "RENDER_WHATSNEW_MESSAGES":
        this.renderWNMessages(target.browser.ownerGlobal, action.data);
        break;
      default:
        Cu.reportError("Unknown message received");
        break;
    }
  }
}
this._ASRouter = _ASRouter;
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
  "TRAILHEAD_CONFIG",
];
