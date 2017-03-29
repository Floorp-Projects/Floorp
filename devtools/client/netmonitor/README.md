# netmonitor

The Network Monitor shows you all the network requests Firefox makes (for example, when it loads a page, or due to XMLHttpRequests), how long each request takes, and details of each request.

## Prerequisite

* [node](https://nodejs.org/) >= 6.9.x
* [npm](https://docs.npmjs.com/getting-started/installing-node) >= 3.x
* [yarn](https://yarnpkg.com/docs/install) >= 0.21.x

## Quick Setup

Assume you are in netmonitor folder

```bash
# Install packages
yarn install

# Create a dev server instance for hosting netmonitor on browser
yarn start

# Run firefox
firefox http://localhost:8000 --start-debugger-server 6080
```

Then open localhost:8000 in any browser to see all tabs in Firefox.
