/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsIDocumentObserver_h___
#define nsIDocumentObserver_h___

#include "nsISupports.h"
#include "nsChangeHint.h"

class nsIAtom;
class nsIContent;
class nsIPresShell;
class nsIStyleSheet;
class nsIStyleRule;
class nsString;
class nsIDocument;

#define NS_IDOCUMENT_OBSERVER_IID \
{ 0xb3f92460, 0x944c, 0x11d1, {0x93, 0x23, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

// Document observer interface
class nsIDocumentObserver : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOCUMENT_OBSERVER_IID)

  /**
   * Notify that a content model update is beginning. This call can be
   * nested.
   */
  NS_IMETHOD BeginUpdate(nsIDocument *aDocument) = 0;

  /**
   * Notify that a content model update is finished. This call can be
   * nested.
   */
  NS_IMETHOD EndUpdate(nsIDocument *aDocument) = 0;

  /**
   * Notify the observer that a document load is beginning.
   */
  NS_IMETHOD BeginLoad(nsIDocument *aDocument) = 0;

  /**
   * Notify the observer that a document load has finished. Note that
   * the associated reflow of the document will be done <b>before</b>
   * EndLoad is invoked, not after.
   */
  NS_IMETHOD EndLoad(nsIDocument *aDocument) = 0;

  /**
   * Notify the observer that the document is being reflowed in
   * the given presentation shell.
   */
  NS_IMETHOD BeginReflow(nsIDocument *aDocument, nsIPresShell* aShell) = 0;

  /**
   * Notify the observer that the document is done being reflowed in
   * the given presentation shell.
   */
  NS_IMETHOD EndReflow(nsIDocument *aDocument, nsIPresShell* aShell) = 0;

  /**
   * Notification that the content model has changed. This method is
   * called automatically by content objects when their state is changed
   * (therefore there is normally no need to invoke this method
   * directly).  The notification is passed to any
   * IDocumentObservers. The notification is passed on to all of the
   * document observers. <p>
   *
   * This notification is not sent when a piece of content is
   * added/removed from the document (the other notifications are used
   * for that).
   *
   * @param aDocument The document being observed
   * @param aContent the piece of content that changed
   * @param aSubContent subrange information about the piece of content
   *  that changed
   */
  NS_IMETHOD ContentChanged(nsIDocument *aDocument,
                            nsIContent* aContent,
                            nsISupports* aSubContent) = 0;

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
   * The optional second content node is to allow optimization
   * of the case where state moves from one node to another
   * (as is likely for :focus and :hover)
   *
   * Either content node may be nsnull, but not both
   *
   * @param aDocument The document being observed
   * @param aContent1 the piece of content that changed
   * @param aContent2 optional second piece of content that changed
   */
  NS_IMETHOD ContentStatesChanged(nsIDocument* aDocument,
                                  nsIContent* aContent1,
                                  nsIContent* aContent2,
                                  PRInt32 aStateMask) = 0;

  /**
   * Notification that the content model has changed. This method is called
   * automatically by content objects when an attribute's value has changed
   * (therefore there is normally no need to invoke this method directly). The
   * notification is passed to any IDocumentObservers document observers. <p>
   *
   * @param aDocument The document being observed
   * @param aContent the piece of content whose attribute changed
   * @param aAttribute the atom name of the attribute
   * @param aModType Whether or not the attribute was added, changed, or removed.
   *   The constants are defined in nsIDOMMutationEvent.h.
   * @param aHint The style hint.
   */
  NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                              nsIContent*  aContent,
                              PRInt32      aNameSpaceID,
                              nsIAtom*     aAttribute,
                              PRInt32      aModType,    
                              nsChangeHint aHint) = 0;

  /**
   * Notifcation that the content model has had data appended to the
   * given content object. This method is called automatically by the
   * content container objects when a new content object is appended to
   * the container (therefore there is normally no need to invoke this
   * method directly). The notification is passed on to all of the
   * document observers.
   *
   * @param aDocument The document being observed
   * @param aContainer the container that had a new child appended
   * @param aNewIndexInContainer the index in the container of the first
   *          new child
   */
  NS_IMETHOD ContentAppended(nsIDocument *aDocument,
                             nsIContent* aContainer,
                             PRInt32     aNewIndexInContainer) = 0;

  /**
   * Notification that content has been inserted. This method is called
   * automatically by the content container objects when a new content
   * object is inserted in the container (therefore there is normally no
   * need to invoke this method directly). The notification is passed on
   * to all of the document observers.
   *
   * @param aDocument The document being observed
   * @param aContainer the container that now contains aChild
   * @param aChild the child that was inserted
   * @param aIndexInContainer the index of the child in the container
   */
  NS_IMETHOD ContentInserted(nsIDocument *aDocument,
                             nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer) = 0;

  /**
   * Notification that content has been replaced. This method is called
   * automatically by the content container objects when a content object
   * is replaced in the container (therefore there is normally no need to
   * invoke this method directly). The notification is passed on to all
   * of the document observers.
   *
   * @param aDocument The document being observed
   * @param aContainer the container that now contains aChild
   * @param aOldChild the child that was replaced
   * @param aNewChild the child that replaced aOldChild
   * @param aIndexInContainer the index of the old and new child in the
   *  container
   */
  NS_IMETHOD ContentReplaced(nsIDocument *aDocument,
                             nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer) = 0;

  /**
   * Content has just been removed. This method is called automatically
   * by content container objects when a content object has just been
   * removed from the container (therefore there is normally no need to
   * invoke this method directly). The notification is passed on to all
   * of the document observers.
   *
   * @param aDocument The document being observed
   * @param aContainer the container that had a child removed
   * @param aChild the child that was just removed
   * @param aIndexInContainer the index of the child in the container
   *  before it was removed
   */
  NS_IMETHOD ContentRemoved(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer) = 0;

  /**
   * A StyleSheet has just been added to the document.  This method is
   * called automatically when a StyleSheet gets added to the
   * document, even if the stylesheet is not applicable. The
   * notification is passed on to all of the document observers.   
   *
   * @param aDocument The document being observed
   * @param aStyleSheet the StyleSheet that has been added
   */
  NS_IMETHOD StyleSheetAdded(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet) = 0;

  /**
   * A StyleSheet has just been removed from the document.  This
   * method is called automatically when a StyleSheet gets removed
   * from the document, even if the stylesheet is not applicable. The
   * notification is passed on to all of the document observers.
   *
   * @param aDocument The document being observed
   * @param aStyleSheet the StyleSheet that has been removed
   */
  NS_IMETHOD StyleSheetRemoved(nsIDocument *aDocument,
                               nsIStyleSheet* aStyleSheet) = 0;
  
  /**
   * A StyleSheet has just changed its applicable state.
   * This method is called automatically when the applicable state
   * of a StyleSheet gets changed. The style sheet passes this
   * notification to the document. The notification is passed on 
   * to all of the document observers.
   *
   * @param aDocument The document being observed
   * @param aStyleSheet the StyleSheet that has changed state
   * @param aApplicable PR_TRUE if the sheet is applicable, PR_FALSE if
   *        it is not applicable
   */
  NS_IMETHOD StyleSheetApplicableStateChanged(nsIDocument *aDocument,
                                              nsIStyleSheet* aStyleSheet,
                                              PRBool aApplicable) = 0;

  /**
   * A StyleRule has just been modified within a style sheet.
   * This method is called automatically when the rule gets
   * modified. The style sheet passes this notification to 
   * the document. The notification is passed on to all of 
   * the document observers.
   *
   * @param aDocument The document being observed
   * @param aStyleSheet the StyleSheet that contians the rule
   * @param aStyleRule the rule that was modified
   * @param aHint some possible info about the nature of the change
   */
  NS_IMETHOD StyleRuleChanged(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              nsChangeHint aHint) = 0;

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
  NS_IMETHOD StyleRuleAdded(nsIDocument *aDocument,
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
  NS_IMETHOD StyleRuleRemoved(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) = 0;

 /**
   * The document is in the process of being destroyed.
   * This method is called automatically during document
   * destruction.
   * 
   * @param aDocument The document being observed
   */
  NS_IMETHOD DocumentWillBeDestroyed(nsIDocument *aDocument) = 0;
};

