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
# The Original Code is Mozilla Communicator client code.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
# 

# Microsoft Visual C++ generated build script - Do not modify

PROJ = LDAP16
DEBUG = 0
PROGTYPE = 0
CALLER = 
ARGS = 
DLLS = 
D_RCDEFINES = -d_DEBUG
R_RCDEFINES = -dNDEBUG
ORIGIN = MSVC
ORIGIN_VER = 1.00
PROJPATH = C:\NETSCAPE\LDAPSDK\EXAMPLES\WINDOWS\WINLDAP\
USEMFC = 1
CC = cl
CPP = cl
CXX = cl
CCREATEPCHFLAG = 
CPPCREATEPCHFLAG = 
CUSEPCHFLAG = 
CPPUSEPCHFLAG = 
FIRSTC =             
FIRSTCPP = CONNDLG.CPP 
RC = rc
CFLAGS_D_WEXE = /nologo /G3 /W3 /Zi /AL /Od /D "_DEBUG" /D "_AFX_NO_AFXCMN_SUPPORT" /D "_WINDOWS" /FR /GA /Fd"LDAP16.PDB"
CFLAGS_R_WEXE = /nologo /W3 /AL /O1 /D "NDEBUG" /D "_AFX_NO_AFXCMN_SUPPORT" /D "_WINDOWS" /FR /GA 
LFLAGS_D_WEXE = /NOLOGO /NOD /PACKC:61440 /STACK:10240 /ALIGN:16 /ONERROR:NOEXE /CO  
LFLAGS_R_WEXE = /NOLOGO /NOD /PACKC:61440 /STACK:10240 /ALIGN:16 /ONERROR:NOEXE  
LIBS_D_WEXE = nsldap.lib lafxcwd oldnames libw llibcew commdlg.lib shell.lib 
LIBS_R_WEXE = nsldap.lib lafxcw oldnames libw llibcew commdlg.lib shell.lib 
RCFLAGS = /nologo
RESFLAGS = /nologo
RUNFLAGS = 
DEFFILE = LDAP16.DEF
OBJS_EXT = 
LIBS_EXT = 
!if "$(DEBUG)" == "1"
CFLAGS = $(CFLAGS_D_WEXE)
LFLAGS = $(LFLAGS_D_WEXE)
LIBS = $(LIBS_D_WEXE)
MAPFILE = nul
RCDEFINES = $(D_RCDEFINES)
!else
CFLAGS = $(CFLAGS_R_WEXE)
LFLAGS = $(LFLAGS_R_WEXE)
LIBS = $(LIBS_R_WEXE)
MAPFILE = nul
RCDEFINES = $(R_RCDEFINES)
!endif
!if [if exist MSVC.BND del MSVC.BND]
!endif
SBRS = CONNDLG.SBR \
		LDAPDOC.SBR \
		LDAPVIEW.SBR \
		MAINFRM.SBR \
		PROPDLG.SBR \
		SRCHDLG.SBR \
		STDAFX.SBR \
		WINLDAP.SBR


CONNDLG_DEP = c:\netscape\ldapsdk\examples\windows\winldap\stdafx.h \
	c:\netscape\ldapsdk\examples\windows\winldap\winldap.h \
	c:\netscape\ldapsdk\examples\windows\winldap\conndlg.h


LDAPDOC_DEP = c:\netscape\ldapsdk\examples\windows\winldap\stdafx.h \
	c:\netscape\ldapsdk\examples\windows\winldap\winldap.h \
	c:\netscape\ldapsdk\examples\windows\winldap\ldapdoc.h


LDAPVIEW_DEP = c:\netscape\ldapsdk\examples\windows\winldap\stdafx.h \
	c:\netscape\ldapsdk\include\ldap.h \
	c:\netscape\ldapsdk\include\lber.h \
	c:\netscape\ldapsdk\examples\windows\winldap\winldap.h \
	c:\netscape\ldapsdk\examples\windows\winldap\ldapdoc.h \
	c:\netscape\ldapsdk\examples\windows\winldap\ldapview.h \
	c:\netscape\ldapsdk\examples\windows\winldap\propdlg.h


MAINFRM_DEP = c:\netscape\ldapsdk\examples\windows\winldap\stdafx.h \
	c:\netscape\ldapsdk\include\ldap.h \
	c:\netscape\ldapsdk\include\lber.h \
	c:\netscape\ldapsdk\examples\windows\winldap\winldap.h \
	c:\netscape\ldapsdk\examples\windows\winldap\mainfrm.h


