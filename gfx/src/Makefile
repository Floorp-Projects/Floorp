#!gmake
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

DEPTH = ../..

include $(DEPTH)/config/config.mk

#DIRS = motif

LIBRARY_NAME = raptorgfx

MODULE=raptor 

REQUIRES=util img xpcom raptor netlib

LCFLAGS=-D_IMPL_NS_GFX 

CPPSRCS=nsColor.cpp nsColorNames.cpp nsColorNamesRGB.cpp nsFont.cpp \
    nsImageGroup.cpp nsImageManager.cpp nsImageNetContext.cpp \
    nsImageRenderer.cpp nsImageRequest.cpp nsImageSystemServices.cpp \
    nsImageURL.cpp nsRect.cpp nsTransform2D.cpp nsFontCache.cpp 

EXPORTS=nsColor.h nsColorNames.h nsCoord.h nsFont.h nsRect.h nsPoint.h \
    nsSize.h nsMargin.h nsTransform2D.h nsIRenderingContext.h \
    nsIFontMetrics.h nsIImageManager.h nsIImageGroup.h nsIImageRequest.h \
    nsIImageObserver.h nsIDeviceContext.h nsIFontCache.h nsIImage.h nsIBlender.h \
    nsGfxCIID.h nsIRegion.h

include $(DEPTH)/config/rules.mk