#define NS_DECL_NSIDOCUMENTOBSERVER                                          \
    NS_IMETHOD BeginUpdate(nsIDocument* aDocument);                          \
    NS_IMETHOD EndUpdate(nsIDocument* aDocument);                            \
    NS_IMETHOD BeginLoad(nsIDocument* aDocument);                            \
    NS_IMETHOD EndLoad(nsIDocument* aDocument);                              \
    NS_IMETHOD BeginReflow(nsIDocument* aDocument,                           \
                           nsIPresShell* aShell);                            \
    NS_IMETHOD EndReflow(nsIDocument* aDocument,                             \
                         nsIPresShell* aShell);                              \
    NS_IMETHOD ContentChanged(nsIDocument* aDocument,                        \
                              nsIContent* aContent,                          \
                              nsISupports* aSubContent);                     \
    NS_IMETHOD ContentStatesChanged(nsIDocument* aDocument,                  \
                                    nsIContent* aContent1,                   \
                                    nsIContent* aContent2,                   \
                                    PRInt32 aStateMask);                     \
    NS_IMETHOD AttributeChanged(nsIDocument* aDocument,                      \
                                nsIContent* aContent,                        \
                                PRInt32 aNameSpaceID,                        \
                                nsIAtom* aAttribute,                         \
                                PRInt32 aModType,                            \
                                nsChangeHint aHint);                         \
    NS_IMETHOD ContentAppended(nsIDocument* aDocument,                       \
                               nsIContent* aContainer,                       \
                               PRInt32 aNewIndexInContainer);                \
    NS_IMETHOD ContentInserted(nsIDocument* aDocument,                       \
                               nsIContent* aContainer,                       \
                               nsIContent* aChild,                           \
                               PRInt32 aIndexInContainer);                   \
    NS_IMETHOD ContentReplaced(nsIDocument* aDocument,                       \
                               nsIContent* aContainer,                       \
                               nsIContent* aOldChild,                        \
                               nsIContent* aNewChild,                        \
                               PRInt32 aIndexInContainer);                   \
    NS_IMETHOD ContentRemoved(nsIDocument* aDocument,                        \
                              nsIContent* aContainer,                        \
                              nsIContent* aChild,                            \
                              PRInt32 aIndexInContainer);                    \
    NS_IMETHOD StyleSheetAdded(nsIDocument* aDocument,                       \
                               nsIStyleSheet* aStyleSheet);                  \
    NS_IMETHOD StyleSheetRemoved(nsIDocument* aDocument,                     \
                                 nsIStyleSheet* aStyleSheet);                \
    NS_IMETHOD StyleSheetApplicableStateChanged(nsIDocument* aDocument,      \
                                                nsIStyleSheet* aStyleSheet,  \
                                                PRBool aApplicable);         \
    NS_IMETHOD StyleRuleChanged(nsIDocument* aDocument,                      \
                                nsIStyleSheet* aStyleSheet,                  \
                                nsIStyleRule* aStyleRule,                    \
                                nsChangeHint aHint);                         \
    NS_IMETHOD StyleRuleAdded(nsIDocument* aDocument,                        \
                              nsIStyleSheet* aStyleSheet,                    \
                              nsIStyleRule* aStyleRule);                     \
    NS_IMETHOD StyleRuleRemoved(nsIDocument* aDocument,                      \
                                nsIStyleSheet* aStyleSheet,                  \
                                nsIStyleRule* aStyleRule);                   \
    NS_IMETHOD DocumentWillBeDestroyed(nsIDocument* aDocument);              \