PROPDLG_DEP = c:\netscape\ldapsdk\examples\windows\winldap\stdafx.h \
	c:\netscape\ldapsdk\include\ldap.h \
	c:\netscape\ldapsdk\include\lber.h \
	c:\netscape\ldapsdk\examples\windows\winldap\winldap.h \
	c:\netscape\ldapsdk\examples\windows\winldap\propdlg.h


SRCHDLG_DEP = c:\netscape\ldapsdk\examples\windows\winldap\stdafx.h \
	c:\netscape\ldapsdk\include\ldap.h \
	c:\netscape\ldapsdk\include\lber.h \
	c:\netscape\ldapsdk\examples\windows\winldap\winldap.h \
	c:\netscape\ldapsdk\examples\windows\winldap\srchdlg.h


STDAFX_DEP = c:\netscape\ldapsdk\examples\windows\winldap\stdafx.h


WINLDAP_DEP = c:\netscape\ldapsdk\examples\windows\winldap\stdafx.h \
	c:\netscape\ldapsdk\include\ldap.h \
	c:\netscape\ldapsdk\include\lber.h \
	c:\netscape\ldapsdk\examples\windows\winldap\winldap.h \
	c:\netscape\ldapsdk\examples\windows\winldap\mainfrm.h \
	c:\netscape\ldapsdk\examples\windows\winldap\ldapdoc.h \
	c:\netscape\ldapsdk\examples\windows\winldap\ldapview.h \
	c:\netscape\ldapsdk\examples\windows\winldap\conndlg.h \
	c:\netscape\ldapsdk\examples\windows\winldap\srchdlg.h


LDAP16_RCDEP = c:\netscape\ldapsdk\examples\windows\winldap\res\winldap.ico \
	c:\netscape\ldapsdk\examples\windows\winldap\res\winldap.rc2


all:	$(PROJ).EXE $(PROJ).BSC

CONNDLG.OBJ:	CONNDLG.CPP $(CONNDLG_DEP)
	$(CPP) $(CFLAGS) $(CPPCREATEPCHFLAG) /c CONNDLG.CPP

LDAPDOC.OBJ:	LDAPDOC.CPP $(LDAPDOC_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c LDAPDOC.CPP

LDAPVIEW.OBJ:	LDAPVIEW.CPP $(LDAPVIEW_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c LDAPVIEW.CPP

MAINFRM.OBJ:	MAINFRM.CPP $(MAINFRM_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c MAINFRM.CPP

PROPDLG.OBJ:	PROPDLG.CPP $(PROPDLG_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c PROPDLG.CPP

SRCHDLG.OBJ:	SRCHDLG.CPP $(SRCHDLG_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c SRCHDLG.CPP

STDAFX.OBJ:	STDAFX.CPP $(STDAFX_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c STDAFX.CPP

WINLDAP.OBJ:	WINLDAP.CPP $(WINLDAP_DEP)
	$(CPP) $(CFLAGS) $(CPPUSEPCHFLAG) /c WINLDAP.CPP

LDAP16.RES:	LDAP16.RC $(LDAP16_RCDEP)
	$(RC) $(RCFLAGS) $(RCDEFINES) -r LDAP16.RC


$(PROJ).EXE::	LDAP16.RES

$(PROJ).EXE::	CONNDLG.OBJ LDAPDOC.OBJ LDAPVIEW.OBJ MAINFRM.OBJ PROPDLG.OBJ SRCHDLG.OBJ \
	STDAFX.OBJ WINLDAP.OBJ $(OBJS_EXT) $(DEFFILE)
	echo >NUL @<<$(PROJ).CRF
STDAFX.OBJ +
CONNDLG.OBJ +
LDAPDOC.OBJ +
LDAPVIEW.OBJ +
MAINFRM.OBJ +
PROPDLG.OBJ +
SRCHDLG.OBJ +
WINLDAP.OBJ +
$(OBJS_EXT)
$(PROJ).EXE
$(MAPFILE)
C:\MSVC\LIB\+
C:\MSVC\MFC\LIB\+
..\..\..\lib\+
$(LIBS)
$(DEFFILE);
<<
	link $(LFLAGS) @$(PROJ).CRF
	$(RC) $(RESFLAGS) LDAP16.RES $@
	@copy $(PROJ).CRF MSVC.BND

$(PROJ).EXE::	LDAP16.RES
	if not exist MSVC.BND 	$(RC) $(RESFLAGS) LDAP16.RES $@

run: $(PROJ).EXE
	$(PROJ) $(RUNFLAGS)


$(PROJ).BSC: $(SBRS)
	bscmake @<<
/o$@ $(SBRS)
<<
