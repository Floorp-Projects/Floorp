/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
/* 
   XFEImages.h - AuroraImage loading class
   Created: Radha Kulkarni <radha@netscape.com>, 06-29-98
 */

#ifndef _xfe_rdfimage_h
#define _xfe_rdfimage_h

#include "libimg.h"         /* Image library public API */
#include "xfe.h"
#include "View.h"
#include "Image.h"

typedef void (* completeCallbackPtr)(void * client_data);

typedef struct _RDFImageList {

  void *                 requestedObj;
  class XFE_RDFImage *   obj;
  Widget                 widget;
  char *                 imageURL;
  PRBool                 isSpaceAvailable;
  struct _RDFImageList * prev;
  struct _RDFImageList * next;

} RDFImageList;

typedef struct  _callbackClientData {

    Widget      widget;
    Pixmap      image;
    Pixmap      mask;
    PRInt32     width;
    PRInt32     height;

} callbackClientData;

//////////////////////////////////////////////////////////////////////////

class XFE_RDFImage : public XFE_Image
{
public:

  XFE_RDFImage(XFE_Component * frame, void * requestedObj,
               char * imageUrl, fe_colormap *, Widget);

  ~XFE_RDFImage();

  void        setCompleteCallback   (completeCallbackPtr callback,
                                     void * callbackData);
  void        RDFDisplayPixmap      (IL_Pixmap * image, IL_Pixmap *  mask,
                                     PRInt32 width, PRInt32 height);

  void        RDFNewPixmap          (IL_Pixmap * image, PRBool isMask);
  void        RDFImageComplete      (IL_Pixmap * image);
  void        addListener           (void * requestedObj, Widget w,
                                     char * imageURL);
  PRBool      isrequestorAlive      (Widget w);

  virtual Pixmap     getPixmap         ();
  virtual Pixmap     getMask           ();
  virtual PRInt32    getImageWidth     ();
  virtual PRInt32    getImageHeight    ();
  virtual void       loadImage         ();
  virtual PRBool     isImageLoaded     ();
 
  static XFE_RDFImage *   isImageAvailable      (char * imageURL);
  static void             removeListener        (void * obj);
  static void             removeListener        (Widget w);
  static XFE_RDFImage *   getRDFImageObject     (Widget);

private:

  PRBool        frameLoaded;    // Indicates if frame is loaded
  int           pairCount;      // Specifies whether pixmap and mask are ready
  static int    refCount;       // Count of # of images loaded
  static int    m_numRDFImagesLoaded;  // # of images in the cache

  static RDFImageList * RDFImagesCache;   // Images cache
  static unsigned int MaxRdfImages;       // Max # of images the cache can hold
  static unsigned int ImageListIncrSize;  // cache size increment.

  static void    removeListener        (RDFImageList *);
  static void    imageCacheInitialize  ();

  // Callback to call after complete image has been obtained.
  completeCallbackPtr    completeCallback;

  void *      callbackData;
};

#endif  /* _xfe_rdfimage_h */





