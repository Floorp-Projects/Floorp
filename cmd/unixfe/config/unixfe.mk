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
##########################################################################
ifdef MOZ_NATIVE_JPEG

XFE_JPEG_LDFLAGS = -ljpeg

else

XFE_JPEG_LDFLAGS = $(DIST)/lib/libjpeg.a

endif

##########################################################################
#
# MOZ_NATIVE_PNG
#
# The default usage for libpng is that its built as part of Mozilla.
#
# MOZ_NATIVE_PNG can be set to make Mozilla build using a 'native' libpng
# as found in platforms such as Linux and IRIX.
#
##########################################################################
ifdef MOZ_NATIVE_PNG

XFE_PNG_LDFLAGS = -lpng

else

XFE_PNG_LDFLAGS = $(DIST)/lib/libpng.a

endif

##########################################################################
#
# MOZ_NATIVE_ZLIB
#
# The default usage for zlib is that its built as part of Mozilla.
# 
# MOZ_NATIVE_JPEG can be set to make Mozilla build using a 'native' libpng
# as found in platforms such as Linux and IRIX.
#
# When built as part of Mozilla, it takes the name 'libzlib'
#
# The native zlib libs seem to be named 'libz' - at least on Linux.
#
# The FULL_STATIC_BUILD force the final Mozilla binary to be as fully
# staitc as possible - which includes zlib.
#
# The cmd/xfe/icons/mkicons present a problem.  Its not important how this
# utility is linked, because its not released.  In any case, im setting
# XFE_ZLIB_MKICONS_LDFLAGS over here to avoid ifdef hackery in the mkicons
# Makefile(s).
#
##########################################################################
ifdef MOZ_NATIVE_ZLIB

XFE_ZLIB_LDFLAGS = -lz

XFE_ZLIB_MKICONS_LDFLAGS = -lz

else

ifdef FULL_STATIC_BUILD

XFE_ZLIB_LDFLAGS = $(DIST)/lib/libzlib.a

XFE_ZLIB_MKICONS_LDFLAGS = $(DIST)/lib/libzlib.a

else

XFE_ZLIB_DSO_LDFLAGS = -lzlib

XFE_ZLIB_MKICONS_LDFLAGS = -lzlib

endif

endif

# eof
