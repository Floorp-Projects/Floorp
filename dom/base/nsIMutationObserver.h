/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIMutationObserver_h
#define nsIMutationObserver_h

#include "nsISupports.h"

#include "mozilla/Assertions.h"

class nsAttrValue;
class nsAtom;
class nsIContent;
class nsINode;

namespace mozilla {
namespace dom {
class Element;
}  // namespace dom
}  // namespace mozilla

#define NS_IMUTATION_OBSERVER_IID                    \
  {                                                  \
    0x6d674c17, 0x0fbc, 0x4633, {                    \
      0x8f, 0x46, 0x73, 0x4e, 0x87, 0xeb, 0xf0, 0xc7 \
    }                                                \
  }

/**
 * Information details about a characterdata change.  Basically, we
 * view all changes as replacements of a length of text at some offset
 * with some other text (of possibly some other length).
 */
struct CharacterDataChangeInfo {
  /**
   * True if this character data change is just an append.
   */
  bool mAppend;

  /**
   * The offset in the text where the change occurred.
   */
  uint32_t mChangeStart;

  /**
   * The offset such that mChangeEnd - mChangeStart is equal to the length of
   * the text we removed. If this was a pure insert, append or a result of
   * `splitText()` this is equal to mChangeStart.
   */
  uint32_t mChangeEnd;

  uint32_t LengthOfRemovedText() const {
    MOZ_ASSERT(mChangeStart <= mChangeEnd);

    return mChangeEnd - mChangeStart;
  }

  /**
   * The length of the text that was inserted in place of the removed text.  If
   * this was a pure text removal, this is 0.
   */
  uint32_t mReplaceLength;

  /**
   * The net result is that mChangeStart characters at the beginning of the
   * text remained as they were.  The next mChangeEnd - mChangeStart characters
   * were removed, and mReplaceLength characters were inserted in their place.
   * The text that used to begin at mChangeEnd now begins at
   * mChangeStart + mReplaceLength.
   */

  struct MOZ_STACK_CLASS Details {
    enum {
      eMerge,  // two text nodes are merged as a result of normalize()
      eSplit   // a text node is split as a result of splitText()
    } mType;
    /**
     * For eMerge it's the text node that will be removed, for eSplit it's the
     * new text node.
     */
    nsIContent* MOZ_NON_OWNING_REF mNextSibling;
  };

  /**
   * Used for splitText() and normalize(), otherwise null.
   */
  Details* mDetails;
};

/**
 * Mutation observer interface
 *
 * See nsINode::AddMutationObserver, nsINode::RemoveMutationObserver for how to
 * attach or remove your observers.
 *
 * WARNING: During these notifications, you are not allowed to perform
 * any mutations to the current or any other document, or start a
 * network load.  If you need to perform such operations do that
 * during the _last_ nsIDocumentObserver::EndUpdate notification.  The
 * exception for this is ParentChainChanged, where mutations should be
 * done from an async event, as the notification might not be
 * surrounded by BeginUpdate/EndUpdate calls.
 */
