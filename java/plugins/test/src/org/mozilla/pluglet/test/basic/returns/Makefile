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

all:  classes tests

classes:
	@echo Building .basic.returns classes ...
	@$(JAVAC) -d $(BUILD_DIR_CLASSES) *.java 
	@echo done


TESTS= \
	Pluglet_createPlugletInstance \
	PlugletInstance_newStream 

#mkdir -p $(HTML_ROOT_DIR)/basic/returns/$${i}; 
#cp -f $${i}/*.html $(HTML_ROOT_DIR)/basic/returns/$${i}; 
tests: 
	@mkdir -p $(HTML_ROOT_DIR)/basic/returns
	@cp -f *.html $(HTML_ROOT_DIR)/basic/returns
	@for i in $(TESTS); do \
		mkdir -p $(BUILD_DIR)/test/basic/returns/$${i}; \
		perl $(UTIL_DIR)/configurator.pl $(COMMON_CONFIG_FILE) $(BUILD_DIR)/test/basic/returns/$${i}/TestProperties $${i}/TestProperties; \
		mkdir -p $(BUILD_DIR_CLASSES)/$(BASIC_PATH)/returns/$${i}; \
		cp -f manifest $(BUILD_DIR_CLASSES)/$(BASIC_PATH)/returns/$${i}; \
		cd $(BUILD_DIR_CLASSES); \
		$(JDKHOME)/bin/jar cfm $(BASIC_PATH)/returns/$${i}/test.jar $(BASIC_PATH)/returns/$${i}/manifest $(BASIC_PATH)/*.class $(BASIC_PATH)/returns/$${i}.class; \
		cd ../../src/$(BASIC_PATH)/returns/; \
	done

clobber:
	@if [ -e $(BUILD_DIR_CLASSES)/$(BASIC_PATH)/returns ]; then \
		rm -rf $(BUILD_DIR_CLASSES)/$(BASIC_PATH)/returns/* ; \
	fi
	@if [ -e $(HTML_ROOT_DIR)/basic/returns ]; then \
		rm -rf $(HTML_ROOT_DIR)/basic/returns/*; \
	fi
	@if [ -e $(BUILD_DIR)/test/basic/returns ]; then \
		rm -rf $(BUILD_DIR)/test/basic/returns/* ; \
	fi