#define NS_IMPL_NSIDOCUMENTOBSERVER_CORE_STUB(_class)                     \
NS_IMETHODIMP                                                             \
_class::BeginUpdate(nsIDocument* aDocument)                               \
{                                                                         \
  return NS_OK;                                                           \
}                                                                         \
NS_IMETHODIMP                                                             \
_class::EndUpdate(nsIDocument* aDocument)                                 \
{                                                                         \
  return NS_OK;                                                           \
}                                                                         \
NS_IMETHODIMP                                                             \
_class::DocumentWillBeDestroyed(nsIDocument* aDocument)                   \
{                                                                         \
  return NS_OK;                                                           \
}

#define NS_IMPL_NSIDOCUMENTOBSERVER_LOAD_STUB(_class)                     \
NS_IMETHODIMP                                                             \
_class::BeginLoad(nsIDocument* aDocument)                                 \
{                                                                         \
  return NS_OK;                                                           \
}                                                                         \
NS_IMETHODIMP                                                             \
_class::EndLoad(nsIDocument* aDocument)                                   \
{                                                                         \
  return NS_OK;                                                           \
}

#define NS_IMPL_NSIDOCUMENTOBSERVER_REFLOW_STUB(_class)                   \
NS_IMETHODIMP                                                             \
_class::BeginReflow(nsIDocument* aDocument,                               \
                                  nsIPresShell* aShell)                   \
{                                                                         \
  return NS_OK;                                                           \
}                                                                         \
NS_IMETHODIMP                                                             \
_class::EndReflow(nsIDocument* aDocument,                                 \
                                nsIPresShell* aShell)                     \
{                                                                         \
  return NS_OK;                                                           \
}

