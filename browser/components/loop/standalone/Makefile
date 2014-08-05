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

runserver: remove_old_config
	node server.js

frontend:
	@echo "Not implemented yet."

# Try hg first, if not fall back to git.
SOURCE_STAMP := $(shell hg parent --template '{node|short}\n' 2> /dev/null)
ifndef SOURCE_STAMP
SOURCE_STAMP := $(shell git describe --always --tag)
endif

SOURCE_DATE := $(shell hg parent --template '{date|date}\n' 2> /dev/null)
ifndef SOURCE_DATE
SOURCE_DATE := $(shell git log -1 --format="%H%n%aD")
endif

version:
	@echo $(SOURCE_STAMP) > content/VERSION.txt
	@echo $(SOURCE_DATE) >> content/VERSION.txt


# The local node server used for client dev (server.js) used to use a static
# content/config.js.  Now that information is server up dynamically.  This
# target is depended on by runserver, and removes any copies of that to avoid
# confusion.
remove_old_config:
	@rm -f content/config.js


# The services development deployment, however, still wants a static config
# file, and needs an easy way to generate one.  This target is for folks
# working with that deployment.
.PHONY: config
config:
	@echo "var loop = loop || {};" > content/config.js
	@echo "loop.config = loop.config || {};" >> content/config.js
	@echo "loop.config.serverUrl          = '`echo $(LOOP_SERVER_URL)`';" >> content/config.js
	@echo "loop.config.pendingCallTimeout = `echo $(LOOP_PENDING_CALL_TIMEOUT)`;" >> content/config.js
