/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentDLF_h__
#define nsContentDLF_h__

#include "nsIDocumentLoaderFactory.h"
#include "nsMimeTypes.h"

class nsDocShell;
class nsIChannel;
class nsIContentViewer;
class nsILoadGroup;
class nsIStreamListener;

#define CONTENT_DLF_CONTRACTID "@mozilla.org/content/document-loader-factory;1"
#define PLUGIN_DLF_CONTRACTID \
  "@mozilla.org/content/plugin/document-loader-factory;1"

class nsContentDLF final : public nsIDocumentLoaderFactory {
 protected:
  virtual ~nsContentDLF();

 public:
  nsContentDLF();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOCUMENTLOADERFACTORY

  nsresult InitUAStyleSheet();

  typedef already_AddRefed<mozilla::dom::Document> (*DocumentCreator)();
  nsresult CreateDocument(const char* aCommand, nsIChannel* aChannel,
                          nsILoadGroup* aLoadGroup, nsIDocShell* aContainer,
                          DocumentCreator aDocumentCreator,
                          nsIStreamListener** aDocListener,
                          nsIContentViewer** aContentViewer);

  /**
   * Create a blank document using the given loadgroup and given
   * principal.  aPrincipal is allowed to be null, in which case the
   * new document will get the about:blank content principal.
   */
  static already_AddRefed<mozilla::dom::Document> CreateBlankDocument(
      nsILoadGroup* aLoadGroup, nsIPrincipal* aPrincipal,
      nsIPrincipal* aPartitionedPrincipal, nsDocShell* aContainer);

 private:
  static nsresult EnsureUAStyleSheet();
  static bool IsImageContentType(const nsACString&);
};

nsresult NS_NewContentDocumentLoaderFactory(nsIDocumentLoaderFactory** aResult);

// clang-format off
#ifdef MOZ_WEBM
#define CONTENTDLF_WEBM_CATEGORIES \
    { "Gecko-Content-Viewers", VIDEO_WEBM, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", AUDIO_WEBM, "@mozilla.org/content/document-loader-factory;1" },
#else
#define CONTENTDLF_WEBM_CATEGORIES
#endif

#define CONTENTDLF_CATEGORIES \
    { "Gecko-Content-Viewers", TEXT_HTML, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", TEXT_PLAIN, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", TEXT_CACHE_MANIFEST, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", TEXT_CSS, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", TEXT_JAVASCRIPT, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", TEXT_ECMASCRIPT, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", APPLICATION_JAVASCRIPT, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", APPLICATION_ECMASCRIPT, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", APPLICATION_XJAVASCRIPT, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", APPLICATION_JSON, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", TEXT_JSON, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", APPLICATION_XHTML_XML, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", TEXT_XML, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", APPLICATION_XML, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", APPLICATION_RDF_XML, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", TEXT_RDF, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", VIEWSOURCE_CONTENT_TYPE, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", IMAGE_SVG_XML, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", APPLICATION_MATHML_XML, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", TEXT_VTT, "@mozilla.org/content/document-loader-factory;1" }, \
    { "Gecko-Content-Viewers", APPLICATION_WAPXHTML_XML, "@mozilla.org/content/document-loader-factory;1" }, \
    CONTENTDLF_WEBM_CATEGORIES
// clang-format on

#endif
