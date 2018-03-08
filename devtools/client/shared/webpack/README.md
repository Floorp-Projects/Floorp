# Webpack Support
This directory contains modules intended to support and customize
DevTools source bundling.

DevTools use Webpack to generate bundles for individual tools,
which allow e.g. running them on top of the Launchpad (within
a browser tab).

Custom loaders implemented in this directory are mostly used to
rewrite existing code, so it's understandable for Webpack.

For example:

The following piece of code is using `lazyRequireGetter` that
is unknown to Webpack.

```
loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/shared/event-emitter");
```

In order to properly bundle `devtools/shared/event-emitter` module
the code needs to be translated into:

```
let EventEmitter = require("devtools/shared/event-emitter");
```

See more in `rewrite-lazy-require`