class nsIMutationObserver : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IMUTATION_OBSERVER_IID)

  /**
   * Notification that the node value of a data node (text, cdata, pi, comment)
   * will be changed.
   *
   * This notification is not sent when a piece of content is
   * added/removed from the document (the other notifications are used
   * for that).
   *
   * @param aContent  The piece of content that changed. Is never null.
   * @param aInfo     The structure with information details about the change.
   *
   * @note Callers of this method might not hold a strong reference to the
   *       observer.  The observer is responsible for making sure it stays
   *       alive for the duration of the call as needed.  The observer may
   *       assume that this call will happen when there are script blockers on
   *       the stack.
   */
  virtual void CharacterDataWillChange(nsIContent* aContent,
                                       const CharacterDataChangeInfo&) = 0;

  /**
   * Notification that the node value of a data node (text, cdata, pi, comment)
   * has changed.
   *
   * This notification is not sent when a piece of content is
   * added/removed from the document (the other notifications are used
   * for that).
   *
   * @param aContent  The piece of content that changed. Is never null.
   * @param aInfo     The structure with information details about the change.
   *
   * @note Callers of this method might not hold a strong reference to the
   *       observer.  The observer is responsible for making sure it stays
   *       alive for the duration of the call as needed.  The observer may
   *       assume that this call will happen when there are script blockers on
   *       the stack.
   */
  virtual void CharacterDataChanged(nsIContent* aContent,
                                    const CharacterDataChangeInfo&) = 0;

  /**
   * Notification that an attribute of an element will change.  This
   * can happen before the BeginUpdate for the change and may not
   * always be followed by an AttributeChanged (in particular, if the
   * attribute doesn't actually change there will be no corresponding
   * AttributeChanged).
   *
   * @param aContent     The element whose attribute will change
   * @param aNameSpaceID The namespace id of the changing attribute
   * @param aAttribute   The name of the changing attribute
   * @param aModType     Whether or not the attribute will be added, changed, or
   *                     removed. The constants are defined in
   *                     MutationEvent.webidl.
   * @param aNewValue    The new value, IF it has been preparsed by
   *                     BeforeSetAttr, otherwise null.
   *
   * @note Callers of this method might not hold a strong reference to the
   *       observer.  The observer is responsible for making sure it stays
   *       alive for the duration of the call as needed.  The observer may
   *       assume that this call will happen when there are script blockers on
   *       the stack.
   */
  virtual void AttributeWillChange(mozilla::dom::Element* aElement,
                                   int32_t aNameSpaceID, nsAtom* aAttribute,
                                   int32_t aModType) = 0;

  /**
   * Notification that an attribute of an element has changed.
   *
   * @param aElement     The element whose attribute changed
   * @param aNameSpaceID The namespace id of the changed attribute
   * @param aAttribute   The name of the changed attribute
   * @param aModType     Whether or not the attribute was added, changed, or
   *                     removed. The constants are defined in
   *                     MutationEvent.webidl.
   * @param aOldValue    The old value, if either the old value or the new
   *                     value are StoresOwnData() (or absent); null otherwise.
   *
   * @note Callers of this method might not hold a strong reference to the
   *       observer.  The observer is responsible for making sure it stays
   *       alive for the duration of the call as needed.  The observer may
   *       assume that this call will happen when there are script blockers on
   *       the stack.
   */
  virtual void AttributeChanged(mozilla::dom::Element* aElement,
                                int32_t aNameSpaceID, nsAtom* aAttribute,
                                int32_t aModType,
                                const nsAttrValue* aOldValue) = 0;

  /**
   * Notification that the root of a native anonymous has been added
   * or removed.
   *
   * @param aContent     Anonymous node that's been added or removed
   * @param aIsRemove    True if it's a removal, false if an addition
   */
  virtual void NativeAnonymousChildListChange(nsIContent* aContent,
                                              bool aIsRemove) {}

  /**
   * Notification that an attribute of an element has been
   * set to the value it already had.
   *
   * @param aElement     The element whose attribute changed
   * @param aNameSpaceID The namespace id of the changed attribute
   * @param aAttribute   The name of the changed attribute
   */
  virtual void AttributeSetToCurrentValue(mozilla::dom::Element* aElement,
                                          int32_t aNameSpaceID,
                                          nsAtom* aAttribute) {}

  /**
   * Notification that one or more content nodes have been appended to the
   * child list of another node in the tree.
   *
   * @param aFirstNewContent the first node appended.
   *
   * @note Callers of this method might not hold a strong reference to the
   *       observer.  The observer is responsible for making sure it stays
   *       alive for the duration of the call as needed.  The observer may
   *       assume that this call will happen when there are script blockers on
   *       the stack.
   */
  virtual void ContentAppended(nsIContent* aFirstNewContent) = 0;

  /**
   * Notification that a content node has been inserted as child to another
   * node in the tree.
   *
   * @param aChild The newly inserted child.
   *
   * @note Callers of this method might not hold a strong reference to the
   *       observer.  The observer is responsible for making sure it stays
   *       alive for the duration of the call as needed.  The observer may
   *       assume that this call will happen when there are script blockers on
   *       the stack.
   */
  virtual void ContentInserted(nsIContent* aChild) = 0;

  /**
   * Notification that a content node has been removed from the child list of
   * another node in the tree.
   *
   * @param aChild     The child that was removed.
   * @param aPreviousSibling The previous sibling to the child that was removed.
   *                         Can be null if there was no previous sibling.
   *
   * @note Callers of this method might not hold a strong reference to the
   *       observer.  The observer is responsible for making sure it stays
   *       alive for the duration of the call as needed.  The observer may
   *       assume that this call will happen when there are script blockers on
   *       the stack.
   */
  virtual void ContentRemoved(nsIContent* aChild,
                              nsIContent* aPreviousSibling) = 0;

  /**
   * The node is in the process of being destroyed. Calling QI on the node is
   * not supported, however it is possible to get children and flags through
   * nsINode as well as calling IsContent and casting to nsIContent to get
   * attributes.
   *
   * NOTE: This notification is only called on observers registered directly
   * on the node. This is because when the node is destroyed it can not have
   * any ancestors. If you want to know when a descendant node is being
   * removed from the observed node, use the ContentRemoved notification.
   *
   * @param aNode The node being destroyed.
   *
   * @note Callers of this method might not hold a strong reference to
   *       the observer.  The observer is responsible for making sure it
   *       stays alive for the duration of the call as needed.
   */
  virtual void NodeWillBeDestroyed(const nsINode* aNode) = 0;

  /**
   * Notification that the node's parent chain has changed. This
   * happens when either the node or one of its ancestors is inserted
   * or removed as a child of another node.
   *
   * Note that when a node is inserted this notification is sent to
   * all descendants of that node, since all such nodes have their
   * parent chain changed.
   *
   * @param aContent  The piece of content that had its parent changed.
   *
   * @note Callers of this method might not hold a strong reference to
   *       the observer.  The observer is responsible for making sure it
   *       stays alive for the duration of the call as needed.
   */

  virtual void ParentChainChanged(nsIContent* aContent) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIMutationObserver, NS_IMUTATION_OBSERVER_IID)

