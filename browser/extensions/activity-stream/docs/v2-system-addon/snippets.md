# Snippets in ActivityStream

## `gSnippetsMap`

`gSnippetsMap` is available to all snippet templates and has methods that a
regular `Map` has: `set`, `get`, `delete`, `clear`, etc.

If you don't care about the completion of the transaction, you may simply use these
methods synchronously:

```js
function setFoo() {
  gSnippetsMap.set("foo", 123);
  return gSnippetsMap.get("foo"); // returns 123
}
```

However, unlike the previous version of snippets, `set`, `delete`, and `clear`
all return a `Promise` that will resolve or reject when the `indexedDB` transaction
is complete. For example:

```js
async function setFoo() {
  await gSnippetsMap.set("foo", 123);
  return gSnippetsMap.get("foo");
}
```

### Methods on gSnippetsMap

`.blockList`: (Array) An array of snippet IDs that have been blocked by the users, or an empty array if none have been blocked.

`.blockSnippetById(id)`: (func) A function that adds an id to a blockList and returns
a Promise that resolves when the blockList has been written to indexedDB.

`.showFirefoxAccounts()`: (func) A function that opens the Firefox Accounts signup page
(specifically `about:accounts?action=signup&entrypoint=snippets`)

`.disableOnboarding()`: (func) A function that disables onboarding for future new tabs (until Firefox restarts).
You should ensure `gSnippetsMap.get("appData.onboardingFinished")` is false before calling this. Note that this
does not hide onboarding notifications on the **current** page, since that would be a disruptive user experience.

`.getTotalBookmarksCount()`: (func) An async function that resolves with an integer representing the number of
bookmarks a user has (including default bookmarks). Note that is value is only updated once per day or when a
a user restarts the browser.

`.getAddonsInfo()`:( func) An async function that resolves with an object `{isFullData: (bool), addons: (obj)}`.
Note that at startup, we are sometimes unable to provide full data due to performance constraits (`.isFullData` will be `false`).
E.g.

```js
{
  isFullData: true,
  addons: {
      // This is the ID of the addon
    "someaddon@mozilla.org": {
      type: "extension",
      isSystem: false,
      isWebExtension: true,
      version: "29.0.0"

      // The following properties may not be available when the browser starts up
      name: "Firefox Screenshots",
      userDisabled: false,
      installDate: Date 2018-02-27T19:33:33.440Z,
    }
  }
}
```

### Expected values in gSnippetsMap

Note that names and functionality of values in v4 snippets have been preserved
where possible. You can access each value by calling `gSnippetsMap.get(id)`. For example,
to find out if a user has a Firefox account, you would check `gSnippetsMap.get("appData.fxaccount")`.

`snippets-cached-version`: (int) The version number of the snippets cached in `snippets`.

`snippets-last-update`: (int) The last time snippets were updated.

`snippets`: (str) The cached payload of the response from the snippets server.

`appData.snippetsURL`: (str) The URL at from which snippets are fetched

`appData.version`: (int) The current version of snippets

`appData.profileCreatedWeeksAgo`: (int) The date the user's profile was created

`appData.profileResetWeeksAgo`: (int) The date the user's profile was reset. `null` if the profile hasn't been reset.

`appData.telemetryEnabled`: (bool) Is telemetry enabled for the user?

`appData.onboardingFinished`: (bool) Has the onboarding tour been completed?

`appData.fxaccount`: (bool) Does the user have a Firefox account?

`appData.selectedSearchEngine`: (obj) An object containing a array of search
engines and a search engine identifier for the currently selected search engine.
For example:

```js
{
  engines: ["searchEngine-google", "searchEngine-yahoo", "searchEngine-bing"],
  searchEngineIdentifier : "google"
}
```

`appData.defaultBrowser`: (bool) Is Firefox the user's default browser?
If we could not determine the default browser, this value is `null`. Note that
currently this value is only checked once when the browser is initialized.

`appData.isDevtoolsUser`: (bool) Has the user ever used devtools?

## Events

ActivityStream dispatches custom DOM events on the window to allow snippets
providers to respond to ActivityStream snippets events.

### `Snippets:Enabled`

Fired when the user chooses to enable snippets.

### `Snippets:Disabled`

Fired when the user chooses to disable snippets.
