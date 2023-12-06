## Activity Stream Router message format

Field name | Type     | Required | Description | Example / Note
---        | ---      | ---      | ---         | ---
`id`       | `string` | Yes | A unique identifier for the message that should not conflict with any other previous message | `ONBOARDING_1`
`template` | `string` | Yes | An id matching an existing Activity Stream Router template |
`content` | `object` | Yes | An object containing all variables/props to be rendered in the template. Subset of allowed tags detailed below. | [See example below](#html-subset)
`bundled` | `integer` | No | The number of messages of the same template this one should be shown with | [See example below](#a-bundled-message-example)
`order` | `integer` | No | If bundled with other messages of the same template, which order should this one be placed in? Defaults to 0 if no order is desired | [See example below](#a-bundled-message-example)
`campaign` | `string` | No | Campaign id that the message belongs to | `RustWebAssembly`
`targeting` | `string` `JEXL` | No | A [JEXL expression](http://normandy.readthedocs.io/en/latest/user/filter_expressions.html#jexl-basics) with all targeting information needed in order to decide if the message is shown | Not yet implemented, [Examples](#targeting-attributes)
`trigger` | `string` | No | An event or condition upon which the message will be immediately shown. This can be combined with `targeting`. Messages that define a trigger will not be shown during non-trigger-based passive message rotation.
`trigger.params` | `[string]` | No | A set of hostnames passed down as parameters to the trigger condition. Used to restrict the number of domains where the trigger/message is valid. | [See example below](#trigger-params)
`trigger.patterns` | `[string]` | No | A set of patterns that match multiple hostnames passed down as parameters to the trigger condition. Used to restrict the number of domains where the trigger/message is valid. | [See example below](#trigger-patterns)
`frequency` | `object` | No | A definition for frequency cap information for the message
`frequency.lifetime` | `integer` | No | The maximum number of lifetime impressions for the message.
`frequency.custom` | `array` | No | An array of frequency cap definition objects including `period`, a time period in milliseconds, and `cap`, a max number of impressions for that period.

### Message example
```javascript
{
  weight: 100,
  id: "PROTECTIONS_PANEL_1",
  template: "protections_panel",
  content: {
    title: {
      string_id: "cfr-protections-panel-header"
    },
    body: {
      string_id: "cfr-protections-panel-body"
    },
    link_text: {
      string_id: "cfr-protections-panel-link-text"
    },
    cta_url: "https://support.mozilla.org/1/firefox/121.0a1/Darwin/en-US/etp-promotions?as=u&utm_source=inproduct",
    cta_type: "OPEN_URL"
  },
  trigger: {
    id: "protectionsPanelOpen"
  },
  groups: [],
  provider: "onboarding"
}
```

### A Bundled Message example
The following 2 messages have a `bundled` property, indicating that they should be shown together, since they have the same template. The number `2` indicates that this message should be shown in a bundle of 2 messages of the same template. The order property defines that ONBOARDING_2 should be shown after ONBOARDING_3 in the bundle.
```javascript
{
  id: "ONBOARDING_2",
  template: "onboarding",
  bundled: 2,
  order: 2,
  content: {
    title: "Private Browsing",
    body: "Browse by yourself. Private Browsing with Tracking Protection blocks online trackers that follow you around the web."
  },
  targeting: "",
  trigger: "firstRun"
}
{
  id: "ONBOARDING_3",
  template: "onboarding",
  bundled: 2,
  order: 1,
  content: {
    title: "Find it faster",
    body: "Access all of your favorite search engines with a click. Search the whole Web or just one website from the search box."
  },
  targeting: "",
  trigger: "firstRun"
}
```

### HTML subset
The following tags are allowed in the content of a message: `i, b, u, strong, em, br`.

Links cannot be rendered using regular anchor tags because [Fluent does not allow for href attributes](https://github.com/projectfluent/fluent.js/blob/a03d3aa833660f8c620738b26c80e46b1a4edb05/fluent-dom/src/overlay.js#L13). They will be wrapped in custom tags, for example `<cta>link</cta>` and the url will be provided as part of the payload:
```
{
  "id": "7899",
  "content": {
    "text": "Use the CMD (CTRL) + T keyboard shortcut to <cta>open a new tab quickly!</cta>",
    "links": {
      "cta": {
        "url": "https://support.mozilla.org/en-US/kb/keyboard-shortcuts-perform-firefox-tasks-quickly"
      }
    }
  }
}
```
If a tag that is not on the allowed is used, the text content will be extracted and displayed.

Grouping multiple allowed elements is not possible, only the first level will be used: `<u><b>text</b></u>` will be interpreted as `<u>text</u>`.

### Trigger params
A set of hostnames that need to exactly match the location of the selected tab in order for the trigger to execute.
```
["github.com", "wwww.github.com"]
```
More examples in the [CFRMessageProvider](https://github.com/mozilla/activity-stream/blob/e76ce12fbaaac1182aa492b84fc038f78c3acc33/lib/CFRMessageProvider.jsm#L40-L47).

### Trigger patterns
A set of patterns that can match multiple hostnames. When the location of the selected tab matches one of the patterns it can execute a trigger.
```
["*://*.github.com"] // can match `github.com` but also match `https://gist.github.com/`
```
More [MatchPattern examples](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Match_patterns#Examples).

### Targeting attributes
(This section has moved to [targeting-attributes.md](../docs/targeting-attributes.md)).
