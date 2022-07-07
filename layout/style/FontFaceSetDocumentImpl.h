/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FontFaceSetDocumentImpl_h
#define mozilla_dom_FontFaceSetDocumentImpl_h

#include "mozilla/dom/FontFaceSetImpl.h"
#include "nsICSSLoaderObserver.h"
#include "nsIDOMEventListener.h"

namespace mozilla::dom {

class FontFaceSetDocumentImpl final : public FontFaceSetImpl,
                                      public nsIDOMEventListener,
                                      public nsICSSLoaderObserver {
  NS_DECL_ISUPPORTS_INHERITED

 public:
  NS_DECL_NSIDOMEVENTLISTENER

  FontFaceSetDocumentImpl(FontFaceSet* aOwner, dom::Document* aDocument);

  void Initialize();
  void Destroy() override;

  bool IsOnOwningThread() override;
  void DispatchToOwningThread(const char* aName,
                              std::function<void()>&& aFunc) override;

  void RefreshStandardFontLoadPrincipal() override;

  dom::Document* GetDocument() const override { return mDocument; }

  already_AddRefed<URLExtraData> GetURLExtraData() override;

  // gfxUserFontSet

  nsresult StartLoad(gfxUserFontEntry* aUserFontEntry,
                     uint32_t aSrcIndex) override;

  bool IsFontLoadAllowed(const gfxFontFaceSrc&) override;

  nsPresContext* GetPresContext() const override;

  bool UpdateRules(const nsTArray<nsFontFaceRuleContainer>& aRules) override;

  RawServoFontFaceRule* FindRuleForEntry(gfxFontEntry* aFontEntry) override;

  /**
   * Notification method called by the nsPresContext to indicate that the
   * refresh driver ticked and flushed style and layout.
   * were just flushed.
   */
  void DidRefresh() override;

  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(StyleSheet* aSheet, bool aWasDeferred,
                              nsresult aStatus) override;

  // For ServoStyleSet to know ahead of time whether a font is loadable.
  void CacheFontLoadability() override;

  void EnsureReady() override;

  bool Add(FontFaceImpl* aFontFace, ErrorResult& aRv) override;

  void FlushUserFontSet() override;
  void MarkUserFontSetDirty() override;

 private:
  ~FontFaceSetDocumentImpl() override;

  uint64_t GetInnerWindowID() override;

  void RemoveDOMContentLoadedListener();

  nsresult CreateChannelForSyncLoadFontData(
      nsIChannel** aOutChannel, gfxUserFontEntry* aFontToLoad,
      const gfxFontFaceSrc* aFontFaceSrc) override;

  // search for @font-face rule that matches a userfont font entry
  RawServoFontFaceRule* FindRuleForUserFontEntry(
      gfxUserFontEntry* aUserFontEntry) override;

  void FindMatchingFontFaces(const nsTHashSet<FontFace*>& aMatchingFaces,
                             nsTArray<FontFace*>& aFontFaces) override;

  void InsertRuleFontFace(FontFaceImpl* aFontFace, FontFace* aFontFaceOwner,
                          StyleOrigin aOrigin,
                          nsTArray<FontFaceRecord>& aOldRecords,
                          bool& aFontSetModified);

  // Helper function for HasLoadingFontFaces.
  void UpdateHasLoadingFontFaces() override;

  /**
   * Returns whether there might be any pending font loads, which should cause
   * the mReady Promise not to be resolved yet.
   */
  bool MightHavePendingFontLoads() override;

#ifdef DEBUG
  bool HasRuleFontFace(FontFaceImpl* aFontFace);
#endif

  TimeStamp GetNavigationStartTimeStamp() override;

  // The document this is a FontFaceSet for.
  RefPtr<dom::Document> mDocument;

  // The @font-face rule backed FontFace objects in the FontFaceSet.
  nsTArray<FontFaceRecord> mRuleFaces;
};

}  // namespace mozilla::dom

#endif  // !defined(mozilla_dom_FontFaceSetDocumentImpl_h)
