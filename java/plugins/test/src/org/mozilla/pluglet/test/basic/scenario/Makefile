# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Sun Microsystems,
# Inc. Portions created by Sun are
# Copyright (C) 1999 Sun Microsystems, Inc. All
# Rights Reserved.
#
# Contributor(s):

DEPTH=../../../../../../..
COMMON_CONFIG_FILE=$(DEPTH)/config/CommonProperties
UTIL_DIR=$(DEPTH)/utils
BUILD_DIR=$(DEPTH)/build
BUILD_DIR_CLASSES=$(BUILD_DIR)/classes
JAVAC=javac
BASIC_PATH=org/mozilla/pluglet/test/basic
CLASSPATH=$(BUILD_DIR_CLASSES):$(MOZILLA_FIVE_HOME)/../classes
TESTS=1 2 3 4 5  8 9 fr1 fr2 fr3

all:  tests  TestSeqScenarioPluglet.jar

classes:
	@echo Building .basic.scenario classes ...
	@$(JAVAC) -d $(BUILD_DIR_CLASSES) *.java 
	@echo done

add_files:
	@cp -f manifest $(BUILD_DIR_CLASSES)/$(BASIC_PATH)/scenario

TestSeqScenarioPluglet.jar: classes add_files
	@cd $(BUILD_DIR_CLASSES); \
	$(JDKHOME)/bin/jar cfm $(BASIC_PATH)/scenario/TestSeqScenarioPluglet.jar $(BASIC_PATH)/scenario/manifest $(BASIC_PATH)/*.class $(BASIC_PATH)/scenario/*.class

clobber:
	@if [ -e $(BUILD_DIR_CLASSES)/$(BASIC_PATH)/scenario ]; then \
		rm -rf $(BUILD_DIR_CLASSES)/$(BASIC_PATH)/scenario/* ; \
	fi
	@if [ -e $(BUILD_DIR)/test/basic/scenario/ ]; then \
		rm -rf $(BUILD_DIR)/test/basic/scenario/*; \
	fi
	@if [ -e  $(HTML_ROOT_DIR)/basic/scenario ]; then \
		rm -rf $(HTML_ROOT_DIR)/basic/scenario/*; \
	fi
tests:
	@mkdir -p $(HTML_ROOT_DIR)/basic/scenario
	@for i in $(TESTS); do \
		cp -f *.html $(HTML_ROOT_DIR)/basic/scenario; \
		cp -f *.js $(HTML_ROOT_DIR)/basic/scenario; \
		mkdir -p $(BUILD_DIR)/test/basic/scenario/$${i}; \
		mkdir -p $(HTML_ROOT_DIR)/basic/scenario/$${i}; \
		perl $(UTIL_DIR)/configurator.pl $(COMMON_CONFIG_FILE) $(BUILD_DIR)/test/basic/scenario/$${i}/TestProperties $${i}/TestProperties; \
		if [ -e $${i}/*.html ]; then \
			cp -f $${i}/*.html $(HTML_ROOT_DIR)/basic/scenario/$${i}; \
		fi; \
		if [ -e $${i}/*.js ]; then \
			cp -f $${i}/*.js $(HTML_ROOT_DIR)/basic/scenario/$${i}; \
		fi; \
	done