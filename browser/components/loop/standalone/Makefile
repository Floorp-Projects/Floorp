# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Note that this Makefile is not invoked by the Mozilla build
# system, which is why it can have dependencies on things the
# build system at-large doesn't yet support.

# XXX In the interest of making the build logic simpler and
# more maintainable, we should be trying to implement new
# functionality in Gruntfile.js rather than here.
# Bug 1066176 tracks moving all functionality currently here
# to the Gruntfile and getting rid of this Makefile entirely.

LOOP_SERVER_URL := $(shell echo $${LOOP_SERVER_URL-http://localhost:5000/v0})
LOOP_FEEDBACK_API_URL := $(shell echo $${LOOP_FEEDBACK_API_URL-"https://input.allizom.org/api/v1/feedback"})
LOOP_FEEDBACK_PRODUCT_NAME := $(shell echo $${LOOP_FEEDBACK_PRODUCT_NAME-Loop})
LOOP_BRAND_WEBSITE_URL := $(shell echo $${LOOP_BRAND_WEBSITE_URL-"https://www.mozilla.org/firefox/"})
LOOP_PRIVACY_WEBSITE_URL := $(shell echo $${LOOP_PRIVACY_WEBSITE_URL-"https://www.mozilla.org/privacy/firefox-hello/"})
LOOP_LEGAL_WEBSITE_URL := $(shell echo $${LOOP_LEGAL_WEBSITE_URL-"https://www.mozilla.org/about/legal/terms/firefox-hello/"})
LOOP_PRODUCT_HOMEPAGE_URL := $(shell echo $${LOOP_PRODUCT_HOMEPAGE_URL-"https://www.firefox.com/hello/"})

NODE_LOCAL_BIN=./node_modules/.bin

install: npm_install tos

npm_install:
	@npm install

test:
	@echo "Not implemented yet."

tos:
	@$(NODE_LOCAL_BIN)/grunt replace marked
	@$(NODE_LOCAL_BIN)/grunt sass

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
	@echo "loop.config.serverUrl = '`echo $(LOOP_SERVER_URL)`';" >> content/config.js
	@echo "loop.config.feedbackApiUrl = '`echo $(LOOP_FEEDBACK_API_URL)`';" >> content/config.js
	@echo "loop.config.feedbackProductName = '`echo $(LOOP_FEEDBACK_PRODUCT_NAME)`';" >> content/config.js
	@echo "loop.config.brandWebsiteUrl = '`echo $(LOOP_BRAND_WEBSITE_URL)`';" >> content/config.js
	@echo "loop.config.privacyWebsiteUrl = '`echo $(LOOP_PRIVACY_WEBSITE_URL)`';" >> content/config.js
	@echo "loop.config.legalWebsiteUrl = '`echo $(LOOP_LEGAL_WEBSITE_URL)`';" >> content/config.js
	@echo "loop.config.learnMoreUrl = '`echo $(LOOP_PRODUCT_HOMEPAGE_URL)`';" >> content/config.js
	@echo "loop.config.fxosApp = loop.config.fxosApp || {};" >> content/config.js
	@echo "loop.config.fxosApp.name = 'Loop';" >> content/config.js
	@echo "loop.config.fxosApp.rooms = true;" >> content/config.js
	@echo "loop.config.fxosApp.manifestUrl = 'http://fake-market.herokuapp.com/apps/packagedApp/manifest.webapp';" >> content/config.js
	@echo "loop.config.roomsSupportUrl = 'https://support.mozilla.org/kb/group-conversations-firefox-hello-webrtc';" >> content/config.js
	@echo "loop.config.guestSupportUrl = 'https://support.mozilla.org/kb/respond-firefox-hello-invitation-guest-mode';" >> content/config.js
	@echo "loop.config.generalSupportUrl = 'https://support.mozilla.org/kb/respond-firefox-hello-invitation-guest-mode';" >> content/config.js
	@echo "loop.config.unsupportedPlatformUrl = 'https://support.mozilla.org/en-US/kb/which-browsers-will-work-firefox-hello-video-chat';" >> content/config.js
