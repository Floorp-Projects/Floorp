## Configuration

[![Npm version](https://img.shields.io/npm/v/devtools-config.svg)](https://npmjs.org/package/devtools-config)

All default config values are in [`config/development.json`](./development.json), to override these values you need to [create a local config file](#create-a-local-config-file).

* *logging*
  * `firefoxProxy` Enables logging the Firefox protocol in the terminal running `npm start`
* *chrome* Chrome browser related flags
  * `debug` Enables listening for remotely debuggable Chrome browsers
  * `port` web socket port specified when launching Chrome from the command line
  * `host` host specified when launching Chrome from the command line
* *node* Node related flags
  * `debug` Enables listening for remotely debuggable Node processes
  * `port` web socket port specified when connecting to node
  * `host` host specified when connecting to node
* *firefox* Firefox browser related flags
  * `webSocketConnection` favours Firefox WebSocket connection over the [firefox-proxy](../bin/firefox-proxy)
  * `host` The hostname used for connecting to Firefox
  * `webSocketPort` Port used for establishing a WebSocket connection with Firefox when `webSocketConnection` is `true` or with a [firefox-proxy](../bin/firefox-proxy) when `webSocketConnection` is `false`
  * `tcpPort` Port used by the [firefox-proxy](../bin/firefox-proxy) when connecting to Firefox
  * `geckoDir` Local location of Firefox source code _only needed by project maintainers_
*  *development* Development server related settings
  * `serverPort` Listen Port used by the development server
  * `examplesPort` Listen Port used to serve examples
* `hotReloading` enables [Hot Reloading](../docs/local-development.md#hot-reloading) of CSS and React
* `baseWorkerURL` Location for where the worker bundles exist

### Local config

You can create a `configs/local.json` file to override development configs. This is great for enabling features locally or changing the theme. Copy the `local-sample` to get started.

```bash
cp configs/local-sample.json configs/local-sample.json
```

> The `local.json` will be ignored by git so any changes you make won't be published, only make changes to the `development.json` file when related to features removed or added to the project.