#define NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATAWILLCHANGE \
  virtual void CharacterDataWillChange(                     \
      nsIContent* aContent, const CharacterDataChangeInfo& aInfo) override;

#define NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED \
  virtual void CharacterDataChanged(                     \
      nsIContent* aContent, const CharacterDataChangeInfo& aInfo) override;

#define NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTEWILLCHANGE                      \
  virtual void AttributeWillChange(mozilla::dom::Element* aElement,          \
                                   int32_t aNameSpaceID, nsAtom* aAttribute, \
                                   int32_t aModType) override;

#define NS_DECL_NSIMUTATIONOBSERVER_NATIVEANONYMOUSCHILDLISTCHANGE  \
  virtual void NativeAnonymousChildListChange(nsIContent* aContent, \
                                              bool aIsRemove) override;

#define NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED                      \
  virtual void AttributeChanged(mozilla::dom::Element* aElement,          \
                                int32_t aNameSpaceID, nsAtom* aAttribute, \
                                int32_t aModType,                         \
                                const nsAttrValue* aOldValue) override;

#define NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED \
  virtual void ContentAppended(nsIContent* aFirstNewContent) override;

#define NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED \
  virtual void ContentInserted(nsIContent* aChild) override;

#define NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED \
  virtual void ContentRemoved(nsIContent* aChild,  \
                              nsIContent* aPreviousSibling) override;

#define NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED \
  virtual void NodeWillBeDestroyed(const nsINode* aNode) override;

#define NS_DECL_NSIMUTATIONOBSERVER_PARENTCHAINCHANGED \
  virtual void ParentChainChanged(nsIContent* aContent) override;

#define NS_DECL_NSIMUTATIONOBSERVER                          \
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATAWILLCHANGE        \
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED           \
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTEWILLCHANGE            \
  NS_DECL_NSIMUTATIONOBSERVER_NATIVEANONYMOUSCHILDLISTCHANGE \
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED               \
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED                \
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED                \
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED                 \
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED            \
  NS_DECL_NSIMUTATIONOBSERVER_PARENTCHAINCHANGED

#define NS_IMPL_NSIMUTATIONOBSERVER_CORE_STUB(_class) \
  void _class::NodeWillBeDestroyed(const nsINode* aNode) {}

#define NS_IMPL_NSIMUTATIONOBSERVER_CONTENT(_class)                          \
  void _class::CharacterDataWillChange(                                      \
      nsIContent* aContent, const CharacterDataChangeInfo& aInfo) {}         \
  void _class::CharacterDataChanged(nsIContent* aContent,                    \
                                    const CharacterDataChangeInfo& aInfo) {} \
  void _class::AttributeWillChange(mozilla::dom::Element* aElement,          \
                                   int32_t aNameSpaceID, nsAtom* aAttribute, \
                                   int32_t aModType) {}                      \
  void _class::NativeAnonymousChildListChange(nsIContent* aContent,          \
                                              bool aIsRemove) {}             \
  void _class::AttributeChanged(                                             \
      mozilla::dom::Element* aElement, int32_t aNameSpaceID,                 \
      nsAtom* aAttribute, int32_t aModType, const nsAttrValue* aOldValue) {} \
  void _class::ContentAppended(nsIContent* aFirstNewContent) {}              \
  void _class::ContentInserted(nsIContent* aChild) {}                        \
  void _class::ContentRemoved(nsIContent* aChild,                            \
                              nsIContent* aPreviousSibling) {}               \
  void _class::ParentChainChanged(nsIContent* aContent) {}

#endif /* nsIMutationObserver_h */
