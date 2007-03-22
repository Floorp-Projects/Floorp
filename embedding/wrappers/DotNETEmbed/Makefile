# 
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Mozilla browser.
#
# The Initial Developer of the Original Code is
# Christopher Blizzard.  Portions created by Christopher Blizzard Copyright (C) 1999, Christopher Blizzard.  All Rights Reserved.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

ifndef MOZ_OBJ_DIR
MOZ_OBJ_DIR     = ${MOZ_SRC}/mozilla
endif

# Compiler variables (implicit rules are used to compile .cpp files)
CC		= cl
CFLAGS		= -nologo -Zi
CXX		= cl
CXXFLAGS	= -nologo -CLR -TP -Zi -wd4005 -we4700 -we4701 -c -DDEBUG -DXP_WIN -DXP_WIN32 ${INCLUDES}

OUTPUT_OPTION	= -Fo$@

# Linker variables (implicit rules can't use link.exe, so we do that manually)
LDFLAGS		= -nologo -dll -debug -libpath:${MOZ_OBJ_DIR}/dist/lib/ -NODEFAULTLIB:MSVCRTD

INCLUDES	= \
		  -I${MOZ_OBJ_DIR}/dist/include			\
		  -I${MOZ_OBJ_DIR}/dist/include/profile		\
		  -I${MOZ_OBJ_DIR}/dist/include/webbrwsr	\
		  -I${MOZ_OBJ_DIR}/dist/include/widget		\
		  -I${MOZ_OBJ_DIR}/dist/include/xpcom		\
		  -I${MOZ_OBJ_DIR}/dist/include/nspr		\
		  -I${MOZ_OBJ_DIR}/dist/include/necko		\
		  -I${MOZ_OBJ_DIR}/dist/include/docshell	\
		  -I${MOZ_OBJ_DIR}/dist/include/dom		\
		  -I${MOZ_OBJ_DIR}/dist/include/string		\
		  -I${MOZ_OBJ_DIR}/dist/include/embed_base	\
		  -I${MOZ_OBJ_DIR}/dist/include/windowwatcher	\
		  -I${MOZ_OBJ_DIR}/dist/include/xpconnect

DEPS		= DotNETEmbed.h Makefile

CPP_FILES	=  \
		  DotNETEmbed.cpp				\
		  DotNETProfileManager.cpp			\
		  DotNETNetworking.cpp				\
		  AssemblyInfo.cpp                

OBJ_FILES	= $(CPP_FILES:%.cpp=%.obj)

LIBS		= user32.lib xpcom.lib embed_base_s.lib 

# Default target.
DotNETEmbed.dll: ${OBJ_FILES} DotNETEmbed.snk ${DEPS}
	link ${LDFLAGS} ${LIBS} ${OBJ_FILES} -out:DotNETEmbed.dll
	@echo
	cp DotNETEmbed.dll ${MOZ_OBJ_DIR}/dist/bin
	cp DotNETEmbed.pdb ${MOZ_OBJ_DIR}/dist/bin
	regasm DotNETEmbed.dll
	@echo
	gacutil.exe -i DotNETEmbed.dll
	@echo

DotNETEmbed.obj: DotNETEmbed.cpp umWebChrome.h ${DEPS} 
	${CXX} ${CXXFLAGS} ${OUTPUT_OPTION} DotNETEmbed.cpp 
DotNETProfileManager.obj: DotNETProfileManager.cpp ${DEPS} 
	${CXX} ${CXXFLAGS} ${OUTPUT_OPTION} DotNETProfileManager.cpp 
DotNETNetworking.obj: DotNETNetworking.cpp DotNETNetworking.h ${DEPS} 
	${CXX} ${CXXFLAGS} ${OUTPUT_OPTION} DotNETNetworking.cpp 
AssemblyInfo.obj: AssemblyInfo.cpp ${DEPS} 
	${CXX} ${CXXFLAGS} ${OUTPUT_OPTION} AssemblyInfo.cpp 

extdlls:
	cp ${MOZ_OBJ_DIR}/dist/bin/nspr4.dll .
	cp ${MOZ_OBJ_DIR}/dist/bin/plc4.dll .
	cp ${MOZ_OBJ_DIR}/dist/bin/plds4.dll .
	cp ${MOZ_OBJ_DIR}/dist/bin/xpcom.dll .

clean:
	rm -f *.obj DotNETEmbed.dll
	rm -f *.pdb *.ilk

realclean: clean
	rm -f *~

