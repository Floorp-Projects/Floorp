/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIDocumentObserver_h___
#define nsIDocumentObserver_h___

#include "mozilla/EventStates.h"
#include "nsISupports.h"
#include "nsIMutationObserver.h"

class nsIContent;
class nsIStyleSheet;
class nsIStyleRule;
class nsIDocument;

#define NS_IDOCUMENT_OBSERVER_IID \
{ 0x900bc4bc, 0x8b6c, 0x4cba, \
 { 0x82, 0xfa, 0x56, 0x8a, 0x80, 0xff, 0xfd, 0x3e } }

typedef uint32_t nsUpdateType;

#define UPDATE_CONTENT_MODEL 0x00000001
#define UPDATE_STYLE         0x00000002
#define UPDATE_ALL (UPDATE_CONTENT_MODEL | UPDATE_STYLE)

// Document observer interface
class nsIDocumentObserver : public nsIMutationObserver
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOCUMENT_OBSERVER_IID)

  /**
   * Notify that a content model update is beginning. This call can be
   * nested.
   */
  virtual void BeginUpdate(nsIDocument *aDocument,
                           nsUpdateType aUpdateType) = 0;

  /**
   * Notify that a content model update is finished. This call can be
   * nested.
   */
  virtual void EndUpdate(nsIDocument *aDocument, nsUpdateType aUpdateType) = 0;

  /**
   * Notify the observer that a document load is beginning.
   */
  virtual void BeginLoad(nsIDocument *aDocument) = 0;

  /**
   * Notify the observer that a document load has finished. Note that
   * the associated reflow of the document will be done <b>before</b>
   * EndLoad is invoked, not after.
   */
  virtual void EndLoad(nsIDocument *aDocument) = 0;

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
  virtual void ContentStateChanged(nsIDocument* aDocument,
                                   nsIContent* aContent,
                                   mozilla::EventStates aStateMask) = 0;

  /**
   * Notification that the state of the document has changed.
   *
   * @param aDocument The document being observed
   * @param aStateMask the state that changed
   */
  virtual void DocumentStatesChanged(nsIDocument* aDocument,
                                     mozilla::EventStates aStateMask) = 0;

  /**
   * A StyleSheet has just been added to the document.  This method is
   * called automatically when a StyleSheet gets added to the
   * document, even if the stylesheet is not applicable. The
   * notification is passed on to all of the document observers.   
   *
   * @param aDocument The document being observed
   * @param aStyleSheet the StyleSheet that has been added
   * @param aDocumentSheet True if sheet is in document's style sheet list,
   *                       false if sheet is not (i.e., UA or user sheet)
   */
  virtual void StyleSheetAdded(nsIDocument *aDocument,
                               nsIStyleSheet* aStyleSheet,
                               bool aDocumentSheet) = 0;

  /**
   * A StyleSheet has just been removed from the document.  This
   * method is called automatically when a StyleSheet gets removed
   * from the document, even if the stylesheet is not applicable. The
   * notification is passed on to all of the document observers.
   *
   * @param aDocument The document being observed
   * @param aStyleSheet the StyleSheet that has been removed
   * @param aDocumentSheet True if sheet is in document's style sheet list,
   *                       false if sheet is not (i.e., UA or user sheet)
   */
  virtual void StyleSheetRemoved(nsIDocument *aDocument,
                                 nsIStyleSheet* aStyleSheet,
                                 bool aDocumentSheet) = 0;
  
  /**
   * A StyleSheet has just changed its applicable state.
   * This method is called automatically when the applicable state
   * of a StyleSheet gets changed. The style sheet passes this
   * notification to the document. The notification is passed on 
   * to all of the document observers.
   *
   * @param aDocument The document being observed
   * @param aStyleSheet the StyleSheet that has changed state
   * @param aApplicable true if the sheet is applicable, false if
   *        it is not applicable
   */
  virtual void StyleSheetApplicableStateChanged(nsIDocument *aDocument,
                                                nsIStyleSheet* aStyleSheet,
                                                bool aApplicable) = 0;

  /**
   * A StyleRule has just been modified within a style sheet.
   * This method is called automatically when the rule gets
   * modified. The style sheet passes this notification to 
   * the document. The notification is passed on to all of 
   * the document observers.
   *
   * Since nsIStyleRule objects are immutable, there is a new object
   * replacing the old one.  However, the use of this method (rather
   * than StyleRuleAdded and StyleRuleRemoved) implies that the new rule
   * matches the same elements and has the same priority (weight,
   * origin, specificity) as the old one.  (However, if it is a CSS
   * style rule, there may be a change in whether it has an important
   * rule.)
   *
   * @param aDocument The document being observed
   * @param aStyleSheet the StyleSheet that contians the rule
   * @param aOldStyleRule The rule being removed.  This rule may not be
   *                      fully valid anymore -- however, it can still
   *                      be used for pointer comparison and
   *                      |QueryInterface|.
   * @param aNewStyleRule The rule being added.
   */
  virtual void StyleRuleChanged(nsIDocument *aDocument,
                                nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aOldStyleRule,
                                nsIStyleRule* aNewStyleRule) = 0;

  /**
   * A StyleRule has just been added to a style sheet.
   * This method is called automatically when the rule gets
   * added to the sheet. The style sheet passes this
   * notification to the document. The notification is passed on 
   * to all of the document observers.
   *
   * @param aDocument The document being observed
   * @param aStyleSheet the StyleSheet that has been modified
   * @param aStyleRule the rule that was added
   */
  virtual void StyleRuleAdded(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) = 0;

  /**
   * A StyleRule has just been removed from a style sheet.
   * This method is called automatically when the rule gets
   * removed from the sheet. The style sheet passes this
   * notification to the document. The notification is passed on 
   * to all of the document observers.
   *
   * @param aDocument The document being observed
   * @param aStyleSheet the StyleSheet that has been modified
   * @param aStyleRule the rule that was removed
   */
  virtual void StyleRuleRemoved(nsIDocument *aDocument,
                                nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDocumentObserver, NS_IDOCUMENT_OBSERVER_IID)

