#!/bin/bash

# Script to easily create a new command, including:
# - a template for the main command file
# - test folder and test head.js file
# - a template for a first test
# - all necessary build manifests

if [[ -z $1 || -z $2 ]]; then
  echo "$0 expects two arguments:"
  echo "$(basename $0) command-file-name CommandName"
  echo " 1) The file name for the command, with '-' as separators between words"
  echo "    This will be the name of the folder"
  echo " 2) The command name being caml cased"
  echo "    This will be used to craft the name of the JavaScript class"
  exit
fi

if [ -e $1 ]; then
  echo "$1 already exists. Please use a new folder/command name."
fi

CMD_FOLDER=$1
CMD_FILE_NAME=$1-command.js
CMD_NAME=$2Command

pushd `dirname $0`

echo "Creating a new command called '$CMD_NAME' in $CMD_FOLDER"

mkdir $CMD_FOLDER

cat > $CMD_FOLDER/moz.build <<EOF
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

DevToolsModules(
    "$CMD_FILE_NAME",
)

if CONFIG["MOZ_BUILD_APP"] != "mobile/android":
    BROWSER_CHROME_MANIFESTS += ["tests/browser.toml"]
EOF

cat > $CMD_FOLDER/$CMD_FILE_NAME <<EOF
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * The $CMD_NAME ...
 */
class $CMD_NAME {
  constructor({ commands, descriptorFront, watcherFront }) {
    this.#commands = commands;
  }
  #commands = null;

}

module.exports = $CMD_NAME;
EOF

mkdir $CMD_FOLDER/tests
cat > $CMD_FOLDER/tests/browser.toml <<EOF
[DEFAULT]
tags = "devtools"
subsuite = "devtools"
support-files = [
  "!/devtools/client/shared/test/shared-head.js",
  "head.js",
]

[browser_$1.js]
EOF


cat > $CMD_FOLDER/tests/head.js <<EOF
* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local"}] */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);
EOF

CMD_NAME_FIRST_LOWERCASE=${CMD_NAME,}
cat > $CMD_FOLDER/tests/browser_$1.js <<EOF
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the $CMD_NAME

add_task(async function () {
  info("Setup the test page");
  const tab = await addTab("data:text/html;charset=utf-8,Test page");

  info("Create a target list for a tab target");
  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();

  const { $CMD_NAME_FIRST_LOWERCASE } = commands;

  // assertions...

  await commands.destroy();
  BrowserTestUtils.removeTab(tab);
});
EOF

popd

echo ""
echo "Command created!"
echo ""
echo "Now:"
echo " - edit moz.build to add '\"$CMD_FOLDER\",' in DIRS (this need to be kept sorted)"
echo " - edit index.js to add '$CMD_NAME_FIRST_LOWERCASE: \"devtools/shared/commands/$CMD_FOLDER/$1-command\"' in Commands dictionary"
