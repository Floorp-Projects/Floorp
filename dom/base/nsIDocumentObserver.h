/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIDocumentObserver_h___
#define nsIDocumentObserver_h___

#include "mozilla/EventStates.h"
#include "mozilla/StyleSheet.h"
#include "nsISupports.h"
#include "nsIMutationObserver.h"

class nsIContent;
namespace mozilla {
namespace dom {
class Document;
}
}  // namespace mozilla

#define NS_IDOCUMENT_OBSERVER_IID                    \
  {                                                  \
    0x71041fa3, 0x6dd7, 0x4cde, {                    \
      0xbb, 0x76, 0xae, 0xcc, 0x69, 0xe1, 0x75, 0x78 \
    }                                                \
  }

// Document observer interface
class nsIDocumentObserver : public nsIMutationObserver {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOCUMENT_OBSERVER_IID)

  /**
   * Notify that a content model update is beginning. This call can be
   * nested.
   */
  virtual void BeginUpdate(mozilla::dom::Document*) = 0;

  /**
   * Notify that a content model update is finished. This call can be
   * nested.
   */
  virtual void EndUpdate(mozilla::dom::Document*) = 0;

  /**
   * Notify the observer that a document load is beginning.
   */
  virtual void BeginLoad(mozilla::dom::Document*) = 0;

  /**
   * Notify the observer that a document load has finished. Note that
   * the associated reflow of the document will be done <b>before</b>
   * EndLoad is invoked, not after.
   */
  virtual void EndLoad(mozilla::dom::Document*) = 0;

  /**
   * Notification that the state of a content node has changed.
   * (ie: gained or lost focus, became active or hovered over)
   * This method is called automatically by content objects
   * when their state is changed (therefore there is normally
   * no need to invoke this method directly).  The notification
   * is passed to any IDocumentObservers. The notification is
   * passed on to all of the document observers. <p>
   *
   * This notification is not sent when a piece of content is
   * added/removed from the document or the content itself changed
   * (the other notifications are used for that).
   *
   * @param aDocument The document being observed
   * @param aContent the piece of content that changed
   */
  virtual void ContentStateChanged(mozilla::dom::Document*,
                                   nsIContent* aContent,
                                   mozilla::EventStates aStateMask) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDocumentObserver, NS_IDOCUMENT_OBSERVER_IID)

#define NS_DECL_NSIDOCUMENTOBSERVER_BEGINUPDATE \
  virtual void BeginUpdate(mozilla::dom::Document*) override;

#define NS_DECL_NSIDOCUMENTOBSERVER_ENDUPDATE \
  virtual void EndUpdate(mozilla::dom::Document*) override;

#define NS_DECL_NSIDOCUMENTOBSERVER_BEGINLOAD \
  virtual void BeginLoad(mozilla::dom::Document*) override;

#define NS_DECL_NSIDOCUMENTOBSERVER_ENDLOAD \
  virtual void EndLoad(mozilla::dom::Document*) override;

#define NS_DECL_NSIDOCUMENTOBSERVER_CONTENTSTATECHANGED     \
  virtual void ContentStateChanged(mozilla::dom::Document*, \
                                   nsIContent* aContent,    \
                                   mozilla::EventStates aStateMask) override;

#define NS_DECL_NSIDOCUMENTOBSERVER               \
  NS_DECL_NSIDOCUMENTOBSERVER_BEGINUPDATE         \
  NS_DECL_NSIDOCUMENTOBSERVER_ENDUPDATE           \
  NS_DECL_NSIDOCUMENTOBSERVER_BEGINLOAD           \
  NS_DECL_NSIDOCUMENTOBSERVER_ENDLOAD             \
  NS_DECL_NSIDOCUMENTOBSERVER_CONTENTSTATECHANGED \
  NS_DECL_NSIMUTATIONOBSERVER

#define NS_IMPL_NSIDOCUMENTOBSERVER_CORE_STUB(_class)  \
  void _class::BeginUpdate(mozilla::dom::Document*) {} \
  void _class::EndUpdate(mozilla::dom::Document*) {}   \
  NS_IMPL_NSIMUTATIONOBSERVER_CORE_STUB(_class)

#define NS_IMPL_NSIDOCUMENTOBSERVER_LOAD_STUB(_class) \
  void _class::BeginLoad(mozilla::dom::Document*) {}  \
  void _class::EndLoad(mozilla::dom::Document*) {}

#define NS_IMPL_NSIDOCUMENTOBSERVER_STATE_STUB(_class)      \
  void _class::ContentStateChanged(mozilla::dom::Document*, \
                                   nsIContent* aContent,    \
                                   mozilla::EventStates aStateMask) {}

#define NS_IMPL_NSIDOCUMENTOBSERVER_CONTENT(_class) \
  NS_IMPL_NSIMUTATIONOBSERVER_CONTENT(_class)

#endif /* nsIDocumentObserver_h___ */
