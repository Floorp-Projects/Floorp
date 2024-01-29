# Remote CFR Messages
Starting in Firefox 68, CFR messages will be defined using [Remote Settings](https://remote-settings.readthedocs.io/en/latest/index.html). In this document, we'll cover how to set up a dev environment.

## Using a dev server for Remote CFR

> Note: Since Novembre 2021, Remote Settings has a proper DEV instance, which is
> reachable without VPN, but has the same config (openid, multi-signoff, ...)
> and collections as STAGE/PROD.

**1. Obtain your Bearer Token**

Until [Bug 1630651](https://bugzilla.mozilla.org/show_bug.cgi?id=1630651) happens, the easiest way to obtain your OpenID credentials is to use the admin interface.

1. [Login on the Admin UI](https://remote-settings-dev.allizom.org/v1/admin/) using your LDAP identity
2. Copy the authentication header (📋 icon in the top bar)
3. Test your credentials with ``curl``. When reaching out the server root URL with this bearer token you should see a ``user`` entry whose ``id`` field is ``ldap:<you>@mozilla.com``.

```bash
SERVER=https://settings.dev.mozaws.net/v1
BEARER_TOKEN="Bearer uLdb-Yafefe....2Hyl5_w"

curl -s ${SERVER}/ -H "Authorization:${BEARER_TOKEN}" | jq .user
```

**2. Create/Update/Delete CFR entries**

> The messages can also be created manually using the [admin interface](https://settings.dev.mozaws.net/v1/admin/).

In following example, we will create a new entry using the REST API (reusing `SERVER` and `BEARER_TOKEN` from previous step).

```bash
CID=cfr

# post a message
curl -X POST ${SERVER}/buckets/main-workspace/collections/${CID}/records \
     -d '{"data":{"id":"PIN_TAB","template":"cfr_doorhanger","content":{"category":"cfrFeatures","bucket_id":"CFR_PIN_TAB","notification_text":{"string_id":"cfr-doorhanger-extension-notification"},"heading_text":{"string_id":"cfr-doorhanger-pintab-heading"},"info_icon":{"label":{"string_id":"cfr-doorhanger-extension-sumo-link"},"sumo_path":"extensionrecommendations"},"text":{"string_id":"cfr-doorhanger-pintab-description"},"descriptionDetails":{"steps":[{"string_id":"cfr-doorhanger-pintab-step1"},{"string_id":"cfr-doorhanger-pintab-step2"},{"string_id":"cfr-doorhanger-pintab-step3"}]},"buttons":{"primary":{"label":{"string_id":"cfr-doorhanger-pintab-ok-button"},"action":{"type":"PIN_CURRENT_TAB"}},"secondary":[{"label":{"string_id":"cfr-doorhanger-extension-cancel-button"},"action":{"type":"CANCEL"}},{"label":{"string_id":"cfr-doorhanger-extension-never-show-recommendation"}},{"label":{"string_id":"cfr-doorhanger-extension-manage-settings-button"},"action":{"type":"OPEN_PREFERENCES_PAGE","data":{"category":"general-cfrfeatures"}}}]}},"targeting":"locale == \"en-US\" && !hasPinnedTabs && recentVisits[.timestamp > (currentDate|date - 3600 * 1000 * 1)]|length >= 3","frequency":{"lifetime":3},"trigger":{"id":"frequentVisits","params":["docs.google.com","www.docs.google.com","calendar.google.com","messenger.com","www.messenger.com","web.whatsapp.com","mail.google.com","outlook.live.com","facebook.com","www.facebook.com","twitter.com","www.twitter.com","reddit.com","www.reddit.com","github.com","www.github.com","youtube.com","www.youtube.com","feedly.com","www.feedly.com","drive.google.com","amazon.com","www.amazon.com","messages.android.com"]}}}' \
     -H 'Content-Type:application/json' \
     -H "Authorization:${BEARER_TOKEN}"
```

The collection was modified and now with pending changes in the workspace. We will now request a review, so that the changes become visible in the **preview** bucket.

```bash
# request review
curl -X PATCH ${SERVER}/buckets/main-workspace/collections/${CID} \
     -H 'Content-Type:application/json' \
     -d '{"data": {"status": "to-review"}}' \
     -H "Authorization:${BEARER_TOKEN}"
```

Now this new record should be listed here: https://settings.dev.mozaws.net/v1/buckets/main-preview/collections/cfr/records

**3. Set Remote Settings prefs to use the dev server.**

Until [support for the DEV environment](https://github.com/mozilla-extensions/remote-settings-devtools/issues/66) is added to the [Remote Settings dev tools](https://github.com/mozilla-extensions/remote-settings-devtools/), we'll change the preferences manually.

> These are critical preferences, you should use a dedicated Firefox profile for development.

```javascript
  Services.prefs.setCharPref("services.settings.loglevel", "debug");
  Services.prefs.setCharPref("services.settings.server", "https://settings.dev.mozaws.net/v1");
  // Pull data from the preview bucket.
  RemoteSettings.enablePreviewMode(true);
```

**3. Set ASRouter CFR pref to use Remote Settings provider and enable asrouter devtools.**

```javascript
Services.prefs.setStringPref("browser.newtabpage.activity-stream.asrouter.providers.cfr", JSON.stringify({"id":"cfr-remote","enabled":true,"type":"remote-settings","collection":"cfr"}));
Services.prefs.setBoolPref("browser.newtabpage.activity-stream.asrouter.devtoolsEnabled", true);
```

**4. Go to `about:asrouter`**
There should be a "cfr-remote" provider listed.

## Using the staging server for Remote CFR

If your message is published in the staging environment the easiest way to test is using the [Remote Settings Devtools](https://github.com/mozilla/remote-settings-devtools/releases) addon. You can install this by going to `about:debugging` and using the `Load Temporary Addon` feature.
The devtools allow you to switch your profile between production and staging and takes care of correctly flipping all the required preferences.

## Remote l10n
By default, all CFR messages are localized with the remote Fluent files hosted in `ms-language-packs` on Remote Settings. For local test and development, you can force ASRouter to use the local Fluent files by flipping the pref `browser.newtabpage.activity-stream.asrouter.useRemoteL10n`.
