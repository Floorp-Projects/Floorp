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

#include "libimg.h"         /* Image library public API */
#include "xfe.h"
#include "View.h"

extern "C" {

typedef void (* completeCallbackPtr)(void * client_data);
void Icon_GetUrlExitRoutine(URL_Struct *pUrl, int iStatus, MWContext *pContext)  ;

};

typedef struct  _callbackClientData {

    Widget   widget;
    Pixmap   image;
    Pixmap   mask;
    jint     width;
    jint     height;
}callbackClientData;



class XFE_RDFImage {

public:
  XFE_RDFImage(XFE_Component * frame, char * imageUrl, fe_colormap *, Widget);
  ~XFE_RDFImage();
  void loadImage();
  Pixmap  getPixmap();
  Pixmap  getMask();
  Boolean  isImageLoaded();
  void     setCompleteCallback(completeCallbackPtr    callback, void * callbackData);

  void XFE_RDFImage::RDFDisplayPixmap(IL_Pixmap * image, IL_Pixmap * mask, long int width, long int height);
  void XFE_RDFImage::RDFNewPixmap(IL_Pixmap * image, Boolean isMask);
  void XFE_RDFImage::RDFImageComplete(IL_Pixmap * image);


private:
  char * m_urlString;         // Url string
  MWContext * m_imageContext; // Special MWContext
  fe_ContextData * fec;       // FE specific data for MWContext
  IL_Pixmap *  m_image;     
  IL_Pixmap *  m_mask;
  static Pixmap   m_badImage;   // Bad image to use if the image loading fails
  XFE_Component * m_frame;


  Boolean     badImage;     // Indicates whether to use the bad bitmap
  Boolean     completelyLoaded;  // Indicates if image is completely loaded
  Boolean     frameLoaded;    // Indicates if frame is loaded
  Boolean     cxtInitSucceeded;  // Indicates if MWcontext is initialized successfully
  Boolean     imageDrawn;


  int         pairCount;      // Specifies whether both pixmap and mask are available
  static int  refCount;      // Count of # of images loaded

  completeCallbackPtr    completeCallback;   // Callback to call after complete image has been obtained.
  void *      callbackData;

};







