/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GECKO_LAYOUT_STYLE_FONTLOADERUTILS_H_
#define GECKO_LAYOUT_STYLE_FONTLOADERUTILS_H_

#include "ErrorList.h"
#include "nsContentSecurityManager.h"

class gfxUserFontEntry;
class nsIChannel;
class nsIHttpChannel;
class nsIInterfaceRequestor;
class nsILoadGroup;
class nsIURI;
struct gfxFontFaceSrc;

namespace mozilla {
enum CORSMode : uint8_t;
namespace dom {
class Document;
class WorkerPrivate;
enum class ReferrerPolicy : uint8_t;
}  // namespace dom

class FontLoaderUtils {
 public:
  // @param aSuppportsPriorityValue See <nsISupportsPriority.idl>.
  static nsresult BuildChannel(nsIChannel** aChannel, nsIURI* aURI,
                               const CORSMode aCORSMode,
                               const dom::ReferrerPolicy& aReferrerPolicy,
                               gfxUserFontEntry* aUserFontEntry,
                               const gfxFontFaceSrc* aFontFaceSrc,
                               dom::Document* aDocument,
                               nsILoadGroup* aLoadGroup,
                               nsIInterfaceRequestor* aCallbacks,
                               bool aIsPreload, int32_t aSupportsPriorityValue);

  static nsresult BuildChannel(nsIChannel** aChannel, nsIURI* aURI,
                               const CORSMode aCORSMode,
                               const dom::ReferrerPolicy& aReferrerPolicy,
                               gfxUserFontEntry* aUserFontEntry,
                               const gfxFontFaceSrc* aFontFaceSrc,
                               dom::WorkerPrivate* aWorkerPrivate,
                               nsILoadGroup* aLoadGroup,
                               nsIInterfaceRequestor* aCallbacks);

 private:
  static void BuildChannelFlags(
      nsIURI* aURI, bool aIsPreload,
      nsContentSecurityManager::CORSSecurityMapping& aCorsMapping,
      nsSecurityFlags& aSecurityFlags, nsContentPolicyType& aContentPolicyType);

  static nsresult BuildChannelSetup(nsIChannel* aChannel,
                                    nsIHttpChannel* aHttpChannel,
                                    nsIReferrerInfo* aReferrerInfo,
                                    const gfxFontFaceSrc* aFontFaceSrc,
                                    int32_t aSupportsPriorityValue);
};

}  // namespace mozilla

#endif  // GECKO_LAYOUT_STYLE_FONTLOADERUTILS_H_
