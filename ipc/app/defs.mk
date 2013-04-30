# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifneq (android,$(MOZ_WIDGET_TOOLKIT))
MOZ_CHILD_PROCESS_NAME := plugin-container$(BIN_SUFFIX)
else
# We want to let Android unpack the file at install time, but it only does
# so if the file is named libsomething.so. The lib/ path is also required
# because the unpacked file will be under the lib/ subdirectory and will
# need to be executed from that path.
MOZ_CHILD_PROCESS_NAME := lib/libplugin-container.so
endif
MOZ_CHILD_PROCESS_BUNDLE := plugin-container.app/Contents/MacOS/
