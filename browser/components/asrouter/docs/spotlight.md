# Spotlight
This is a window or tab level modal, the user is given a primary and a secondary button to interact with the modal.
Spotlights by default are `“window modal”` preventing access to the rest of the browser including opening and switching tabs. `“Tab modal”` grays out page content and allows interacting with tabs and the rest of the browser.

[More examples of templates supported in Spotlight](https://experimenter.info/messaging/desktop-messaging-surfaces/#multistage-spotlight)

## Example of Spotlight page

![Spotlight](./spotlight.png)

## Testing Spotlight
1. Go to `about:config`, set pref `browser.newtabpage.activity-stream.asrouter.devtoolsEnabled` to `true`
2. Open a new tab and go to `about:newtab#devtools` in the url bar
3. In devtools `Messages` section, select and show messages from `onboarding` as provider
4. You should see example JSON messages with `"template": "spotlight"`. Clicking `Show` next to spotlight template message should show respective message UI
5. For quick testing, you can directly modify the message in the text area with your changes or by pasting your custom screen message JSON. Clicking `Modify` shows your new updated spotlight message.
6. Ensure that all required properties are covered according to the [Spotlight Schema](https://searchfox.org/mozilla-central/source/browser/components/newtab/content-src/asrouter/templates/OnboardingMessage/Spotlight.schema.json)
7. Clicking `Share`, copies link to clipboard that can be pasted in the url bar to preview spotlight custom screen(s) in browser and can be shared to get feedback from your team.
- **Note:** Spotlight can be either window or tab level, with the `"modal": "tab"` or `"modal": "window"` property in the recipe

### Via the Experiments:
You can test the spotlight by creating an experiment or landing a message in tree. [Messaging Journey](https://experimenter.info/messaging/desktop-messaging-journey) captures creating and testing experiments via Nimbus and landing messages in Firefox.

### Example JSON recipe for Spotlight

```
{
  "template": "spotlight",
  "targeting": "firefoxVersion >= 114",
  "frequency": {
    "lifetime": 1
  },
  "trigger": {
    "id": "defaultBrowserCheck"
  },
  "content": {
    "template": "multistage",
    "id": "Spotlight_MESSAGE_ID",
    "transitions": true,
    "modal": "tab",
    "screens": [
      {
        "id": "Screen_ID",
        "content": {
          "logo": {
            "imageURL": "chrome://activity-stream/content/data/content/assets/heart.webp",
            "height": "73px"
          },
          "title": {
            "fontSize": "36px",
            "raw": "Say hello to Firefox"
          },
          "title_style": "fancy shine",
          "subtitle": {
            "lineHeight": "1.4",
            "marginBlock": "8px 16px",
            "raw": "Here’s a quick reminder that you can keep your favorite browser just one click away."
          },
          "primary_button": {
            "label": {
              "string_id": "onboarding-start-browsing-button-label"
            },
            "action": {
              "type": "OPEN_URL",
              "data": {
                "args": "https://www.mozilla.org",
                "where": "tab"
              }
            }
          },
          "secondary_button": {
            "action": {
              "navigate": true
            },
            "label": {
              "marginBlock": "0 -20px",
              "raw": "Not now"
            }
          },
          "dismiss_button": {
            "action": {
              "navigate": true
            }
          }
        }
      }
    ]
  }
}
```
