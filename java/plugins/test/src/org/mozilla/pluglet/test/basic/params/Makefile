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
CLASSPATH=$(MOZILLA_FIVE_HOME)/../classes:$(BUILD_DIR_CLASSES):$$(CLASSPATH)
HTML_ROOT_DIR=/set/Mozilla/avm/pl_sol/build/htroot
#$$(HTML_ROOT_DIR)

all:  classes tests_common tests_onda

classes:
	@echo Building .basic.params classes ...
	@$(JAVAC) -d $(BUILD_DIR_CLASSES) *.java 
	@echo done

ONDA_TESTS= \
	PlugletStreamListener_onDataAvailable_input_1 \
	PlugletStreamListener_onDataAvailable_input_2 \
	PlugletStreamListener_onDataAvailable_input_3 \
	PlugletStreamListener_onDataAvailable_input_4 \
	PlugletStreamListener_onDataAvailable_input_5 \
	PlugletStreamListener_onDataAvailable_input_6 \
	PlugletStreamListener_onDataAvailable_input_7 \
	PlugletStreamListener_onDataAvailable_input_8 \
	PlugletStreamListener_onDataAvailable_input_9 \
	PlugletStreamListener_onDataAvailable_input_11 \
	PlugletStreamListener_onDataAvailable_input_12 

MIME_TESTS= \
	Pluglet_createPlugletInstance_mimeType_1 \
	Pluglet_createPlugletInstance_mimeType_2 \
	Pluglet_createPlugletInstance_mimeType_3 \
	Pluglet_createPlugletInstance_mimeType_4 \
	Pluglet_createPlugletInstance_mimeType_5 \
	Pluglet_createPlugletInstance_mimeType_6 

#       Pluglet_createPlugletInstance_mimeType_7 

clobber:
	@cd $(BUILD_DIR_CLASSES)/$(BASIC_PATH)/params/;rm -rf *
	@cd $(HTML_ROOT_DIR)/basic/params/;rm -rf *
	@cd $(BUILD_DIR)/test/basic/params/;rm -rf *
tests_common:
	@echo Performs common procedures for all PARAMS tests ...
	#@mkdir -p $(HTML_ROOT_DIR)/basic/params          #COMMENTED ...
	#@cp -f *.html $(HTML_ROOT_DIR)/basic/params; 	  # 
	@for i in $(MIME_TESTS) $(ONDA_TESTS) ; do \
		echo Making test $${i}; \
		mkdir -p $(BUILD_DIR)/test/basic/params/$${i}; \
		perl $(UTIL_DIR)/configurator.pl $(COMMON_CONFIG_FILE) $(BUILD_DIR)/test/basic/params/$${i}/TestProperties $${i}/TestProperties; \
		mkdir -p $(HTML_ROOT_DIR)/basic/params/$${i}; \
		mkdir -p $(BUILD_DIR_CLASSES)/$(BASIC_PATH)/params/$${i}; \
		if [ -e $${i}/*.html ] ; then \
			cp  -f $${i}/*.html $(HTML_ROOT_DIR)/basic/params/$${i}; \
		fi; \
		cp -f $${i}/manifest $(BUILD_DIR_CLASSES)/$(BASIC_PATH)/params/$${i}; \
		cd $(BUILD_DIR_CLASSES); \
		$(JDKHOME)/bin/jar cfm $(BASIC_PATH)/params/$${i}/test.jar $(BASIC_PATH)/params/$${i}/manifest $(BASIC_PATH)/*.class $(BASIC_PATH)/params/*.class; \
		cd ../../src/$(BASIC_PATH)/params/; \
	done
#Additional procedures
#Pluglet_createPlugletInstance_mimeType_6
	@cp -f Pluglet_createPlugletInstance_mimeType_6/test.zip $(HTML_ROOT_DIR)/basic/params/Pluglet_createPlugletInstance_mimeType_6  

#Additional procedures for tests on onDataAvailable
tests_onda:
	@echo  Copying test.tdt ... 
	@for i in $(ONDA_TESTS) ;do \
		cp -f $${i}/test.tdt $(HTML_ROOT_DIR)/basic/params/$${i}; \
		cp -f $${i}/test.tdt $(BUILD_DIR)/test/basic/params/$${i}; \
	done














