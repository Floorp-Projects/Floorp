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


#cd $(TEST_TOP_DIR)/build ; 
DEPTH=..
BUILD_DIR=$(DEPTH)/build
SRC_DIR=$(DEPTH)/src
CLASSES_SRC=$(SRC_DIR)/classes
CLASSES_BUILD=$(BUILD_DIR)/classes
JAVAC=javac

include $(DEPTH)/config/CommonProperties

DIRS=classes test/basic

all: 
	MAKE_ARGS=$@ ;\
	$(MAKE)  dirs

clobber:
	$(MAKE)  dirs MAKE_ARGS=$@ 

dirs:	
	for i in $(DIRS);do \
	cd $${i}; $(MAKE) $(MAKE_ARGS); \
	cd $(TEST_TOP_DIR)/build ;\
	done

