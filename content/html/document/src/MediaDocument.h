/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  MediaDocument();
  virtual ~MediaDocument();

  virtual nsresult Init();

  virtual nsresult StartDocumentLoad(const char*         aCommand,
                                     nsIChannel*         aChannel,
                                     nsILoadGroup*       aLoadGroup,
                                     nsISupports*        aContainer,
                                     nsIStreamListener** aDocListener,
                                     bool                aReset = true,
                                     nsIContentSink*     aSink = nsnull);

protected:
  virtual nsresult CreateSyntheticDocument();

  friend class MediaDocumentStreamListener;
  nsresult StartLayout();

  void GetFileName(nsAString& aResult);

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
                             PRInt32            aWidth = 0,
                             PRInt32            aHeight = 0,
                             const nsAString&   aStatus = EmptyString());

  nsCOMPtr<nsIStringBundle>     mStringBundle;
  static const char* const      sFormatNames[4];

private:
  enum                          {eWithNoInfo, eWithFile, eWithDim, eWithDimAndFile};
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
