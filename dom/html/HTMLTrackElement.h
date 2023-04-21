/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLTrackElement_h
#define mozilla_dom_HTMLTrackElement_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/TextTrack.h"
#include "nsCycleCollectionParticipant.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"

class nsIContent;

namespace mozilla::dom {

class WebVTTListener;
class WindowDestroyObserver;
enum class TextTrackReadyState : uint8_t;

class HTMLTrackElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLTrackElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLTrackElement,
                                           nsGenericHTMLElement)

  // HTMLTrackElement WebIDL
  void GetKind(DOMString& aKind) const;
  void SetKind(const nsAString& aKind, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::kind, aKind, aError);
  }

  void GetSrc(DOMString& aSrc) const { GetHTMLURIAttr(nsGkAtoms::src, aSrc); }

  void SetSrc(const nsAString& aSrc, ErrorResult& aError);

  void GetSrclang(DOMString& aSrclang) const {
    GetHTMLAttr(nsGkAtoms::srclang, aSrclang);
  }
  void GetSrclang(nsAString& aSrclang) const {
    GetHTMLAttr(nsGkAtoms::srclang, aSrclang);
  }
  void SetSrclang(const nsAString& aSrclang, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::srclang, aSrclang, aError);
  }

  void GetLabel(DOMString& aLabel) const {
    GetHTMLAttr(nsGkAtoms::label, aLabel);
  }
  void GetLabel(nsAString& aLabel) const {
    GetHTMLAttr(nsGkAtoms::label, aLabel);
  }
  void SetLabel(const nsAString& aLabel, ErrorResult& aError) {
    SetHTMLAttr(nsGkAtoms::label, aLabel, aError);
  }

  bool Default() const { return GetBoolAttr(nsGkAtoms::_default); }
  void SetDefault(bool aDefault, ErrorResult& aError) {
    SetHTMLBoolAttr(nsGkAtoms::_default, aDefault, aError);
  }

  TextTrackReadyState ReadyState() const;
  uint16_t ReadyStateForBindings() const {
    return static_cast<uint16_t>(ReadyState());
  }
  void SetReadyState(TextTrackReadyState);

  TextTrack* GetTrack();

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // Override ParseAttribute() to convert kind strings to enum values.
  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;

  // Override BindToTree() so that we can trigger a load when we become
  // the child of a media element.
  virtual nsresult BindToTree(BindContext&, nsINode& aParent) override;
  virtual void UnbindFromTree(bool aNullParent) override;

  virtual void AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                            const nsAttrValue* aValue,
                            const nsAttrValue* aOldValue,
                            nsIPrincipal* aMaybeScriptedPrincipal,
                            bool aNotify) override;

  void DispatchTrackRunnable(const nsString& aEventName);
  void DispatchTrustedEvent(const nsAString& aName);
  void DispatchTestEvent(const nsAString& aName);

  void CancelChannelAndListener();

  // Only load resource for the non-disabled track with media parent.
  void MaybeDispatchLoadResource();

 protected:
  virtual ~HTMLTrackElement();

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;
  void OnChannelRedirect(nsIChannel* aChannel, nsIChannel* aNewChannel,
                         uint32_t aFlags);

  friend class TextTrackCue;
  friend class WebVTTListener;

  RefPtr<TextTrack> mTrack;
  nsCOMPtr<nsIChannel> mChannel;
  RefPtr<HTMLMediaElement> mMediaParent;
  RefPtr<WebVTTListener> mListener;

  void CreateTextTrack();

 private:
  // Open a new channel to the HTMLTrackElement's src attribute and call
  // mListener's LoadResource().
  void LoadResource(RefPtr<WebVTTListener>&& aWebVTTListener);
  bool mLoadResourceDispatched;

  void MaybeClearAllCues();

  RefPtr<WindowDestroyObserver> mWindowDestroyObserver;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_HTMLTrackElement_h
