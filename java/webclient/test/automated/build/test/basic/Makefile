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



DEPTH=../../..
BUILD_DIR=$(DEPTH)/build
SRC_DIR=$(DEPTH)/src
UTIL_DIR=$(DEPTH)/utils
COMMON_PROPERTIES=$(DEPTH)/config/CommonProperties
TEST_DIRS_SRC=$(SRC_DIR)/test/basic
TEST_DIRS_BUILD=$(BUILD_DIR)/test/basic
PERL=perl
TESTS_SUFFIX=test/basic
SECTIONS  = api \
	    mixed \
	    stress \
	    $(NULL)
include $(COMMON_PROPERTIES)

all: tests


test_dirs:
	@for i in `cat .$(SECTION).dirs`;do \
		echo Proceed test properties in  $(TEST_DIRS_SRC)/$(SECTION)/$${i} ... ;\
		$(PERL) $(UTIL_DIR)/configurator.pl $(COMMON_PROPERTIES) \
						     $(TEST_DIRS_BUILD)/$(SECTION)/$${i}/TestProperties \
						     $(TEST_DIRS_SRC)/$(SECTION)/$${i}/TestProperties; \
		if [ -f $(TEST_DIRS_SRC)/$(SECTION)/$${i}/Test.lst  ] ;then \
			echo Proceed test arguments in  $(TEST_DIRS_SRC)/$(SECTION)/$${i} ...; \
			$(PERL) $(UTIL_DIR)/configurator.pl $(COMMON_PROPERTIES) \
							     $(TEST_DIRS_BUILD)/$(SECTION)/$${i}/Test.lst \
							     $(TEST_DIRS_SRC)/$(SECTION)/$${i}/Test.lst ;\
		fi ;\
	        if [ -f  $(TEST_DIRS_SRC)/$(SECTION)/$${i}/*.html ] ;then \
	                echo Copy HTML files from $(TEST_DIRS_SRC)/$(SECTION)/$${i} ;\
			mkdir -p $(HTML_ROOT_DIR)/$(TESTS_SUFFIX)/$(SECTION)/$${i} ;\
		 	cp -f $(TEST_DIRS_SRC)/$(SECTION)/$${i}/*.html $(HTML_ROOT_DIR)/$(TESTS_SUFFIX)/$(SECTION)/$${i} ;\
			mkdir -p $(HTML_ROOT_DIR)/$(TESTS_SUFFIX)/$(SECTION)/$${i} ;\
		 	cp -f $(TEST_DIRS_SRC)/$(SECTION)/$${i}/*.html $(HTML_ROOT_DIR)/$(TESTS_SUFFIX)/$(SECTION)/$${i} ;\
		fi ;\
	        if [ -f $(TEST_DIRS_SRC)/$(SECTION)/$${i}/*.cgi ] ;then \
			mkdir -p $(CGI_BIN_ROOT_DIR)/$(TESTS_SUFFIX)/$(SECTION)/$${i} ;\
			for j in $(TEST_DIRS_SRC)/$(SECTION)$${i}/*.cgi; do \
				CGI_NAME=`basename $${j}`; \
	 			$(PERL) $(UTIL_DIR)/configurator.pl $(COMMON_PROPERTIES) \
					$(CGI_BIN_ROOT_DIR)/$(TESTS_SUFFIX)/$(SECTION)/$${i}/$${CGI_NAME} \
					$(TEST_DIRS_SRC)/$(SECTION)/$${i}/$${CGI_NAME}; \
			done ;\
		fi ;\
	done
clobber:
	rm -f $(BUILD_DIR)/run/*
	for i in $(DIRS);do \
		echo Make clobber in $(TEST_DIRS_BUILD)/$(SECTION)/$${i} ;\
		rm -rf  $(TEST_DIRS_BUILD)/$(SECTION)/$${i}/* ;\
	done

sections:
	@for i in $(SECTIONS); do \
		mkdir -p $(HTML_ROOT_DIR)/$(TESTS_SUFFIX)/$${i} ;\
		if [ -f $(TEST_DIRS_SRC)/$${i}/*.html ] ;then \
			echo Copy HTML files from $(TEST_DIRS_SRC)/$${i} ;\
			cp -f  $(TEST_DIRS_SRC)/$${i}/*.html $(HTML_ROOT_DIR)/$(TESTS_SUFFIX)/$${i} ;\
		fi ;\
		mkdir -p $(CGI_BIN_ROOT_DIR)/$(TESTS_SUFFIX)/$${i} ;\
		if [ -f $(TEST_DIRS_SRC)/$${i}/*.cgi ] ;then \
			echo Copy CGI files from $(TEST_DIRS_SRC)/$${i} ;\
			for j in $(TEST_DIRS_SRC)/$${i}/*.cgi; do \
				CGI_NAME=`basename $${j}`; \
	 			$(PERL) $(UTIL_DIR)/configurator.pl $(COMMON_PROPERTIES) \
					$(CGI_BIN_ROOT_DIR)/$(TESTS_SUFFIX)/$${i}/$${CGI_NAME} \
					$(TEST_DIRS_SRC)/$${i}/$${CGI_NAME}; \
			done ;\
		fi ;\
		echo Generate list of $${i} tests ;\
		$(PERL) $(UTIL_DIR)/dirGen.pl  $(TEST_DIRS_SRC)/$${i} .$${i}.dirs ;\
		$(MAKE) test_dirs SECTION=$${i};\
		$(PERL) $(UTIL_DIR)/lstGen.pl $(BUILD_DIR)/$(TESTS_SUFFIX)/$${i} $(BUILD_DIR)/run/$${i}.lst ;\
	done
tests:  
	@mkdir -p $(BUILD_DIR)/run
	@$(PERL) $(UTIL_DIR)/configurator.pl $(COMMON_PROPERTIES) \
						$(BUILD_DIR)/run/autorun.pl\
						$(SRC_DIR)/run/autorun.pl
	@$(PERL) $(UTIL_DIR)/configurator.pl $(COMMON_PROPERTIES) \
						$(BUILD_DIR)/run/forceful.pl\
						$(SRC_DIR)/run/forceful.pl
	@cp -f $(SRC_DIR)/run/BWTest.lst $(BUILD_DIR)/run
	@cp -f $(SRC_DIR)/run/BWMixedTest.lst $(BUILD_DIR)/run
	@echo Copy HTML files from $(TEST_DIRS_SRC)
	@mkdir -p $(HTML_ROOT_DIR)/$(TESTS_SUFFIX)
	@cp -f $(TEST_DIRS_SRC)/*.html $(HTML_ROOT_DIR)/$(TESTS_SUFFIX)
	@mkdir -p $(CGI_BIN_ROOT_DIR)/$(TESTS_SUFFIX)
	@for i in $(TEST_DIRS_SRC)/*.cgi; do \
		CGI_NAME=`basename $${i}`; \
	 	$(PERL) $(UTIL_DIR)/configurator.pl $(COMMON_PROPERTIES) \
			$(CGI_BIN_ROOT_DIR)/$(TESTS_SUFFIX)/$${CGI_NAME} \
			$(TEST_DIRS_SRC)/$${CGI_NAME}; \
	done
	@$(MAKE) sections











