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
   RDFImage.cpp - Aurora Image loading class
   Created: Radha Kulkarni <radha@netscape.com>, 06-29-98
 */

#include "RDFImage.h"
#include "ViewGlue.h"
#include "xfe.h"
#include "il_util.h"

#ifdef DEBUG_radha
#define D(x) x
#else
#define D(x)
#endif

#if 0
extern "C"
{
void DisplayPixmap(MWContext *, IL_Pixmap *, IL_Pixmap * , PRInt32, PRInt32, PRInt32, PRInt32, PRInt32, PRInt32);
void NewPixmap(MWContext *, IL_Pixmap * image, PRBool mask);
void ImageComplete(MWContext *, IL_Pixmap * image);
void fe_load_default_font(MWContext *context);
};
#endif

int XFE_RDFImage::refCount = 0;
int XFE_RDFImage::m_numRDFImagesLoaded = 0;
RDFImageList * XFE_RDFImage::RDFImagesCache = (RDFImageList *) NULL;
// Max number of images in cache - to be replaced with very clever hash table
unsigned int XFE_RDFImage::MaxRdfImages = 30;
//unsigned int XFE_RDFImage::ImageListIncrSize = 20;


//////////////////////////////////////////////////////////////////////////


// Initialize the  images list. This s'd someday be a hash list
void 
XFE_RDFImage::imageCacheInitialize()
{

  /* The array way of indexing the cache only works for now when
   * NavCenterView is the only client of RDFImage. This w'd break or
   * corrupt if more clients come in for RDFImage. The cache needs
   * to be a Doubly linked list. That will happen in the next round
   * of changes to RDFImage.
   */
	if (RDFImagesCache == NULL)
	{
        m_numRDFImagesLoaded = 0;
		RDFImagesCache = (RDFImageList *) XP_CALLOC(MaxRdfImages, sizeof(RDFImageList));
	}
}

XFE_RDFImage::XFE_RDFImage(XFE_Component * frame,  
                           void * requestedObj, 
                           char * imageURL,
                           fe_colormap * cmap, 
                           Widget  baseWidget)
    : XFE_Image(frame, imageURL, cmap, baseWidget)
{

  // Initializations 
  pairCount = 0;
  completeCallback = (completeCallbackPtr) NULL;
  callbackData = (void *) NULL;


  // Make sure the cache is up and running.
//  XFE_RDFImage::imageCacheInitialize();


  if (cxtInitSucceeded)
    addListener(requestedObj, baseWidget, imageURL); 	

}


XFE_RDFImage::~XFE_RDFImage()
{
  int i = 0, j = 0;

 
  // Delete the bad image if m_numRDFImagesLoaded = 0
  if (m_numRDFImagesLoaded == 0)
  {
       D(printf("Need to delete the bad image here \n");)
       XP_FREE(RDFImagesCache);
  }

}


/* Set the callback for the requestor */
void
XFE_RDFImage::setCompleteCallback(completeCallbackPtr callback,  void * client_data)
{
     completeCallback  = callback;
     callbackData = client_data;

}


/* Image library callback with the image pixmap and mask. The mask
 * mask part is not working right now.
 */
void 
XFE_RDFImage::RDFDisplayPixmap(IL_Pixmap * image, IL_Pixmap * mask, PRInt32 width, PRInt32 height)
{
	m_image = image;
	m_mask = mask;
    imageWidth = width;
    imageHeight = height;
    pairCount++;

    if (!completelyLoaded) 
    {   
       callbackClientData * client_data = XP_NEW_ZAP(callbackClientData);
       client_data->widget = (Widget)callbackData;
       client_data->image = getPixmap();
       client_data->mask = getMask();
       client_data->width = width;
       client_data->height = height;
       completelyLoaded = True;
       if (isrequestorAlive(client_data->widget))
          (*completeCallback)(client_data);

    }
	return;
}

/* Image library callback */
void
XFE_RDFImage::RDFNewPixmap(IL_Pixmap * image, PRBool isMask)
{
	if (isMask)
		m_mask = image;
	else m_image = image;

}

/* This method is really useful when different images are
 * requested. If a request for the same image is made
 * in immediate succession like we do right now in NavCenterView.cpp
 * this doesn't work. So commented off for now
 */


void
XFE_RDFImage::RDFImageComplete(IL_Pixmap * image)
{
#ifdef UNDEF
	// Will get a call for both the mask and for the image.  
	if (m_image && m_mask)
	{
		if (pairCount == 2)
          if (isrequestorAlive)
			(*completeCallback)(getPixmap(), getMask(), callbackData);
		else 
          pairCount++;
	}
	else if (m_image)
	{
		// We have no mask.
        if (isrequestorAlive)
		   (*completeCallback)(getPixmap(), getMask(), callbackData);
	}
#endif  /* UNDEF  */

}

