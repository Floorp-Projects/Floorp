# Remote CFR Messages
Starting in Firefox 68, CFR messages will be defined using [Remote Settings](https://remote-settings.readthedocs.io/en/latest/index.html). In this document, we'll cover how how to set up a dev environment.

## Using a dev server for Remote CFR

**1. Setup the Remote Settings dev server with the CFR messages.**
Note that the dev server gets wiped every 24 hours so this will be have to be done once a day. You can check if there currently are any messages [here](https://kinto.dev.mozaws.net/v1//buckets/main/collections/cfr/records).

```bash
SERVER=https://kinto.dev.mozaws.net/v1

# create user
curl -X PUT ${SERVER}/accounts/devuser \
     -d '{"data": {"password": "devpass"}}' \
     -H 'Content-Type:application/json'

BASIC_AUTH=devuser:devpass

# create collection
CID=cfr
curl -X PUT ${SERVER}/buckets/main/collections/${CID} \
     -H 'Content-Type:application/json' \
     -u ${BASIC_AUTH}

# post a message
curl -X POST ${SERVER}/buckets/main/collections/${CID}/records \
     -d '{"data":{"id":"PIN_TAB","template":"cfr_doorhanger","content":{"category":"cfrFeatures","bucket_id":"CFR_PIN_TAB","notification_text":{"string_id":"cfr-doorhanger-extension-notification"},"heading_text":{"string_id":"cfr-doorhanger-pintab-heading"},"info_icon":{"label":{"string_id":"cfr-doorhanger-extension-sumo-link"},"sumo_path":"extensionrecommendations"},"text":{"string_id":"cfr-doorhanger-pintab-description"},"descriptionDetails":{"steps":[{"string_id":"cfr-doorhanger-pintab-step1"},{"string_id":"cfr-doorhanger-pintab-step2"},{"string_id":"cfr-doorhanger-pintab-step3"}]},"buttons":{"primary":{"label":{"string_id":"cfr-doorhanger-pintab-ok-button"},"action":{"type":"PIN_CURRENT_TAB"}},"secondary":[{"label":{"string_id":"cfr-doorhanger-extension-cancel-button"},"action":{"type":"CANCEL"}},{"label":{"string_id":"cfr-doorhanger-extension-never-show-recommendation"}},{"label":{"string_id":"cfr-doorhanger-extension-manage-settings-button"},"action":{"type":"OPEN_PREFERENCES_PAGE","data":{"category":"general-cfrfeatures"}}}]}},"targeting":"locale == \"en-US\" && !hasPinnedTabs && recentVisits[.timestamp > (currentDate|date - 3600 * 1000 * 1)]|length >= 3","frequency":{"lifetime":3},"trigger":{"id":"frequentVisits","params":["docs.google.com","www.docs.google.com","calendar.google.com","messenger.com","www.messenger.com","web.whatsapp.com","mail.google.com","outlook.live.com","facebook.com","www.facebook.com","twitter.com","www.twitter.com","reddit.com","www.reddit.com","github.com","www.github.com","youtube.com","www.youtube.com","feedly.com","www.feedly.com","drive.google.com","amazon.com","www.amazon.com","messages.android.com"]}}}' \
     -H 'Content-Type:application/json' \
     -u ${BASIC_AUTH}
```

Now there should be a message listed: https://kinto.dev.mozaws.net/v1//buckets/main/collections/cfr/records

NOTE: The collection and messages can also be created manually using the [admin interface](https://kinto.dev.mozaws.net/v1/admin/).

**2. Set Remote Settings prefs to use the dev server.**

```javascript
Services.prefs.setStringPref("services.settings.server", "https://kinto.dev.mozaws.net/v1");

// Disable signature verification
const { RemoteSettings } = ChromeUtils.import("resource://services-settings/remote-settings.js", {});

RemoteSettings("cfr").verifySignature = false;
```

**3. Set ASRouter CFR pref to use Remote Settings provider and enable asrouter devtools.**

```javascript
Services.prefs.setStringPref("browser.newtabpage.activity-stream.asrouter.providers.cfr", JSON.stringify({"id":"cfr-remote","enabled":true,"type":"remote-settings","bucket":"cfr","frequency":{"custom":[{"period":"daily","cap":1}]},"categories":["cfrAddons","cfrFeatures"]}));
Services.prefs.setBoolPref("browser.newtabpage.activity-stream.asrouter.devtoolsEnabled", true);
```

**4. Go to `about:newtab#devtools`**
There should be a "cfr-remote" provider listed.
