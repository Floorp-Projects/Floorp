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
# The Original Code is the Mozilla browser.
# 
# The Initial Developer of the Original Code is Christopher
# Blizzard.  Portions created by Christopher Blizzard
# Copyright (C) 1999, Christopher Blizzard.  All Rights Reserved.
#
# Contributor(s):
#

topsrcdir       = ${MOZ_SRC}/mozilla
srcdir          = ${MOZ_SRC}/mozilla/embedding/wrappers/DotNETEmbed
VPATH           = ${MOZ_SRC}/mozilla/embedding/wrappers/DotNETEmbed

# Compiler variables (implicit rules are used to compile .cpp files)
CC		= cl
CFLAGS	= -nologo -Zi
CXX			= cl
CXXFLAGS	= -nologo -CLR -TP -Zi -we4700 -we4701 -c -DDEBUG -DXP_WIN -DXP_WIN32 ${INCLUDES}

OUTPUT_OPTION	= -Fo$@

# Linker variables (implicit rules can't use link.exe, so we do that manually)
LDFLAGS		= -nologo -dll -debug -libpath:${topsrcdir}/dist/lib/ -NODEFAULTLIB:MSVCRTD

INCLUDES	= \
		  -I${topsrcdir}/dist/include     \
		  -I${topsrcdir}/dist/include/profile     \
		  -I${topsrcdir}/dist/include/webbrwsr    \
		  -I${topsrcdir}/dist/include/widget    \
		  -I${topsrcdir}/dist/include/xpcom     \
		  -I${topsrcdir}/dist/include/nspr      \
		  -I${topsrcdir}/dist/include/necko      \
		  -I${topsrcdir}/dist/include/docshell        \
		  -I${topsrcdir}/dist/include/dom       \
		  -I${topsrcdir}/dist/include/string    \
		  -I${topsrcdir}/dist/include/embed_base    \
		  -I${topsrcdir}/dist/include/windowwatcher \
		  -I${topsrcdir}/dist/include/xpconnect

DEPS      = DotNETEmbed.h Makefile

CPP_FILES	=  \
		  DotNETEmbed.cpp     \
		  DotNETProfileManager.cpp     \
		  DotNETNetworking.cpp     \
		  AssemblyInfo.cpp                

OBJ_FILES	= $(CPP_FILES:%.cpp=%.obj)

LIBS		= user32.lib xpcom.lib embed_base_s.lib 

# Default target.
DotNETEmbed.dll: ${OBJ_FILES} DotNETEmbed.snk ${DEPS}
	link ${LDFLAGS} ${LIBS} ${OBJ_FILES} -out:DotNETEmbed.dll
	@echo
	cp DotNETEmbed.dll ${topsrcdir}/dist/bin
	cp DotNETEmbed.pdb ${topsrcdir}/dist/bin
#	regasm -silent DotNETEmbed.dll
#	@echo
#	gacutil.exe -silent -i DotNETEmbed.dll
#	@echo
#	@touch vc7/Debug/*
#	cp DotNETEmbed.dll vc7/Debug
#	@echo

DotNETEmbed.obj: DotNETEmbed.cpp umWebChrome.h ${DEPS} 
	${CXX} ${CXXFLAGS} ${OUTPUT_OPTION} DotNETEmbed.cpp 
DotNETProfileManager.obj: DotNETProfileManager.cpp umProfileManager.h ${DEPS} 
	${CXX} ${CXXFLAGS} ${OUTPUT_OPTION} DotNETProfileManager.cpp 
DotNETNetworking.obj: DotNETNetworking.cpp umURI.h ${DEPS} 
	${CXX} ${CXXFLAGS} ${OUTPUT_OPTION} DotNETNetworking.cpp 
AssemblyInfo.obj: AssemblyInfo.cpp ${DEPS} 
	${CXX} ${CXXFLAGS} ${OUTPUT_OPTION} AssemblyInfo.cpp 

extdlls:
	cp ${topsrcdir}/dist/bin/nspr4.dll .
	cp ${topsrcdir}/dist/bin/plc4.dll .
	cp ${topsrcdir}/dist/bin/plds4.dll .
	cp ${topsrcdir}/dist/bin/xpcom.dll .

clean:
	rm -f *.obj DotNETEmbed.dll
	rm -f *.pdb *.ilk

realclean: clean
	rm -f *~