/* returns if the image has been completely loaded */
PRBool 
XFE_RDFImage::isImageLoaded(void)
{
     return (completelyLoaded);

}


/* return the image pixmap */
Pixmap
XFE_RDFImage::getPixmap(void)
{
   if (m_image && m_image->client_data)
     return (Pixmap)(((fe_PixmapClientData *)(m_image->client_data))->pixmap);
   return (Pixmap)NULL;
}

/* Return the image mask */
Pixmap
XFE_RDFImage::getMask(void)
{
    if (m_mask && m_mask->client_data)
       return (Pixmap)(((fe_PixmapClientData *)(m_mask->client_data))->pixmap);
    return (Pixmap)NULL;

}

/* Get image width */
PRInt32
XFE_RDFImage::getImageWidth(void)
{
  return(imageWidth);
}

/* Get image height */
PRInt32
XFE_RDFImage::getImageHeight(void)
{
  return(imageHeight);
}


/* Call NET_GetURL() to fetch the image */
void 
XFE_RDFImage::loadImage(void)
{
   if (cxtInitSucceeded)
     NET_GetURL(NET_CreateURLStruct(m_urlString, NET_DONT_RELOAD), 
                FO_CACHE_AND_PRESENT, m_imageContext, 
                XFE_Image::getURLExit_cb);
}



/*
 * Given a imageURL, this method goes thro' the cache and checks if
 * is already available. If available, it returns a handle to the
 * RDFImageobject, that holds the pixmap for the image.
 * The lookup is currently a simple strcmp. But it can be replaced by
 * a better system.
 */

XFE_RDFImage *
XFE_RDFImage::isImageAvailable(char * imageURL)
{
   RDFImageList * ptr = RDFImagesCache;
   if (!ptr)
      return (XFE_RDFImage *) NULL;
   do 
   {

      if ((!XP_STRCMP(imageURL, ptr->imageURL))) {
        /* If the RDFImage object related to the url 
         * is still around, and the pixmap has been completely loaded,
         * return handle to the RDFImage object.
         */

        if (ptr->obj && ((ptr->obj)->isImageLoaded()))
          return (ptr->obj); 
        else
          /* The image URL is in the cache,but the image is not ready yet */
          return((XFE_RDFImage *)NULL);
      }
      ptr = ptr->next;
   } while (ptr != RDFImagesCache);
   return (XFE_RDFImage *)NULL;

#ifdef UNDEF
int   i = 0;

  for (i = 0; i< m_numRDFImagesLoaded; i++)
  {
     if (!XP_STRCMP(imageURL, RDFImagesCache[i].imageURL))
     {
       /* If the RDFImage object related to the url 
        * is still around, and the pixmap has been completely loaded,
        * return handle to the RDFImage object.
        */

       if (RDFImagesCache[i].obj && ((RDFImagesCache[i].obj)->isImageLoaded()))
         return (RDFImagesCache[i].obj);
     }
   }   // for

   return (XFE_RDFImage * ) NULL;
#endif  /* UNDEF */
}



/* 
 * Remove a widget from the listener list.
 * This s'd be called by the destroy callback of the
 * widget that requested the image. This is to take care of cases
 * where a specific widget associated with the image goes away, but the
 * class that it originated from is not gone. This
 * works with isRequestorAlive()
 */

void
XFE_RDFImage::removeListener(Widget w)
{

   RDFImageList * ptr = RDFImagesCache;
   if (!ptr)
      return;
   do 
   {
      if (ptr->widget == w) {
        ptr->widget = NULL;
      }
      ptr = ptr->next;
   } while (ptr != RDFImagesCache);

}

/*
 * This method s'd be called from the requesting object's destructor. 
 * It basically goes through the cache and deletes all the RDFImage
 * objects that the requestor had requested.
 */

void
XFE_RDFImage::removeListener(void * obj)
{
   RDFImageList * ptr = RDFImagesCache, *p;
   int cnt = m_numRDFImagesLoaded;
   if (!ptr)
      return;

   do 
   {
      if (ptr->requestedObj == obj) {
        removeListener(ptr);
        // set the pointers properly
        ptr->prev->next = ptr->next;
        ptr->next->prev = ptr->prev;
        p = ptr->next;
        if (ptr == RDFImagesCache)
            RDFImagesCache = ptr->next;
        XP_FREE(ptr);
        ptr = p;
      }
      else
        ptr = ptr->next;
      cnt--;
   } while (/* ptr->next != RDFImagesCache */ cnt != 0) ;

}  /* removeListener */


