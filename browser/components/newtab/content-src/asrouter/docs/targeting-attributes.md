# Targeting attributes

When you create ASRouter messages such as snippets, contextual feature recommendations, or onboarding cards, you may choose to include **targeting information** with those messages.

Targeting information must be captured in [an expression](./targeting-guide.md) that has access to the following attributes. You may combine and compare any of these attributes as needed.

Please note that some targeting attributes require stricter controls on the telemetry than can be colleted, so when in doubt, ask for review.

## Available attributes

* [addonsInfo](#addonsinfo)
* [attributionData](#attributiondata)
* [browserSettings](#browsersettings)
* [currentDate](#currentdate)
* [devToolsOpenedCount](#devtoolsopenedcount)
* [isDefaultBrowser](#isdefaultbrowser)
* [firefoxVersion](#firefoxversion)
* [locale](#locale)
* [localeLanguageCode](#localelanguagecode)
* [needsUpdate](#needsupdate)
* [pinnedSites](#pinnedsites)
* [previousSessionEnd](#previoussessionend)
* [profileAgeCreated](#profileagecreated)
* [profileAgeReset](#profileagereset)
* [providerCohorts](#providercohorts)
* [region](#region)
* [searchEngines](#searchengines)
* [sync](#sync)
* [topFrecentSites](#topfrecentsites)
* [totalBookmarksCount](#totalbookmarkscount)
* [trailheadInterrupt](#trailheadinterrupt)
* [trailheadTriplet](#trailheadtriplet)
* [usesFirefoxSync](#usesfirefoxsync)
* [isFxAEnabled](#isFxAEnabled)
* [xpinstallEnabled](#xpinstallEnabled)
* [hasPinnedTabs](#haspinnedtabs)
* [hasAccessedFxAPanel](#hasaccessedfxapanel)
* [isWhatsNewPanelEnabled](#iswhatsnewpanelenabled)
* [earliestFirefoxVersion](#earliestfirefoxversion)

## Detailed usage

### `addonsInfo`
Provides information about the add-ons the user has installed.

Note that the `name`, `userDisabled`, and `installDate` is only available if `isFullData` is `true` (this is usually not the case right at start-up).

**Due to an existing bug, `userDisabled` is not currently available**

#### Examples
* Has the user installed the unicorn addon?
```java
addonsInfo.addons["unicornaddon@mozilla.org"]
```

* Has the user installed and disabled the unicorn addon?
```java
addonsInfo.isFullData && addonsInfo.addons["unicornaddon@mozilla.org"].userDisabled
```

#### Definition
```ts
declare const addonsInfo: Promise<AddonsInfoResponse>;
interface AddonsInfoResponse {
  // Does this include extra information requiring I/O?
  isFullData: boolean;
  // addonId should be something like activity-stream@mozilla.org
  [addonId: string]: {
    // Version of the add-on
    version: string;
    // (string) e.g. "extension"
    type: AddonType;
    // Version of the add-on
    isSystem: boolean;
    // Is the add-on a webextension?
    isWebExtension: boolean;
    // The name of the add-on
    name: string;
    // Is the add-on disabled?
    // CURRENTLY UNAVAILABLE due to an outstanding bug
    userDisabled: boolean;
    // When was it installed? e.g. "2018-03-10T03:41:06.000Z"
    installDate: string;
  };
}
```
### `attributionData`

An object containing information on exactly how Firefox was downloaded

#### Examples
* Was the browser installed via the `"back_to_school"` campaign?
```java
attributionData && attributionData.campaign == "back_to_school"
```

#### Definition
```ts
declare const attributionData: AttributionCode;
interface AttributionCode {
  // Descriptor for where the download started from
  campaign: string,
  // A source, like addons.mozilla.org, or google.com
  source: string,
  // The medium for the download, like if this was referral
  medium: string,
  // Additional content, like an addonID for instance
  content: string
}
```

### `browserSettings`

Includes two properties:
* `attribution`, which indicates how Firefox was downloaded - DEPRECATED - please use [attributionData](#attributiondata)
* `update`, which has information about how Firefox updates

Note that attribution can be `undefined`, so you should check that it exists first.

#### Examples
* Is updating enabled?
```java
browserSettings.update.enabled
```

#### Definition

```ts
declare const browserSettings: {
  attribution: undefined | {
    // Referring partner domain, when install happens via a known partner
    // e.g. google.com
    source: string;
    // category of the source, such as "organic" for a search engine
    // e.g. organic
    medium: string;
    // identifier of the particular campaign that led to the download of the product
    // e.g. back_to_school
    campaign: string;
    // identifier to indicate the particular link within a campaign
    // e.g. https://mozilla.org/some-page
    content: string;
  },
  update: {
    // Is auto-downloading enabled?
    autoDownload: boolean;
    // What release channel, e.g. "nightly"
    channel: string;
    // Is updating enabled?
    enabled: boolean;
  }
}
```

### `currentDate`

The current date at the moment message targeting is checked.

#### Examples
* Is the current date after Oct 3, 2018?
```java
currentDate > "Wed Oct 03 2018 00:00:00"|date
```

#### Definition

```ts
declare const currentDate; ECMA262DateString;
// ECMA262DateString = Date.toString()
type ECMA262DateString = string;
```

### `devToolsOpenedCount`
Number of usages of the web console or scratchpad.

#### Examples
* Has the user opened the web console more than 10 times?
```java
devToolsOpenedCount > 10
```

#### Definition
```ts
declare const devToolsOpenedCount: number;
```

### `isDefaultBrowser`

Is Firefox the user's default browser?

#### Definition

```ts
declare const isDefaultBrowser: boolean;
```

### `firefoxVersion`

The major Firefox version of the browser

#### Examples
* Is the version of the browser greater than 63?
```java
firefoxVersion > 63
```

#### Definition

```ts
declare const firefoxVersion: number;
```

### `locale`
The current locale of the browser including country code, e.g. `en-US`.

#### Examples
* Is the locale of the browser either English (US) or German (Germany)?
```java
locale in ["en-US", "de-DE"]
```

#### Definition
```ts
declare const locale: string;
```

### `localeLanguageCode`
The current locale of the browser NOT including country code, e.g. `en`.
This is useful for matching all countries of a particular language.

#### Examples
* Is the locale of the browser any English locale?
```java
localeLanguageCode == "en"
```

#### Definition
```ts
declare const localeLanguageCode: string;
```

### `needsUpdate`

Does the client have the latest available version installed

```ts
declare const needsUpdate: boolean;
```

### `pinnedSites`
The sites (including search shortcuts) that are pinned on a user's new tab page.

#### Examples
* Has the user pinned any site on `foo.com`?
```java
"foo.com" in pinnedSites|mapToProperty("host")
```

* Does the user have a pinned `duckduckgo.com` search shortcut?
```java
"duckduckgo.com" in pinnedSites[.searchTopSite == true]|mapToProperty("host")
```

#### Definition
```ts
interface PinnedSite {
  // e.g. https://foo.mozilla.com/foo/bar
  url: string;
  // e.g. foo.mozilla.com
  host: string;
  // is the pin a search shortcut?
  searchTopSite: boolean;
}
declare const pinnedSites: Array<PinnedSite>
```

### `previousSessionEnd`

Timestamp of the previously closed session.

#### Definition
```ts
declare const previousSessionEnd: UnixEpochNumber;
// UnixEpochNumber is UNIX Epoch timestamp, e.g. 1522843725924
type UnixEpochNumber = number;
```

### `profileAgeCreated`

The date the profile was created as a UNIX Epoch timestamp.

#### Definition

```ts
declare const profileAgeCreated: UnixEpochNumber;
// UnixEpochNumber is UNIX Epoch timestamp, e.g. 1522843725924
type UnixEpochNumber = number;
```

### `profileAgeReset`

The date the profile was reset as a UNIX Epoch timestamp (if it was reset).

#### Examples
* Was the profile never reset?
```java
!profileAgeReset
```

#### Definition
```ts
// profileAgeReset can be undefined if the profile was never reset
// UnixEpochNumber is number, e.g. 1522843725924
declare const profileAgeReset: undefined | UnixEpochNumber;
// UnixEpochNumber is UNIX Epoch timestamp, e.g. 1522843725924
type UnixEpochNumber = number;
```

### `providerCohorts`

Information about cohort settings (from prefs, including shield studies) for each provider.

#### Examples
* Is the user in the "foo_test" cohort for snippets?
```java
providerCohorts.snippets == "foo_test"
```

#### Definition

```ts
declare const providerCohorts: {
  [providerId: string]: string;
}
```

### `region`

Country code retrieved from `location.services.mozilla.com`. Can be `""` if request did not finish or encountered an error.

#### Examples
* Is the user in Canada?
```java
region == "CA"
```

#### Definition

```ts
declare const region: string;
```

### `searchEngines`

Information about the current and available search engines.

#### Examples
* Is the current default search engine set to google?
```java
searchEngines.current == "google"
```

#### Definition

```ts
declare const searchEngines: Promise<SearchEnginesResponse>;
interface SearchEnginesResponse: {
  current: SearchEngineId;
  installed: Array<SearchEngineId>;
}
// This is an identifier for a search engine such as "google" or "amazondotcom"
type SearchEngineId = string;
```

### `sync`

Information about synced devices.

#### Examples
* Is at least 1 mobile device synced to this profile?
```java
sync.mobileDevices > 0
```

#### Definition

```ts
declare const sync: {
  desktopDevices: number;
  mobileDevices: number;
  totalDevices: number;
}
```

### `topFrecentSites`

Information about the browser's top 25 frecent sites.

**Please note this is a restricted targeting property that influences what telemetry is allowed to be collected may not be used without review**


#### Examples
* Is mozilla.com in the user's top frecent sites with a frececy greater than 400?
```java
"mozilla.com" in topFrecentSites[.frecency >= 400]|mapToProperty("host")
```

#### Definition
```ts
declare const topFrecentSites: Promise<Array<TopSite>>
interface TopSite {
  // e.g. https://foo.mozilla.com/foo/bar
  url: string;
  // e.g. foo.mozilla.com
  host: string;
  frecency: number;
  lastVisitDate: UnixEpochNumber;
}
// UnixEpochNumber is UNIX Epoch timestamp, e.g. 1522843725924
type UnixEpochNumber = number;
```

### `totalBookmarksCount`

Total number of bookmarks.

#### Definition

```ts
declare const totalBookmarksCount: number;
```

### `trailheadInterrupt`

(67.05+ only) Experiment branch for "interrupt" study

### `trailheadTriplet`

(67.05+ only) Experiment branch for "triplet" study

### `usesFirefoxSync`

Does the user use Firefox sync?

#### Definition

```ts
declare const usesFirefoxSync: boolean;
```

### `isFxAEnabled`

Does the user have Firefox sync enabled? The service could potentially be turned off [for enterprise builds](https://searchfox.org/mozilla-central/rev/b59a99943de4dd314bae4e44ab43ce7687ccbbec/browser/components/enterprisepolicies/Policies.jsm#327).

#### Definition

```ts
declare const isFxAEnabled: boolean;
```

### `xpinstallEnabled`

Pref used by system administrators to disallow add-ons from installed altogether.

#### Definition

```ts
declare const xpinstallEnabled: boolean;
```

### `hasPinnedTabs`

Does the user have any pinned tabs in any windows.

#### Definition

```ts
declare const hasPinnedTabs: boolean;
```

### `hasAccessedFxAPanel`

Boolean pref that gets set the first time the user opens the FxA toolbar panel

#### Definition

```ts
declare const hasAccessedFxAPanel: boolean;
```

### `isWhatsNewPanelEnabled`

Boolean pref that controls if the What's New panel feature is enabled

#### Definition

```ts
declare const isWhatsNewPanelEnabled: boolean;
```

### `earliestFirefoxVersion`

Integer value of the first Firefox version the profile ran on

#### Definition

```ts
declare const earliestFirefoxVersion: boolean;
```
