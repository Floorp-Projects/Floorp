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

DEPTH			=../../../../../../..
PL_DEPTH		=../../../../../../../..
BUILD_DIR		=$(DEPTH)/build
UTIL_DIR		=$(DEPTH)/utils
BUILD_DIR_CLASSES	=$(BUILD_DIR)/classes
BASIC_PATH		=org/mozilla/pluglet/test/basic
SRC_PATH		=.
AUTOMATION_PATH 	=./automation
CONFIG_DIR    		=$(DEPTH)/config
TEMPLATE_FILE 		=$(CONFIG_DIR)/SecurityTests.template
KEY_LIST_FILE 		=$(CONFIG_DIR)/SecurityTests.keys
TARGET_FILE		=$(CONFIG_DIR)/SecurityTests.properties
CONFIG_SCRIPT 		=$(UTIL_DIR)/configurator.pl
TREE_GEN_SCRIPT		=tree-gen.pl
CLASSPATH		=$(BUILD_DIR_CLASSES)
#;$(CLASSPATH)
PERL			=perl
JAVAC			=javac

all: config common_html test_pluglet

#
# Call the configuration utility and prepare the .properties file
#
config:
	@echo Configuring properties ...
	@$(PERL) $(CONFIG_SCRIPT) $(KEY_LIST_FILE) $(TARGET_FILE) $(TEMPLATE_FILE)
	@cd util; \
	$(PERL) $(TREE_GEN_SCRIPT) $(PL_DEPTH)
common_html:
	@echo HTML_ROOT_DIR is "$(HTML_ROOT_DIR)"
	@mkdir -p $(HTML_ROOT_DIR)/security
	@cp -f bastest.html $(HTML_ROOT_DIR)/security
classes:
	@echo Building security tests ...
	@$(JAVAC) -classpath $(CLASSPATH) -d $(BUILD_DIR_CLASSES) $(SRC_PATH)/*.java
	@echo Building security automation tests ...
	@$(JAVAC) -classpath $(CLASSPATH) -d $(BUILD_DIR_CLASSES) $(AUTOMATION_PATH)/*.java
test_pluglet: classes
	@cp -f manifest $(BUILD_DIR_CLASSES)/$(BASIC_PATH)/security
	@cd $(BUILD_DIR_CLASSES); \
	$(JDKHOME)/bin/jar cfm $(BASIC_PATH)/security/TestSecurityPluglet.jar \
				 $(BASIC_PATH)/security/manifest $(BASIC_PATH)/*.class \
				 $(BASIC_PATH)/security/*.class $(BASIC_PATH)/security/automation/*.class
clobber:
	@if [ -e $(HTML_ROOT_DIR)/security ]; then \
		rm -rf $(HTML_ROOT_DIR)/security/*; \
	fi
	@if [ -e $(BUILD_DIR_CLASSES)/$(BASIC_PATH)/security ] ;then \
		rm -rf $(BUILD_DIR_CLASSES)/$(BASIC_PATH)/security/* ; \
	fi
	@if [ -e $(BUILD_DIR)/test/basic/security ]; then \
		rm -rf $(BUILD_DIR)/test/basic/security/*; \
	fi