#define NS_DECL_NSIDOCUMENTOBSERVER_BEGINUPDATE                              \
    virtual void BeginUpdate(nsIDocument* aDocument,                         \
                             nsUpdateType aUpdateType) override;

#define NS_DECL_NSIDOCUMENTOBSERVER_ENDUPDATE                                \
    virtual void EndUpdate(nsIDocument* aDocument, nsUpdateType aUpdateType) override;

#define NS_DECL_NSIDOCUMENTOBSERVER_BEGINLOAD                                \
    virtual void BeginLoad(nsIDocument* aDocument) override;

#define NS_DECL_NSIDOCUMENTOBSERVER_ENDLOAD                                  \
    virtual void EndLoad(nsIDocument* aDocument) override;

#define NS_DECL_NSIDOCUMENTOBSERVER_CONTENTSTATECHANGED                      \
    virtual void ContentStateChanged(nsIDocument* aDocument,                 \
                                     nsIContent* aContent,                   \
                                     mozilla::EventStates aStateMask) override;

#define NS_DECL_NSIDOCUMENTOBSERVER_DOCUMENTSTATESCHANGED                    \
    virtual void DocumentStatesChanged(nsIDocument* aDocument,               \
                                       mozilla::EventStates aStateMask) override;

#define NS_DECL_NSIDOCUMENTOBSERVER_STYLESHEETADDED                          \
    virtual void StyleSheetAdded(nsIDocument* aDocument,                     \
                                 nsIStyleSheet* aStyleSheet,                 \
                                 bool aDocumentSheet) override;

#define NS_DECL_NSIDOCUMENTOBSERVER_STYLESHEETREMOVED                        \
    virtual void StyleSheetRemoved(nsIDocument* aDocument,                   \
                                   nsIStyleSheet* aStyleSheet,               \
                                   bool aDocumentSheet) override;

#define NS_DECL_NSIDOCUMENTOBSERVER_STYLESHEETAPPLICABLESTATECHANGED         \
    virtual void StyleSheetApplicableStateChanged(nsIDocument* aDocument,    \
                                                  nsIStyleSheet* aStyleSheet,\
                                                  bool aApplicable) override;

#define NS_DECL_NSIDOCUMENTOBSERVER_STYLERULECHANGED                         \
    virtual void StyleRuleChanged(nsIDocument* aDocument,                    \
                                  nsIStyleSheet* aStyleSheet,                \
                                  nsIStyleRule* aOldStyleRule,               \
                                  nsIStyleRule* aNewStyleRule) override;

#define NS_DECL_NSIDOCUMENTOBSERVER_STYLERULEADDED                           \
    virtual void StyleRuleAdded(nsIDocument* aDocument,                      \
                                nsIStyleSheet* aStyleSheet,                  \
                                nsIStyleRule* aStyleRule) override;

#define NS_DECL_NSIDOCUMENTOBSERVER_STYLERULEREMOVED                         \
    virtual void StyleRuleRemoved(nsIDocument* aDocument,                    \
                                  nsIStyleSheet* aStyleSheet,                \
                                  nsIStyleRule* aStyleRule) override;