/* free and reset an element in the cache */
void
XFE_RDFImage::removeListener(RDFImageList * ptr) {

    // free the string
    if (ptr->imageURL) {
         free(ptr->imageURL);
    }

    // Set the widget pointer to NULL
    ptr->widget = NULL;

    /* Delete the RDFImage object  */

     if (ptr->obj) {
         delete(ptr->obj);  
     }

    // reset the requestor's pointer 
    ptr->requestedObj = (void *)NULL;

    m_numRDFImagesLoaded--;

}  /* removeListener */


/* Add a new image to the cache */
void
XFE_RDFImage::addListener(void * requestedObj, Widget w, char * imageURL)
{

   RDFImageList * ptr = XP_NEW_ZAP(RDFImageList);

  if (m_numRDFImagesLoaded == 0) {
    /* First entry */
    RDFImagesCache = ptr;
  }
  ptr->next = RDFImagesCache;
  ptr->prev = ((m_numRDFImagesLoaded == 0) ? RDFImagesCache : RDFImagesCache->prev);
  (ptr->prev)->next = ptr;
  RDFImagesCache->prev = ptr;

  ptr->obj = this;
  ptr->requestedObj = requestedObj;
  ptr->widget  = w;
  ptr->imageURL  = (char *)XP_ALLOC(sizeof(char) * (strlen(imageURL) + 1));
  strncpy(ptr->imageURL, imageURL, strlen(imageURL));
  m_numRDFImagesLoaded++;
  

#ifdef UNDEF
  /* Go through the cache and see if there are any existing 
   * space that is available.
   */
  for (i=0; i<m_numRDFImagesLoaded; i++) {
    if (RDFImagesCache[i].isSpaceAvailable) {
      break;
    }
  }


  if (m_numRDFImagesLoaded == MaxRdfImages) {
    /* The array is full. realloc */
     RDFImagesCache =  (RDFImageList *) XP_REALLOC(RDFImagesCache, ((MaxRdfImages + ImageListIncrSize)*sizeof(RDFImageList)));
     MaxRdfImages +=ImageListIncrSize;

  }
  if (m_numRDFImagesLoaded < MaxRdfImages) {
RDFImagesCache[i].obj = this;
   RDFImagesCache[i].requestedObj = requestedObj;
   RDFImagesCache[i].widget  = w;
   RDFImagesCache[i].imageURL  = (char *)XP_ALLOC(sizeof(char) * (strlen(imageURL) + 1));
   strncpy(RDFImagesCache[i].imageURL, imageURL, strlen(imageURL));
    RDFImagesCache[i].isSpaceAvailable = False;
   m_numRDFImagesLoaded++;
  }
#endif   /* UNDEF  */

}   /* addListener */


/* 
 * This is called by RDFDisplayPixmap(), 
 * to make sure that the widget that
 * requested the image is still around
 */
PRBool 
XFE_RDFImage::isrequestorAlive(Widget w)
{
   RDFImageList * ptr = RDFImagesCache;

   if (!ptr)
     return(False);     
   do 
   {
      if (ptr->widget  == w) {
        return(True);
      }
      ptr = ptr->next;
   } while (ptr != RDFImagesCache);
   return (False);

#ifdef UNDEF
  for(int i = 0; i<m_numRDFImagesLoaded; i++) {
    if (RDFImagesCache[i].widget == w)
      return (True);
  }
  return (False);
#endif  /* UNDEF  */

}

/* 
 * Given a widget id, get a handle to the corresponding RDFImage object 
 * This is used by the Image library callbacks, for whom the widget id
 * is the only  link to the XFE world
 */

XFE_RDFImage * 
XFE_RDFImage::getRDFImageObject(Widget w)
{
   RDFImageList * ptr = RDFImagesCache;
   if (!ptr)
     return(XFE_RDFImage *)  NULL;
   do 
   {
      if (ptr->widget  == w) {
        return(ptr->obj);
      }
      ptr = ptr->next;
   } while (ptr != RDFImagesCache);
   return (XFE_RDFImage *)NULL;

#ifdef UNDEF
  for(int i = 0; i<m_numRDFImagesLoaded; i++) {
    if (RDFImagesCache[i].widget == w)
      return (RDFImagesCache[i].obj);
  }
  return (XFE_RDFImage *) NULL;
#endif  /* UNDEF  */
}



