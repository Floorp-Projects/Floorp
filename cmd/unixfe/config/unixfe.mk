#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.
#

##########################################################################
#                                                                        #
#                           STOP THE INSANITY                            #
#                                                                        #
##########################################################################


##########################################################################
#
# Name:			unixfe.mk
#
# Description:	Makefile hackery shared across unix and unik-like 
#               Mozilla front ends.
#
#               The purpose of this file is to STOP THE INSANITY that
#               has become the XFE Makefile and that will soon be the 
#               gnomefe, qtfe, ybfe and foofe Makfiles.
#
#               Anything that can be shared between the unix fes, should
#               be shared.  Always look here first before adding anything
#               to the toolkit specific Makefiles, or you will suffer 
#               dearly.
#
# Author:		Ramiro Estrugo <ramiro@netscape.com>
#
##########################################################################

##########################################################################
#
# MOZ_NATIVE_JPEG
#
# The default usage for libjpeg is that its built as part of Mozilla.
#
# MOZ_NATIVE_JPEG can be set to make Mozilla build using a 'native' libjpeg
# as found in platforms such as Linux and IRIX.
#
# Only one of the following gets set:
#
# XFE_JPEG_LIB
# XFE_JPEG_DSO
#
# The following get set if required jpeg lib is not native, but its
# dynamic:
#
# XFE_JPEG_REQUIRES
# XFE_MKICONS_BIN_LDPATH
#
##########################################################################
ifdef MOZ_NATIVE_JPEG

XFE_JPEG_LIB				=
XFE_JPEG_DSO				= -ljpeg
XFE_JPEG_REQUIRES			=
XFE_MKICONS_BIN_LDPATH		=

else

XFE_JPEG_LIB				= $(DIST)/lib/libjpeg.a
XFE_JPEG_DSO				= 
XFE_JPEG_REQUIRES			= jpeg
XFE_MKICONS_BIN_LDPATH		= -L$(DIST)/bin

endif
##########################################################################



##########################################################################
#
# MOZ_NATIVE_PNG
#
# The default usage for libpng is that its built as part of Mozilla.
#
# MOZ_NATIVE_PNG can be set to make Mozilla build using a 'native' libpng
# as found in platforms such as Linux and IRIX.
#
# Only one of the following gets set:
#
# XFE_PNG_LIB
# XFE_PNG_DSO
#
# The following get set if required png lib is not native, but its
# dynamic:
#
# XFE_PNG_REQUIRES
# XFE_MKICONS_BIN_LDPATH
#
##########################################################################
ifdef MOZ_NATIVE_PNG

XFE_PNG_LIB					=
XFE_PNG_DSO					= -lpng
XFE_PNG_REQUIRES			=
XFE_MKICONS_BIN_LDPATH		=

else

XFE_PNG_LIB					= $(DIST)/lib/libpng.a
XFE_PNG_DSO					= 
XFE_PNG_REQUIRES			= png
XFE_MKICONS_BIN_LDPATH		= -L$(DIST)/bin

endif
##########################################################################



##########################################################################
#
# MOZ_NATIVE_ZLIB
#
# The default usage for libzlib is that its built as part of Mozilla.
#
# MOZ_NATIVE_ZLIB can be set to make Mozilla build using a 'native' libzlib
# as found in platforms such as Linux and IRIX.
#
# When built as part of Mozilla, it takes the name 'libzlib'
#
# The native zlib libs seem to be named 'libz' - at least on Linux.
#
# The FULL_STATIC_BUILD force the final Mozilla binary to be as fully
# staitc as possible - which includes zlib.
#
# Only one of the following gets set:
#
# XFE_ZLIB_LIB
# XFE_ZLIB_DSO
#
# The following get set if required zlib lib is not native, but its
# dynamic:
#
# XFE_ZLIB_REQUIRES
# XFE_MKICONS_BIN_LDPATH
#
##########################################################################
ifdef MOZ_NATIVE_ZLIB

XFE_ZLIB_LIB				=
XFE_ZLIB_DSO				= -lz
XFE_ZLIB_REQUIRES			=
XFE_MKICONS_BIN_LDPATH		=

else

ifdef FULL_STATIC_BUILD

XFE_ZLIB_LIB				= $(DIST)/lib/libzlib.a
XFE_ZLIB_DSO				=
XFE_ZLIB_REQUIRES			= zlib
XFE_MKICONS_BIN_LDPATH		=

else

XFE_ZLIB_LIB				=
XFE_ZLIB_DSO				= -lzlib
XFE_ZLIB_REQUIRES			= zlib
XFE_MKICONS_BIN_LDPATH		= -L$(DIST)/bin

endif

endif

# eof
