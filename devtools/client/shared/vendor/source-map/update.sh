#!/bin/bash

# Helper to update the vendoring of:
# https://github.com/mozilla/source-map
# in the current folder.

REPO=$1
if [[ ! -d $REPO ]]; then
  echo "Usage: $0 PATH_TO_SOURCE_MAP_REPO"
  echo "'$REPO' isn't a directory" 
  echo "It should be a path to a local checkout of:"
  echo "https://github.com/mozilla/source-map"
  exit
fi

cp $REPO/source-map.js source-map.js
cp $REPO/lib/*.js lib/
cp $REPO/lib/*.wasm lib/

# For a couple of files, we have to pick the browser version
# (instead of node version)
cp lib/read-wasm-browser.js lib/read-wasm.js
cp lib/url-browser.js lib/url.js
rm lib/read-wasm-browser.js lib/url-browser.js

echo "Warning: lib/read-wasm.js has been forked in mozilla-central to support running in both Firefox and Jest/Node"
echo "You may want to review the difference and mostly revert to mozilla-central revision"
echo ""

# In the following module, we have to move to a relative URL
# instead of the global require("whatwg-url").
sed -i "s#whatwg-url#../../whatwg-url.js#" lib/url.js

# Record the git changeset so that we ease tracking
# what version we are currently using
git -C $REPO rev-parse HEAD > GITHUB_CHANGESET

echo "source-map synchronization completed"
