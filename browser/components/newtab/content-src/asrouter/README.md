# Activity Stream Router

## Preferences `browser.newtab.activity-stream.asrouter.*`

Name | Used for | Type | Example value
---  | ---      | ---  | ---
`whitelistHosts` | Whitelist a host in order to fetch messages from its endpoint | `[String]` |  `["gist.github.com", "gist.githubusercontent.com", "localhost:8000"]`
`messageProviders` | Message provider options | `Object` | [see below](#message-providers)

### Message providers

```json
[
   {
      "id" : "onboarding",
      "type" : "local",
      "localProvider" : "OnboardingMessageProvider"
   },
   {
      "type" : "remote",
      "url" : "https://snippets.cdn.mozilla.net/us-west/bundles/bundle_d6d90fb9098ce8b45e60acf601bcb91b68322309.json",
      "updateCycleInMs" : 14400000,
      "id" : "snippets"
   }
]
```

## Admin Interface

* Navigate to `about:newtab#asrouter`
  * See all available messages and message providers
  * Block, unblock or force show a specific message

## Snippet Preview

* Whitelist the provider host that will serve the messages
  * In `about:config`, `browser.newtab.activity-stream.asrouter.whitelistHosts` can contain a array of hosts
  * Example value `["gist.github.com", "gist.githubusercontent.com", "localhost:8000"]`
  * Errors are surfaced in the `Console` tab of the `Browser Toolbox` ([read how to enable](https://developer.mozilla.org/en-US/docs/Tools/Browser_Toolbox))
* Navigate to `about:newtab?endpoint=<URL>`
  * Example `https://gist.githubusercontent.com/piatra/70234f08696c0a0509d7ba5568cd830f/raw/68370f34abc134142c64b6f0a9b9258a06de7aa3/messages.json`
  * URL should be from an endpoint that was just whitelisted
  * The snippet preview should imediately load
  * The endpoint must be HTTPS, the host must be whitelisted
  * Errors are surfaced in the `Console` tab of the `Browser Toolbox`

### [Snippet message format documentation](https://github.com/mozilla/activity-stream/blob/master/content-src/asrouter/schemas/message-format.md)
