# User Actions

A subset of actions are available to messages via fields like `button_action` for snippets, or `primary_action` for CFRs.

## Usage

For snippets, you should add the action type in `button_action` and any additional parameters in `button_action_args. For example:

```json
{
  "button_action": "OPEN_ABOUT_PAGE",
  "button_action_args": "config"
}
```

## Available Actions

### `OPEN_APPLICATIONS_MENU`

* args: (none)

Opens the applications menu.

### `OPEN_PRIVATE_BROWSER_WINDOW`

* args: (none)

Opens a new private browsing window.


### `OPEN_URL`

* args: `string` (a url)

Opens a given url.

Example:

```json
{
  "button_action": "OPEN_URL",
  "button_action_args": "https://foo.com"
}
```

### `OPEN_ABOUT_PAGE`

* args: `string` (a valid about page without the `about:` prefix)

Opens a given about page

Example:

```json
{
  "button_action": "OPEN_ABOUT_PAGE",
  "button_action_args": "config"
}
```

### `OPEN_PREFERENCES_PAGE`

* args: `string` (a category accessible via a `#`)

Opens `about:preferences` with an optional category accessible via a `#` in the URL (e.g. `about:preferences#home`).

Example:

```json
{
  "button_action": "OPEN_PREFERENCES_PAGE",
  "button_action_args": "home"
}
```

### `SHOW_FIREFOX_ACCOUNTS`

* args: (none)

Opens Firefox accounts sign-up page. Encodes some information that the origin was from snippets by default.

### `PIN_CURRENT_TAB`

* args: (none)

Pins the currently focused tab.

### `ENABLE_FIREFOX_MONITOR`

* args:
```ts
{
  url: string;
  flowRequestParams: {
    entrypoint: string;
    utm_term: string;
    form_type: string;
  }
}
```

Opens an oauth flow to enable Firefox Monitor at a given `url` and adds Firefox metrics that user given a set of `flowRequestParams`.

### `url`

The URL should start with `https://monitor.firefox.com/oauth/init` and add various metrics tags as search params, including:

* `utm_source`
* `utm_campaign`
* `form_type`
* `entrypoint`

You should verify the values of these search params with whoever is doing the data analysis (e.g. Leif Oines).

### `flowRequestParams`

These params are used by Firefox to add information specific to that individual user to the final oauth URL. You should include:

* `entrypoint`
* `utm_term`
* `form_type`

The `entrypoint` and `form_type` values should match the encoded values in your `url`.

You should verify the values with whoever is doing the data analysis (e.g. Leif Oines).

### Example

```json
{
  "button_action": "ENABLE_FIREFOX_MONITOR",
  "button_action_args": {
     "url": "https://monitor.firefox.com/oauth/init?utm_source=snippets&utm_campaign=monitor-snippet-test&form_type=email&entrypoint=newtab",
      "flowRequestParams": {
        "entrypoint": "snippets",
        "utm_term": "monitor",
        "form_type": "email"
      }
  }
}
```
