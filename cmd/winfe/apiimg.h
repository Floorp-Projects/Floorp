/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef __APIIMG_H
#define __APIIMG_H

#ifndef __APIAPI_H
    #include "apiapi.h"
#endif
#ifndef __NSGUIDS_H
    #include "nsguids.h"
#endif

#define	APICLASS_IMAGEMAP   "ImageMap"

class IImageMap 
{
public:
    virtual int Initialize ( 
            unsigned int rcid,
            int width,
            int height
            ) = 0;

// Please use these draw functions instead of the MFC ones below

    virtual void DrawImage (
            int index,
            int x,
            int y,
            HDC hDestDC,
            BOOL bButton
            ) = 0;

	virtual void DrawTransImage (
        int index,
        int x,
        int y,
        HDC hDestDC
        ) = 0;

// Please don't use these functions, they are obsolete
// and provided only for compatibility

    virtual void DrawImage (
            int index,
            int x,
            int y,
            CDC * hDestDC,
            BOOL bButton
            ) = 0;

	virtual void DrawTransImage (
        int index,
        int x,
        int y,
        CDC * hDestDC
        ) = 0;

    virtual void ReInitialize (
            void
            ) = 0;

    virtual void UseNormal (
            void
            ) = 0;

    virtual void UseHighlight (
            void
            ) = 0;

    virtual int GetImageHeight (
            void
            ) = 0;

    virtual int GetImageWidth (
            void
            ) = 0;

    virtual int GetImageCount (
            void
            ) = 0;

    virtual unsigned int GetResourceID (
            void
            ) = 0;

};

typedef IImageMap * LPIMAGEMAP;

#define ApiImageMap(v,unk) APIPTRDEF(IID_IImageMap,IImageMap,v,unk)

#endif
