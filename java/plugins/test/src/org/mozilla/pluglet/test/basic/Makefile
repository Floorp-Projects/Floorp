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


DEPTH=../../../../../..
BUILD_DIR=$(DEPTH)/build
BUILD_DIR_CLASSES=$(BUILD_DIR)/classes
JAVAC=javac
BASIC_PATH=org/mozilla/pluglet/test/basic
CLASSPATH=$(MOZILLA_FIVE_HOME)/../classes:$(BUILD_DIR_CLASSES)

TEST_DIRS=api params security returns scenario

all: basic tests

tests:
	@if [ -z $(HTML_ROOT_DIR) ] ; then \
		echo "======================================"; \
		echo "Error. You MUST specify \$HTML_ROOT_DIR"; \
		echo "variable for make in test dirs."; \
		echo "======================================"; \
		exit -1; \
	fi
	@for i in $(TEST_DIRS) ;do \
		cd $$i;$(MAKE) $(MAKE_ARGS); cd ..;\
	done

basic: classes 	TestLoaderPluglet.jar

classes:
	@echo "Building .basic classes ..."
	@$(JAVAC) -d $(BUILD_DIR_CLASSES) *.java 
	@echo done
#$(JDKHOME)/bin/jar cfm 

TestLoaderPluglet.jar: classes add_files
	@cd $(BUILD_DIR_CLASSES);$(JDKHOME)/bin/jar cfm \
		    $(BASIC_PATH)/TestLoaderPluglet.jar \
				 $(BASIC_PATH)/manifest \
	                         $(BASIC_PATH)/*.class 
add_files:
	@cp -f manifest $(BUILD_DIR_CLASSES)/$(BASIC_PATH)

clobber:
	@cd $(BUILD_DIR_CLASSES)/$(BASIC_PATH);rm -f *.class *.jar manifest
	@$(MAKE) tests MAKE_ARGS=$@
