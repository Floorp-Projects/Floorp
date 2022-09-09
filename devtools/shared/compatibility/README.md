# Compatibility Dataset

## How to update the MDN compatibility data

The Compatibility panel detects issues by comparing against official [MDN compatibility data](https://github.com/mdn/browser-compat-data). It uses a local snapshot of the dataset. This dataset needs to be manually synchronized periodically to `devtools/shared/compatibility/dataset` (ideally with every Firefox release).

The subsets from the dataset required by the Compatibility panel are:

- browsers: [https://github.com/mdn/browser-compat-data/tree/master/browsers](https://github.com/mdn/browser-compat-data/tree/master/browsers)
- css.properties: [https://github.com/mdn/browser-compat-data/tree/master/css](https://github.com/mdn/browser-compat-data/tree/master/css).

In order to download up-to-date data, you need to run the following commands:

- `cd devtools/shared/compatibility`
- `yarn install --no-lockfile` and select the latest package version for the `@mdn/browser-compat-data` package
- `yarn update`

This should save the `css-properties.json` JSON file directly in `devtools/shared/compatibility/dataset/`.

The browsers data are stored in a RemoteSettings collection, and updates are handled by a script in https://github.com/firefox-devtools/remote-settings-mdn-browser-compat-data .
The script is run every day in automation, and if the data are updated, the team should receive a data review email.

To review the data update, you need to be connected to the Mozilla Corporate VPN (See https://mana.mozilla.org/wiki/display/SD/VPN), log into https://settings-writer.stage.mozaws.net/v1/admin/#/buckets/main/collections/devtools-compatibility-browsers/records (Using `OpenID Connect (LDAP)`)
Then run Firefox, and use the [RemoteSettings DevTools WebExtension](https://github.com/mozilla-extensions/remote-settings-devtools) to use the `Prod (preview)` environment and restart the browser.
Then open the compatibility panel and make sure that the updated browsers do appear in the `Settings` panel.

Check that all tests still pass. It is possible that changes in the structure or contents of the latest dataset will cause tests to fail. If that is the case, fix the tests. **Do not manually change the contents or structure of the local dataset** because any changes will be overwritten by the next update from the official dataset.
