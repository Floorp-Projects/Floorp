/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaDocument_h
#define mozilla_dom_MediaDocument_h

#include "mozilla/Attributes.h"
#include "nsHTMLDocument.h"
#include "nsGenericHTMLElement.h"
#include "nsIStringBundle.h"
#include "nsIThreadRetargetableStreamListener.h"

#define NSMEDIADOCUMENT_PROPERTIES_URI \
  "chrome://global/locale/layout/MediaDocument.properties"

#define NSMEDIADOCUMENT_PROPERTIES_URI_en_US \
  "resource://gre/res/locale/layout/MediaDocument.properties"

namespace mozilla::dom {

class MediaDocument : public nsHTMLDocument {
 public:
  MediaDocument();
  virtual ~MediaDocument();

  // Subclasses need to override this.
  enum MediaDocumentKind MediaDocumentKind() const override = 0;

  virtual nsresult Init() override;

  virtual nsresult StartDocumentLoad(const char* aCommand, nsIChannel* aChannel,
                                     nsILoadGroup* aLoadGroup,
                                     nsISupports* aContainer,
                                     nsIStreamListener** aDocListener,
                                     bool aReset = true) override;

  virtual bool WillIgnoreCharsetOverride() override { return true; }

 protected:
  // Hook to be called once our initial document setup is done.  Subclasses
  // should call this from SetScriptGlobalObject when setup hasn't been done
  // yet, a non-null script global is being set, and they have finished whatever
  // setup work they plan to do for an initial load.
  void InitialSetupDone();

  // Check whether initial setup has been done.
  [[nodiscard]] bool InitialSetupHasBeenDone() const {
    return mDidInitialDocumentSetup;
  }

  virtual nsresult CreateSyntheticDocument();

  friend class MediaDocumentStreamListener;
  virtual nsresult StartLayout();

  void GetFileName(nsAString& aResult, nsIChannel* aChannel);

  nsresult LinkStylesheet(const nsAString& aStylesheet);
  nsresult LinkScript(const nsAString& aScript);

  void FormatStringFromName(const char* aName,
                            const nsTArray<nsString>& aParams,
                            nsAString& aResult);

  // |aFormatNames[]| needs to have four elements in the following order:
  // a format name with neither dimension nor file, a format name with
  // filename but w/o dimension, a format name with dimension but w/o filename,
  // a format name with both of them.  For instance, it can have
  // "ImageTitleWithNeitherDimensionsNorFile", "ImageTitleWithoutDimensions",
  // "ImageTitleWithDimesions2",  "ImageTitleWithDimensions2AndFile".
  //
  // Also see MediaDocument.properties if you want to define format names
  // for a new subclass. aWidth and aHeight are pixels for |ImageDocument|,
  // but could be in other units for other 'media', in which case you have to
  // define format names accordingly.
  void UpdateTitleAndCharset(const nsACString& aTypeStr, nsIChannel* aChannel,
                             const char* const* aFormatNames = sFormatNames,
                             int32_t aWidth = 0, int32_t aHeight = 0,
                             const nsAString& aStatus = u""_ns);

  nsCOMPtr<nsIStringBundle> mStringBundle;
  nsCOMPtr<nsIStringBundle> mStringBundleEnglish;
  static const char* const sFormatNames[4];

 private:
  enum { eWithNoInfo, eWithFile, eWithDim, eWithDimAndFile };

  // A boolean that indicates whether we did our initial document setup.  This
  // will be false initially, become true when we finish setting up the document
  // during initial load and stay true thereafter.
  bool mDidInitialDocumentSetup;
};

class MediaDocumentStreamListener : public nsIStreamListener,
                                    public nsIThreadRetargetableStreamListener {
 protected:
  virtual ~MediaDocumentStreamListener();

 public:
  explicit MediaDocumentStreamListener(MediaDocument* aDocument);

  NS_DECL_THREADSAFE_ISUPPORTS

  NS_DECL_NSIREQUESTOBSERVER

  NS_DECL_NSISTREAMLISTENER

  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER

  void DropDocumentRef() { mDocument = nullptr; }

  RefPtr<MediaDocument> mDocument;
  nsCOMPtr<nsIStreamListener> mNextStream;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_MediaDocument_h */
