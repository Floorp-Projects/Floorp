/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMEventTarget.h"
#include "nsIHttpChannel.h"

namespace mozilla {
namespace dom {

class WebVTTListener;

class HTMLTrackElement MOZ_FINAL : public nsGenericHTMLElement
{
public:
  HTMLTrackElement(already_AddRefed<nsINodeInfo>& aNodeInfo);
  virtual ~HTMLTrackElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLTrackElement,
                                           nsGenericHTMLElement)

  // HTMLTrackElement WebIDL
  void GetKind(DOMString& aKind) const;
  void SetKind(const nsAString& aKind, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::kind, aKind, aError);
  }

  void GetSrc(DOMString& aSrc) const
  {
    GetHTMLURIAttr(nsGkAtoms::src, aSrc);
  }
  void SetSrc(const nsAString& aSrc, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::src, aSrc, aError);
  }

  void GetSrclang(DOMString& aSrclang) const
  {
    GetHTMLAttr(nsGkAtoms::srclang, aSrclang);
  }
  void GetSrclang(nsString& aSrclang) const
  {
    GetHTMLAttr(nsGkAtoms::srclang, aSrclang);
  }
  void SetSrclang(const nsAString& aSrclang, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::srclang, aSrclang, aError);
  }

  void GetLabel(DOMString& aLabel) const
  {
    GetHTMLAttr(nsGkAtoms::label, aLabel);
  }
  void GetLabel(nsString& aLabel) const
  {
    GetHTMLAttr(nsGkAtoms::label, aLabel);
  }
  void SetLabel(const nsAString& aLabel, ErrorResult& aError)
  {
    SetHTMLAttr(nsGkAtoms::label, aLabel, aError);
  }

  bool Default() const
  {
    return GetBoolAttr(nsGkAtoms::_default);
  }
  void SetDefault(bool aDefault, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::_default, aDefault, aError);
  }

  uint16_t ReadyState() const;

  TextTrack* Track();

  virtual nsresult Clone(nsINodeInfo* aNodeInfo, nsINode** aResult) const MOZ_OVERRIDE;

  // For Track, ItemValue reflects the src attribute
  virtual void GetItemValueText(nsAString& aText) MOZ_OVERRIDE
  {
    DOMString value;
    GetSrc(value);
    aText = value;
  }
  virtual void SetItemValueText(const nsAString& aText) MOZ_OVERRIDE
  {
    ErrorResult rv;
    SetSrc(aText, rv);
  }

  // Override ParseAttribute() to convert kind strings to enum values.
  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult) MOZ_OVERRIDE;

  // Override BindToTree() so that we can trigger a load when we become
  // the child of a media element.
  virtual nsresult BindToTree(nsIDocument* aDocument,
                              nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) MOZ_OVERRIDE;
  virtual void UnbindFromTree(bool aDeep, bool aNullParent) MOZ_OVERRIDE;

  // Check enabling preference.
  static bool IsWebVTTEnabled();

protected:
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
  void OnChannelRedirect(nsIChannel* aChannel, nsIChannel* aNewChannel,
                         uint32_t aFlags);
  // Open a new channel to the HTMLTrackElement's src attribute and call
  // mListener's LoadResource().
  void LoadResource();

  friend class TextTrackCue;
  friend class WebVTTListener;

  nsRefPtr<TextTrack> mTrack;
  nsCOMPtr<nsIChannel> mChannel;
  nsRefPtr<HTMLMediaElement> mMediaParent;
  nsRefPtr<WebVTTListener> mListener;

  void CreateTextTrack();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLTrackElement_h
