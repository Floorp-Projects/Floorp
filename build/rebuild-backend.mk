# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

BACKEND_GENERATION_SCRIPT ?= config.status

# A traditional rule would look like this:
#    backend.%:
#        @echo do stuff
#
# But with -j<n>, and multiple items in BUILD_BACKEND_FILES, the command would
# run multiple times in parallel.
#
# "Fortunately", make has some weird semantics for pattern rules: if there are
# multiple targets in a pattern rule and each of them is matched at most once,
# the command will only run once. So:
#     backend%RecursiveMakeBackend backend%FasterMakeBackend:
#         @echo do stuff
#     backend: backend.RecursiveMakeBackend backend.FasterMakeBackend
# would only execute the command once.
#
# Credit where due: http://stackoverflow.com/questions/2973445/gnu-makefile-rule-generating-a-few-targets-from-a-single-source-file/3077254#3077254
$(subst .,%,$(BUILD_BACKEND_FILES)):
	@echo 'Build configuration changed. Regenerating backend.'
	$(PYTHON) $(BACKEND_GENERATION_SCRIPT)

define build_backend_rule
$(1): $$(wildcard $$(shell cat $(1).in))

endef
$(foreach file,$(BUILD_BACKEND_FILES),$(eval $(call build_backend_rule,$(file))))
