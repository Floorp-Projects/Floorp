## Activity Stream Router message format

Field name | Type     | Required | Description | Example / Note
---        | ---      | ---      | ---         | ---
`id`       | `string` | Yes | A unique identifier for the message that should not conflict with any other previous message | `ONBOARDING_1`
`template` | `string` | Yes | An id matching an existing Activity Stream Router template | [See example](https://github.com/mozilla/activity-stream/blob/33669c67c2269078a6d3d6d324fb48175d98f634/system-addon/content-src/message-center/templates/SimpleSnippet.jsx)
`publish_start` | `date` | No | When to start showing the message | `1524474850876`
`publish_end` | `date` | No | When to stop showing the message | `1524474850876`
`content` | `object` | Yes | An object containing all variables/props to be rendered in the template. Subset of allowed tags detailed below. | [See example below](#html-subset)
`campaign` | `string` | No | Campaign id that the message belongs to | `RustWebAssembly`
`targeting` | `string` `JEXL` | No | A [JEXL expression](http://normandy.readthedocs.io/en/latest/user/filter_expressions.html#jexl-basics) with all targeting information needed in order to decide if the message is shown | Not yet implemented, [Examples](#targeting-attributes)
`trigger` | `string` | No | An event or condition upon which the message will be immediately shown. This can be combined with `targeting`. Messages that define a trigger will not be shown during non-trigger-based passive message rotation.

### Message example
```javascript
{
  id: "ONBOARDING_1",
  template: "simple_snippet",
  content: {
    title: "Find it faster",
    body: "Access all of your favorite search engines with a click. Search the whole Web or just one website from the search box."
  }
}
```

### HTML subset
The following tags are allowed in the content of the snippet: `i, b, u, strong, em, br`.

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

### Targeting attributes
For a more in-depth explanation of JEXL syntax you can read the [Normady project docs](https://normandy.readthedocs.io/en/stable/user/filters.html#jexl-basics).

Currently we expose the following targeting attributes that can be used by messages:

Name | Type | Example value | Description
---  | ---  | ---           | ---      
`profileAgeCreated` | Number | `1522843725924` | Profile creation timestamp
`profileAgeReset` | `Number` or `undefined` | `1522843725924` | When (if) the profile was reset
`hasFxAccount` | `Boolean` | `true` | Does the user have a firefox account
`addonsInfo` | `Object` | [example below](#addonsinfo-example) | Information about the addons the user has installed

#### addonsInfo Example

```javascript
{
  "addons": {
    ...
    "activity-stream@mozilla.org": {
      "version": "2018.07.06.1113-783442c0",
      "type": "extension",
      "isSystem": true,
      "isWebExtension": false,
      "name": "Activity Stream",
      "userDisabled": false,
      "installDate": "2018-03-10T03:41:06.000Z"
    }
  },
  "isFullData": true
}
```

#### Usage
A message needs to contain the `targeting` property (JEXL string) which is evaluated against the provided attributes.
Examples:

```javascript
{
  "id": "7864",
  "content": {...},
  // simple equality check
  "targeting": "hasFxAccount == true"
}

{
  "id": "7865",
  "content": {...},
  // using JEXL transforms and combining two attributes
  "targeting": "hasFxAccount == true && profileAgeCreated > '2018-01-07'|date"
}

{
  "id": "7866",
  "content": {...},
  // targeting addon information
  "targeting": "addonsInfo.addons['activity-stream@mozilla.org'].name == 'Activity Stream'"
}
```
