# 
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
# The Original Code is The Waterfall Java Plugin Module
#  
# The Initial Developer of the Original Code is Sun Microsystems Inc
# Portions created by Sun Microsystems Inc are Copyright (C) 2001
# All Rights Reserved.
# 
# $Id: rules.mk,v 1.2 2001/07/12 19:57:34 edburns%acm.org Exp $
# 
# Contributor(s):
# 
#     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
# 

# nmake isn't the best make program I know about
FILES_class = $(FILES_class:.java=.class)
FILES_class=$(FILES_class:/=\)
PKG1_DIR=$(PKG:.=\)
PKG2_DIR=$(PKG2:.=\)
PKG3_DIR=$(PKG3:.=\)
PKG4_DIR=$(PKG4:.=\)
PKG5_DIR=$(PKG5:.=\)
PKG6_DIR=$(PKG6:.=\)
PKG7_DIR=$(PKG7:.=\)
PKG8_DIR=$(PKG8:.=\)
PKG9_DIR=$(PKG9:.=\)
PKG10_DIR=$(PKG10:.=\)
PKG11_DIR=$(PKG11:.=\)
PKG12_DIR=$(PKG12:.=\)
PKG13_DIR=$(PKG13:.=\)
PKG14_DIR=$(PKG14:.=\)
PKG15_DIR=$(PKG15:.=\)
PKG16_DIR=$(PKG16:.=\)
PKG17_DIR=$(PKG17:.=\)
PKG18_DIR=$(PKG18:.=\)

{$(JAVASRCDIR)\$(PKG1_DIR)\}.java{$(CLASSDESTDIR)\$(PKG1_DIR)\}.class:
	@echo $(?) >>.classes.list
{$(JAVASRCDIR)\$(PKG2_DIR)\}.java{$(CLASSDESTDIR)\$(PKG2_DIR)\}.class:
	@echo $(?) >>.classes.list
{$(JAVASRCDIR)\$(PKG3_DIR)\}.java{$(CLASSDESTDIR)\$(PKG3_DIR)\}.class:
	@echo $(?) >>.classes.list
{$(JAVASRCDIR)\$(PKG4_DIR)\}.java{$(CLASSDESTDIR)\$(PKG4_DIR)\}.class:
	@echo $(?) >>.classes.list
{$(JAVASRCDIR)\$(PKG5_DIR)\}.java{$(CLASSDESTDIR)\$(PKG5_DIR)\}.class:
	@echo $(?) >>.classes.list
{$(JAVASRCDIR)\$(PKG6_DIR)\}.java{$(CLASSDESTDIR)\$(PKG6_DIR)\}.class:
	@echo $(?) >>.classes.list
{$(JAVASRCDIR)\$(PKG7_DIR)\}.java{$(CLASSDESTDIR)\$(PKG7_DIR)\}.class:
	@echo $(?) >>.classes.list
{$(JAVASRCDIR)\$(PKG8_DIR)\}.java{$(CLASSDESTDIR)\$(PKG8_DIR)\}.class:
	@echo $(?) >>.classes.list
{$(JAVASRCDIR)\$(PKG9_DIR)\}.java{$(CLASSDESTDIR)\$(PKG9_DIR)\}.class:
	@echo $(?) >>.classes.list
{$(JAVASRCDIR)\$(PKG10_DIR)\}.java{$(CLASSDESTDIR)\$(PKG10_DIR)\}.class:
	@echo $(?) >>.classes.list
{$(JAVASRCDIR)\$(PKG11_DIR)\}.java{$(CLASSDESTDIR)\$(PKG11_DIR)\}.class:
	@echo $(?) >>.classes.list
{$(JAVASRCDIR)\$(PKG12_DIR)\}.java{$(CLASSDESTDIR)\$(PKG12_DIR)\}.class:
	@echo $(?) >>.classes.list
{$(JAVASRCDIR)\$(PKG13_DIR)\}.java{$(CLASSDESTDIR)\$(PKG13_DIR)\}.class:
    @echo $(?) >>.classes.list
{$(JAVASRCDIR)\$(PKG14_DIR)\}.java{$(CLASSDESTDIR)\$(PKG14_DIR)\}.class:
    @echo $(?) >>.classes.list
{$(JAVASRCDIR)\$(PKG15_DIR)\}.java{$(CLASSDESTDIR)\$(PKG15_DIR)\}.class:
    @echo $(?) >>.classes.list
{$(JAVASRCDIR)\$(PKG16_DIR)\}.java{$(CLASSDESTDIR)\$(PKG16_DIR)\}.class:
    @echo $(?) >>.classes.list
{$(JAVASRCDIR)\$(PKG17_DIR)\}.java{$(CLASSDESTDIR)\$(PKG17_DIR)\}.class:
    @echo $(?) >>.classes.list
{$(JAVASRCDIR)\$(PKG18_DIR)\}.java{$(CLASSDESTDIR)\$(PKG18_DIR)\}.class:
    @echo $(?) >>.classes.list


.SUFFIXES:
.SUFFIXES: .java .class

classes: $(CLASSES_INIT) $(CLASSDESTDIR) delete.classlist $(FILES_class) compile.classlist

$(CLASSDESTDIR):
	@mkdir $@

delete.classlist:	
	@if exist .classes.list $(DEL) .classes.list

compile.classlist:
	@if exist .classes.list echo "Compiling Java classes"
	@if exist .classes.list type .classes.list 
	@if exist .classes.list $(JAVAC_CMD) -d $(CLASSDESTDIR) -classpath $(CLASSDESTDIR) -classpath $(ADDCLASSPATH) \
	@.classes.list

clean:
	@ echo Remove classfiles from $(CLASSDESTDIR)
	@ IF EXIST $(CLASSDESTDIR) $(DELTREE) $(CLASSDESTDIR)
	@ IF EXIST .classes.list $(DEL) .classes.list

.PHONY: delete.classlist compile.classlist classes