#define NS_DECL_NSIDOCUMENTOBSERVER                                          \
    NS_DECL_NSIDOCUMENTOBSERVER_BEGINUPDATE                                  \
    NS_DECL_NSIDOCUMENTOBSERVER_ENDUPDATE                                    \
    NS_DECL_NSIDOCUMENTOBSERVER_BEGINLOAD                                    \
    NS_DECL_NSIDOCUMENTOBSERVER_ENDLOAD                                      \
    NS_DECL_NSIDOCUMENTOBSERVER_CONTENTSTATECHANGED                          \
    NS_DECL_NSIDOCUMENTOBSERVER_DOCUMENTSTATESCHANGED                        \
    NS_DECL_NSIDOCUMENTOBSERVER_STYLESHEETADDED                              \
    NS_DECL_NSIDOCUMENTOBSERVER_STYLESHEETREMOVED                            \
    NS_DECL_NSIDOCUMENTOBSERVER_STYLESHEETAPPLICABLESTATECHANGED             \
    NS_DECL_NSIDOCUMENTOBSERVER_STYLERULECHANGED                             \
    NS_DECL_NSIDOCUMENTOBSERVER_STYLERULEADDED                               \
    NS_DECL_NSIDOCUMENTOBSERVER_STYLERULEREMOVED                             \
    NS_DECL_NSIMUTATIONOBSERVER


#define NS_IMPL_NSIDOCUMENTOBSERVER_CORE_STUB(_class)                     \
void                                                                      \
_class::BeginUpdate(nsIDocument* aDocument, nsUpdateType aUpdateType)     \
{                                                                         \
}                                                                         \
void                                                                      \
_class::EndUpdate(nsIDocument* aDocument, nsUpdateType aUpdateType)       \
{                                                                         \
}                                                                         \
NS_IMPL_NSIMUTATIONOBSERVER_CORE_STUB(_class)

#define NS_IMPL_NSIDOCUMENTOBSERVER_LOAD_STUB(_class)                     \
void                                                                      \
_class::BeginLoad(nsIDocument* aDocument)                                 \
{                                                                         \
}                                                                         \
void                                                                      \
_class::EndLoad(nsIDocument* aDocument)                                   \
{                                                                         \
}

#define NS_IMPL_NSIDOCUMENTOBSERVER_STATE_STUB(_class)                    \
void                                                                      \
_class::ContentStateChanged(nsIDocument* aDocument,                       \
                            nsIContent* aContent,                         \
                            mozilla::EventStates aStateMask)              \
{                                                                         \
}                                                                         \
                                                                          \
void                                                                      \
_class::DocumentStatesChanged(nsIDocument* aDocument,                     \
                              mozilla::EventStates aStateMask)            \
{                                                                         \
}

#define NS_IMPL_NSIDOCUMENTOBSERVER_CONTENT(_class)                       \
NS_IMPL_NSIMUTATIONOBSERVER_CONTENT(_class)

#define NS_IMPL_NSIDOCUMENTOBSERVER_STYLE_STUB(_class)                    \
void                                                                      \
_class::StyleSheetAdded(nsIDocument* aDocument,                           \
                        nsIStyleSheet* aStyleSheet,                       \
                        bool aDocumentSheet)                            \
{                                                                         \
}                                                                         \
void                                                                      \
_class::StyleSheetRemoved(nsIDocument* aDocument,                         \
                          nsIStyleSheet* aStyleSheet,                     \
                          bool aDocumentSheet)                          \
{                                                                         \
}                                                                         \
void                                                                      \
_class::StyleSheetApplicableStateChanged(nsIDocument* aDocument,          \
                                         nsIStyleSheet* aStyleSheet,      \
                                         bool aApplicable)              \
{                                                                         \
}                                                                         \
void                                                                      \
_class::StyleRuleChanged(nsIDocument* aDocument,                          \
                         nsIStyleSheet* aStyleSheet,                      \
                         nsIStyleRule* aOldStyleRule,                     \
                         nsIStyleRule* aNewStyleRule)                     \
{                                                                         \
}                                                                         \
void                                                                      \
_class::StyleRuleAdded(nsIDocument* aDocument,                            \
                       nsIStyleSheet* aStyleSheet,                        \
                       nsIStyleRule* aStyleRule)                          \
{                                                                         \
}                                                                         \
void                                                                      \
_class::StyleRuleRemoved(nsIDocument* aDocument,                          \
                         nsIStyleSheet* aStyleSheet,                      \
                         nsIStyleRule* aStyleRule)                        \
{                                                                         \
}

#endif /* nsIDocumentObserver_h___ */