#define NS_IMPL_NSIDOCUMENTOBSERVER_STATE_STUB(_class)                    \
NS_IMETHODIMP                                                             \
_class::ContentStatesChanged(nsIDocument* aDocument,                      \
                                           nsIContent* aContent1,         \
                                           nsIContent* aContent2,         \
                                           PRInt32 aStateMask)            \
{                                                                         \
  return NS_OK;                                                           \
}

#define NS_IMPL_NSIDOCUMENTOBSERVER_CONTENT(_class)                       \
NS_IMETHODIMP                                                             \
_class::ContentChanged(nsIDocument* aDocument,                            \
                                     nsIContent* aContent,                \
                                     nsISupports* aSubContent)            \
{                                                                         \
  return NS_OK;                                                           \
}                                                                         \
NS_IMETHODIMP                                                             \
_class::AttributeChanged(nsIDocument* aDocument,                          \
                                       nsIContent* aContent,              \
                                       PRInt32 aNameSpaceID,              \
                                       nsIAtom* aAttribute,               \
                                       PRInt32 aModType,                  \
                                       nsChangeHint aHint)                \
{                                                                         \
  return NS_OK;                                                           \
}                                                                         \
NS_IMETHODIMP                                                             \
_class::ContentAppended(nsIDocument* aDocument,                           \
                                      nsIContent* aContainer,             \
                                      PRInt32 aNewIndexInContainer)       \
{                                                                         \
  return NS_OK;                                                           \
}                                                                         \
NS_IMETHODIMP                                                             \
_class::ContentInserted(nsIDocument* aDocument,                           \
                                      nsIContent* aContainer,             \
                                      nsIContent* aChild,                 \
                                      PRInt32 aIndexInContainer)          \
{                                                                         \
  return NS_OK;                                                           \
}                                                                         \
NS_IMETHODIMP                                                             \
_class::ContentReplaced(nsIDocument* aDocument,                           \
                                      nsIContent* aContainer,             \
                                      nsIContent* aOldChild,              \
                                      nsIContent* aNewChild,              \
                                      PRInt32 aIndexInContainer)          \
{                                                                         \
  return NS_OK;                                                           \
}                                                                         \
NS_IMETHODIMP                                                             \
_class::ContentRemoved(nsIDocument* aDocument,                            \
                                     nsIContent* aContainer,              \
                                     nsIContent* aChild,                  \
                                     PRInt32 aIndexInContainer)           \
{                                                                         \
  return NS_OK;                                                           \
}

#define NS_IMPL_NSIDOCUMENTOBSERVER_STYLE_STUB(_class)                    \
NS_IMETHODIMP                                                             \
_class::StyleSheetAdded(nsIDocument* aDocument,                           \
                                      nsIStyleSheet* aStyleSheet)         \
{                                                                         \
  return NS_OK;                                                           \
}                                                                         \
NS_IMETHODIMP                                                             \
_class::StyleSheetRemoved(nsIDocument* aDocument,                         \
                                        nsIStyleSheet* aStyleSheet)       \
{                                                                         \
  return NS_OK;                                                           \
}                                                                         \
NS_IMETHODIMP                                                             \
_class::StyleSheetApplicableStateChanged(nsIDocument* aDocument,          \
                                         nsIStyleSheet* aStyleSheet,      \
                                         PRBool aApplicable)              \
{                                                                         \
  return NS_OK;                                                           \
}                                                                         \
NS_IMETHODIMP                                                             \
_class::StyleRuleChanged(nsIDocument* aDocument,                          \
                                       nsIStyleSheet* aStyleSheet,        \
                                       nsIStyleRule* aStyleRule,          \
                                       nsChangeHint aHint)                \
{                                                                         \
  return NS_OK;                                                           \
}                                                                         \
NS_IMETHODIMP                                                             \
_class::StyleRuleAdded(nsIDocument* aDocument,                            \
                                     nsIStyleSheet* aStyleSheet,          \
                                     nsIStyleRule* aStyleRule)            \
{                                                                         \
  return NS_OK;                                                           \
}                                                                         \
NS_IMETHODIMP                                                             \
_class::StyleRuleRemoved(nsIDocument* aDocument,                          \
                                       nsIStyleSheet* aStyleSheet,        \
                                       nsIStyleRule* aStyleRule)          \
{                                                                         \
  return NS_OK;                                                           \
}

#endif /* nsIDocumentObserver_h___ */
