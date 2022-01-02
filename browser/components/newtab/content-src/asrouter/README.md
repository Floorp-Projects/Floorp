# Activity Stream Router

## Preferences `browser.newtab.activity-stream.asrouter.*`

Name | Used for | Type | Example value
---  | ---      | ---  | ---
`allowHosts` | Allow a host in order to fetch messages from its endpoint | `[String]` |  `["gist.github.com", "gist.githubusercontent.com", "localhost:8000"]`
`providers.snippets` | Message provider options for snippets | `Object` | [see below](#message-providers)
`providers.cfr` | Message provider options for cfr | `Object` | [see below](#message-providers)
`providers.onboarding` | Message provider options for onboarding | `Object` | [see below](#message-providers)
`useRemoteL10n` | Controls whether to use the remote Fluent files for l10n, default as `true` | `Boolean` | `[true|false]`

### Message providers examples

```json
{
  "id" : "snippets",
  "type" : "remote",
  "enabled": true,
  "url" : "https://snippets.cdn.mozilla.net/us-west/bundles/bundle_d6d90fb9098ce8b45e60acf601bcb91b68322309.json",
  "updateCycleInMs" : 14400000
}
```

```json
{
  "id" : "onboarding",
  "enabled": true,
  "type" : "local",
  "localProvider" : "OnboardingMessageProvider"
}
```

### [Snippet message format documentation](https://github.com/mozilla/activity-stream/blob/master/content-src/asrouter/schemas/message-format.md)
