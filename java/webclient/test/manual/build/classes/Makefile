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


DEPTH=../..
SRC=$(DEPTH)/src
CONFIG=$(DEPTH)/config/
UTILS=$(DEPTH)/utils
BUILD_DIR=$(DEPTH)/build
BUILD_CLASSES=$(BUILD_DIR)/classes
JAVAC=javac
include ./java.list
include $(CONFIG)/CommonProperties


all: 
	@for i in $(JAVA_SRC);do \
		mkdir -p `dirname $(HTML_ROOT_DIR)/$${i}`; \
		$(JAVAC) -d `dirname $(HTML_ROOT_DIR)/$${i}` $(SRC)/classes/$${i}; \
	done










