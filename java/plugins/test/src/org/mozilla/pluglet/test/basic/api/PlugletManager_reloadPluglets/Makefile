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


TEST_NAME=PlugletManager_reloadPluglets
DEPTH=../../../../../../../..
#DEPTH1 used to build in the subdirectories, e.g SecondPluglet and deleteApplet
DEPTH1=../../../../../../../../..
UTIL_DIR=$(DEPTH)/utils
BUILD_DIR=$(DEPTH)/build
CONFIG_FILE=$(DEPTH)/config/CommonProperties
BASIC_PATH=test/basic/api
CLASSPATH=$(MOZILLA_FIVE_HOME)/../classes:$(DEPTH1)/build/classes
DEST_DIR=$(BUILD_DIR)/$(BASIC_PATH)/$(TEST_NAME)
MAKE_PARAM=perl ../makeParameters.pl
MAKE_COMB=perl ../makeCombinations.pl

default: all
all: second_pluglet delApplet html properties parameters data_files
html:
	@mkdir -p $(HTML_DEST_DIR)/$(TEST_NAME)
	@cp -f *.html $(HTML_DEST_DIR)/$(TEST_NAME) 
	@cp -f deleteApplet/*.html $(HTML_DEST_DIR)/$(TEST_NAME) 

second_pluglet: 
	@cd SecondPluglet; \
	$(JDKHOME)/bin/javac  -classpath $(CLASSPATH) *.java; \
	$(JDKHOME)/bin/jar cvfm SecondPluglet.jar manifest *.class
delApplet:
	@cd deleteApplet; \
	$(JDKHOME)/bin/javac  -classpath $(CLASSPATH) DelApp.java
properties:
	@mkdir -p $(DEST_DIR)
	@perl $(UTIL_DIR)/configurator.pl $(CONFIG_FILE) $(DEST_DIR)/TestProperties TestProperties
#We need to do it for rm -feteApplet
	@cp -f TestProperties $(HTML_DEST_DIR)/$(TEST_NAME) 
parameters:
	@cp -f $(TEST_NAME).lst $(DEST_DIR) 
combinations:
	@$(MAKE_COMB)  ParamCombinations $(TEST_NAME).lst $(BUILD_DIR)/$(COMMON_LST_FILE)
#Copy second pluglet jar in parameters dir
#Note, that this jar file will be copied in $PLUGLET dir by test pluglet before execution
#of reload pluglets. 
data_files:
	@cp -f SecondPluglet/SecondPluglet.jar $(DEST_DIR) 
	@cp -f deleteApplet/DelApp.class  $(HTML_DEST_DIR)/$(TEST_NAME) 
clobber:
	@if [ -e $(HTML_DEST_DIR)/$(TEST_NAME) ]; then \
		rm -f $(HTML_DEST_DIR)/$(TEST_NAME)/* ; \
	fi
	@if [ -e $(DEST_DIR) ]; then \
		rm -f $(DEST_DIR)/* ; \
	fi
	@rm -f SecondPluglet/SecondPluglet.jar
	@rm -f SecondPluglet/*.class
