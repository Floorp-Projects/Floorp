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


DEPTH=..
SRC=$(DEPTH)/src
CONFIG=$(DEPTH)/config/
UTILS=$(DEPTH)/utils
BUILD_DIR=$(DEPTH)/build
CLASSES=$(BUILD_DIR)/classes
include html.list
include $(CONFIG)/CommonProperties

all: html cgi lst 

html: images
	@echo Proceed HTML files ...
	@for i in $(HTML_LIST) ;do \
		perl $(UTILS)/configurator.pl $(CONFIG)/CommonProperties $(HTML_ROOT_DIR)/$${i} $(SRC)/html/$${i} ; \
	done
	@echo Creating chkbox group in $(START_HTML) ...
	@perl $(UTILS)/lst_gen.pl $(HTML_ROOT_DIR)/$(START_HTML) $(CONFIG)/$(LST_FILE)

images:
	@echo Copy images ...
	@mkdir -p $(HTML_ROOT_DIR)/log
	@mkdir -p $(HTML_ROOT_DIR)/images
	@cp -f $(SRC)/images/*.gif  $(HTML_ROOT_DIR)/images

cgi: 
	@echo Proceed CGI scripts ...
	@perl $(UTILS)/configurator.pl $(CONFIG)/CommonProperties $(CGI_BIN_ROOT_DIR)/results.cgi $(SRC)/cgi/results.cgi
	@chmod a+x $(CGI_BIN_ROOT_DIR)/results.cgi
	@perl $(UTILS)/configurator.pl $(CONFIG)/CommonProperties $(CGI_BIN_ROOT_DIR)/start.cgi $(SRC)/cgi/start.cgi
	@chmod a+x $(CGI_BIN_ROOT_DIR)/start.cgi
	@perl $(UTILS)/configurator.pl $(CONFIG)/CommonProperties $(CGI_BIN_ROOT_DIR)/finish.cgi $(SRC)/cgi/finish.cgi
	@chmod a+x $(CGI_BIN_ROOT_DIR)/finish.cgi
	@perl $(UTILS)/configurator.pl $(CONFIG)/CommonProperties $(CGI_BIN_ROOT_DIR)/manager.cgi $(SRC)/cgi/manager.cgi
	@chmod a+x $(CGI_BIN_ROOT_DIR)/manager.cgi
	@echo Creating chkbox group in start.cgi ...
	@perl $(UTILS)/lst_gen.pl $(CGI_BIN_ROOT_DIR)/start.cgi $(CONFIG)/$(LST_FILE)

lst:    
	@echo Copy list file ...
	@cp -f $(CONFIG)/WebClientTests.lst $(CGI_BIN_ROOT_DIR)







