/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaDocument_h
#define mozilla_dom_MediaDocument_h

#include "nsHTMLDocument.h"
#include "nsGenericHTMLElement.h"
#include "nsAutoPtr.h"
#include "nsIStringBundle.h"

#define NSMEDIADOCUMENT_PROPERTIES_URI "chrome://global/locale/layout/MediaDocument.properties"

namespace mozilla {
namespace dom {

class MediaDocument : public nsHTMLDocument
{
public:
  MediaDocument(bool aUseXPConnectToWrap = false);
  virtual ~MediaDocument();

  virtual nsresult Init();

  virtual nsresult StartDocumentLoad(const char*         aCommand,
                                     nsIChannel*         aChannel,
                                     nsILoadGroup*       aLoadGroup,
                                     nsISupports*        aContainer,
                                     nsIStreamListener** aDocListener,
                                     bool                aReset = true,
                                     nsIContentSink*     aSink = nullptr);

  virtual void SetScriptGlobalObject(nsIScriptGlobalObject* aGlobalObject);

protected:
  void BecomeInteractive();

  virtual nsresult CreateSyntheticDocument();

  friend class MediaDocumentStreamListener;
  nsresult StartLayout();

  void GetFileName(nsAString& aResult);

  nsresult LinkStylesheet(const nsAString& aStylesheet);

  // |aFormatNames[]| needs to have four elements in the following order: 
  // a format name with neither dimension nor file, a format name with
  // filename but w/o dimension, a format name with dimension but w/o filename,
  // a format name with both of them.  For instance, it can have
  // "ImageTitleWithNeitherDimensionsNorFile", "ImageTitleWithoutDimensions",
  // "ImageTitleWithDimesions",  "ImageTitleWithDimensionsAndFile".
  //
  // Also see MediaDocument.properties if you want to define format names
  // for a new subclass. aWidth and aHeight are pixels for |ImageDocument|,
  // but could be in other units for other 'media', in which case you have to 
  // define format names accordingly. 
  void UpdateTitleAndCharset(const nsACString&  aTypeStr,
                             const char* const* aFormatNames = sFormatNames,
                             int32_t            aWidth = 0,
                             int32_t            aHeight = 0,
                             const nsAString&   aStatus = EmptyString());

  nsCOMPtr<nsIStringBundle>     mStringBundle;
  static const char* const      sFormatNames[4];
  
private:
  enum                          {eWithNoInfo, eWithFile, eWithDim, eWithDimAndFile};
  bool                          mDocumentElementInserted;   
};


class MediaDocumentStreamListener: public nsIStreamListener
{
public:
  MediaDocumentStreamListener(MediaDocument *aDocument);
  virtual ~MediaDocumentStreamListener();
  void SetStreamListener(nsIStreamListener *aListener);

  NS_DECL_ISUPPORTS

  NS_DECL_NSIREQUESTOBSERVER

  NS_DECL_NSISTREAMLISTENER

  nsRefPtr<MediaDocument>      mDocument;
  nsCOMPtr<nsIStreamListener>  mNextStream;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_MediaDocument_h */
