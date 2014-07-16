# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

LOOP_SERVER_URL := $(shell echo $${LOOP_SERVER_URL-http://localhost:5000})
LOOP_PENDING_CALL_TIMEOUT := $(shell echo $${LOOP_PENDING_CALL_TIMEOUT-20000})
NODE_LOCAL_BIN=./node_modules/.bin

install:
	@npm install

test:
	@echo "Not implemented yet."

lint:
	@$(NODE_LOCAL_BIN)/jshint *.js content test

runserver: config
	@node server.js

frontend:
	@echo "Not implemented yet."

config:
	@echo "var loop = loop || {};" > content/config.js
	@echo "loop.config = loop.config || {};" >> content/config.js
	@echo "loop.config.serverUrl          = '`echo $(LOOP_SERVER_URL)`';" >> content/config.js
	@echo "loop.config.pendingCallTimeout = `echo $(LOOP_PENDING_CALL_TIMEOUT)`;" >> content/config.js
