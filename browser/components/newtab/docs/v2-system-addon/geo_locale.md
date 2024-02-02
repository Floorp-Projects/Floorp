# Custom `geo`, `locale`, and update channels

There are instances where you may need to change your local build's locale, geo, and update channel (such as changes to the visibility of Discovery Stream on a per-geo/locale basis in `ActivityStream.sys.mjs`).

## Changing update channel

- Change `app.update.channel` to desired value (eg: `release`) by editing `LOCAL_BUILD/Contents/Resources/defaults/pref/channel-prefs.js`. (**NOTE:** Changing pref `app.update.channel` from `about:config` seems to have no effect!)

## Changing geo

- Set `browser.search.region` to desired geo (eg `CA`)

## Changing locale

*Note: These prefs are only configurable on a nightly or local build.*

- Toggle `extensions.langpacks.signatures.required` to `false`
- Toggle `xpinstall.signatures.required` to `false`
- Toggle `intl.multilingual.downloadEnabled` to `true`
- Toggle `intl.multilingual.enabled` to `true`
- For Mac and Linux builds, open the [langpack](https://archive.mozilla.org/pub/firefox/nightly/latest-mozilla-central-l10n/linux-x86_64/xpi/) for target locale in your local build (eg `firefox-70.0a1.en-CA.langpack.xpi` if you want an `en-CA` locale).
- For Windows, use [https://archive.mozilla.org/pub/firefox/nightly/latest-mozilla-central-l10n/](https://archive.mozilla.org/pub/firefox/nightly/latest-mozilla-central-l10n/)
- In `about:preferences` click "Set Alternatives" under "Language", move desired locale to the top position, click OK, click "Apply And Restart"
