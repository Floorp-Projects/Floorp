/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLTableElement.h"
#include "mozilla/MappedDeclarations.h"
#include "nsAttrValueInlines.h"
#include "nsHTMLStyleSheet.h"
#include "nsMappedAttributes.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/HTMLCollectionBinding.h"
#include "mozilla/dom/HTMLTableElementBinding.h"
#include "nsContentUtils.h"
#include "jsfriendapi.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Table)

namespace mozilla::dom {

/* ------------------------- TableRowsCollection --------------------------- */
/**
 * This class provides a late-bound collection of rows in a table.
 * mParent is NOT ref-counted to avoid circular references
 */
class TableRowsCollection final : public nsIHTMLCollection,
                                  public nsStubMutationObserver,
                                  public nsWrapperCache {
 public:
  explicit TableRowsCollection(HTMLTableElement* aParent);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

  virtual uint32_t Length() override;
  virtual Element* GetElementAt(uint32_t aIndex) override;
  virtual nsINode* GetParentObject() override { return mParent; }

  virtual Element* GetFirstNamedElement(const nsAString& aName,
                                        bool& aFound) override;
  virtual void GetSupportedNames(nsTArray<nsString>& aNames) override;

  NS_IMETHOD ParentDestroyed();

  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS_AMBIGUOUS(TableRowsCollection,
                                                        nsIHTMLCollection)

  // nsWrapperCache
  using nsWrapperCache::GetWrapperPreserveColor;
  using nsWrapperCache::PreserveWrapper;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

 protected:
  // Unregister ourselves as a mutation observer, and clear our internal state.
  void CleanUp();
  void LastRelease() { CleanUp(); }
  virtual ~TableRowsCollection() {
    // we do NOT have a ref-counted reference to mParent, so do NOT
    // release it!  this is to avoid circular references.  The
    // instantiator who provided mParent is responsible for managing our
    // reference for us.
    CleanUp();
  }

  virtual JSObject* GetWrapperPreserveColorInternal() override {
    return nsWrapperCache::GetWrapperPreserveColor();
  }
  virtual void PreserveWrapperInternal(
      nsISupports* aScriptObjectHolder) override {
    nsWrapperCache::PreserveWrapper(aScriptObjectHolder);
  }

  // Ensure that HTMLTableElement is in a valid state. This must be called
  // before inspecting the mRows object.
  void EnsureInitialized();

  // Checks if the passed-in container is interesting for the purposes of
  // invalidation due to a mutation observer.
  bool InterestingContainer(nsIContent* aContainer);

  // Check if the passed-in nsIContent is a <tr> within the section defined by
  // `aSection`. The root of the table is considered to be part of the `<tbody>`
  // section.
  bool IsAppropriateRow(nsAtom* aSection, nsIContent* aContent);

  // Scan backwards starting from `aCurrent` in the table, looking for the
  // previous row in the table which is within the section `aSection`.
  nsIContent* PreviousRow(nsAtom* aSection, nsIContent* aCurrent);

  // Handle the insertion of the child `aChild` into the container `aContainer`
  // within the tree. The container must be an `InterestingContainer`. This
  // method updates the mRows, mBodyStart, and mFootStart member variables.
  //
  // HandleInsert returns an integer which can be passed to the next call of the
  // method in a loop inserting children into the same container. This will
  // optimize subsequent insertions to require less work. This can either be -1,
  // in which case we don't know where to insert the next row, and When passed
  // to HandleInsert, it will use `PreviousRow` to locate the index to insert.
  // Or, it can be an index to insert the next <tr> in the same container at.
  int32_t HandleInsert(nsIContent* aContainer, nsIContent* aChild,
                       int32_t aIndexGuess = -1);

  // The HTMLTableElement which this TableRowsCollection tracks the rows for.
  HTMLTableElement* mParent;

  // The current state of the TableRowsCollection. mBodyStart and mFootStart are
  // indices into mRows which represent the location of the first row in the
  // body or foot section. If there are no rows in a section, the index points
  // at the location where the first element in that section would be inserted.
  nsTArray<nsCOMPtr<nsIContent>> mRows;
  uint32_t mBodyStart;
  uint32_t mFootStart;
  bool mInitialized;
};

TableRowsCollection::TableRowsCollection(HTMLTableElement* aParent)
    : mParent(aParent), mBodyStart(0), mFootStart(0), mInitialized(false) {
  MOZ_ASSERT(mParent);
}

void TableRowsCollection::EnsureInitialized() {
  if (mInitialized) {
    return;
  }
  mInitialized = true;

  // Initialize mRows as the TableRowsCollection is created. The mutation
  // observer should keep it up to date.
  //
  // It should be extremely unlikely that anyone creates a TableRowsCollection
  // without calling a method on it, so lazily performing this initialization
  // seems unnecessary.
  AutoTArray<nsCOMPtr<nsIContent>, 32> body;
  AutoTArray<nsCOMPtr<nsIContent>, 32> foot;
  mRows.Clear();

  auto addRowChildren = [&](nsTArray<nsCOMPtr<nsIContent>>& aArray,
                            nsIContent* aNode) {
    for (nsIContent* inner = aNode->nsINode::GetFirstChild(); inner;
         inner = inner->GetNextSibling()) {
      if (inner->IsHTMLElement(nsGkAtoms::tr)) {
        aArray.AppendElement(inner);
      }
    }
  };

  for (nsIContent* node = mParent->nsINode::GetFirstChild(); node;
       node = node->GetNextSibling()) {
    if (node->IsHTMLElement(nsGkAtoms::thead)) {
      addRowChildren(mRows, node);
    } else if (node->IsHTMLElement(nsGkAtoms::tbody)) {
      addRowChildren(body, node);
    } else if (node->IsHTMLElement(nsGkAtoms::tfoot)) {
      addRowChildren(foot, node);
    } else if (node->IsHTMLElement(nsGkAtoms::tr)) {
      body.AppendElement(node);
    }
  }

  mBodyStart = mRows.Length();
  mRows.AppendElements(std::move(body));
  mFootStart = mRows.Length();
  mRows.AppendElements(std::move(foot));

  mParent->AddMutationObserver(this);
}

void TableRowsCollection::CleanUp() {
  // Unregister ourselves as a mutation observer.
  if (mInitialized && mParent) {
    mParent->RemoveMutationObserver(this);
  }

  // Clean up all of our internal state and make it empty in case someone looks
  // at us.
  mRows.Clear();
  mBodyStart = 0;
  mFootStart = 0;

  // We set mInitialized to true in case someone still has a reference to us, as
  // we don't need to try to initialize first.
  mInitialized = true;
  mParent = nullptr;
}

JSObject* TableRowsCollection::WrapObject(JSContext* aCx,
                                          JS::Handle<JSObject*> aGivenProto) {
  return HTMLCollection_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TableRowsCollection, mRows)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TableRowsCollection)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(TableRowsCollection,
                                                   LastRelease())

NS_INTERFACE_TABLE_HEAD(TableRowsCollection)
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  NS_INTERFACE_TABLE(TableRowsCollection, nsIHTMLCollection,
                     nsIMutationObserver)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(TableRowsCollection)
NS_INTERFACE_MAP_END

uint32_t TableRowsCollection::Length() {
  EnsureInitialized();
  return mRows.Length();
}

Element* TableRowsCollection::GetElementAt(uint32_t aIndex) {
  EnsureInitialized();
  if (aIndex < mRows.Length()) {
    return mRows[aIndex]->AsElement();
  }
  return nullptr;
}

Element* TableRowsCollection::GetFirstNamedElement(const nsAString& aName,
                                                   bool& aFound) {
  EnsureInitialized();
  aFound = false;
  RefPtr<nsAtom> nameAtom = NS_Atomize(aName);
  NS_ENSURE_TRUE(nameAtom, nullptr);

  for (auto& node : mRows) {
    if (node->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                                       nameAtom, eCaseMatters) ||
        node->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::id,
                                       nameAtom, eCaseMatters)) {
      aFound = true;
      return node->AsElement();
    }
  }

  return nullptr;
}

void TableRowsCollection::GetSupportedNames(nsTArray<nsString>& aNames) {
  EnsureInitialized();
  for (auto& node : mRows) {
    if (node->HasID()) {
      nsAtom* idAtom = node->GetID();
      MOZ_ASSERT(idAtom != nsGkAtoms::_empty, "Empty ids don't get atomized");
      nsDependentAtomString idStr(idAtom);
      if (!aNames.Contains(idStr)) {
        aNames.AppendElement(idStr);
      }
    }

    nsGenericHTMLElement* el = nsGenericHTMLElement::FromNode(node);
    if (el) {
      const nsAttrValue* val = el->GetParsedAttr(nsGkAtoms::name);
      if (val && val->Type() == nsAttrValue::eAtom) {
        nsAtom* nameAtom = val->GetAtomValue();
        MOZ_ASSERT(nameAtom != nsGkAtoms::_empty,
                   "Empty names don't get atomized");
        nsDependentAtomString nameStr(nameAtom);
        if (!aNames.Contains(nameStr)) {
          aNames.AppendElement(nameStr);
        }
      }
    }
  }
}

NS_IMETHODIMP
TableRowsCollection::ParentDestroyed() {
  CleanUp();
  return NS_OK;
}

bool TableRowsCollection::InterestingContainer(nsIContent* aContainer) {
  return mParent && aContainer &&
         (aContainer == mParent ||
          (aContainer->GetParent() == mParent &&
           aContainer->IsAnyOfHTMLElements(nsGkAtoms::thead, nsGkAtoms::tbody,
                                           nsGkAtoms::tfoot)));
}

bool TableRowsCollection::IsAppropriateRow(nsAtom* aSection,
                                           nsIContent* aContent) {
  if (!aContent->IsHTMLElement(nsGkAtoms::tr)) {
    return false;
  }
  // If it's in the root, then we consider it to be in a tbody.
  nsIContent* parent = aContent->GetParent();
  if (aSection == nsGkAtoms::tbody && parent == mParent) {
    return true;
  }
  return parent->IsHTMLElement(aSection);
}

nsIContent* TableRowsCollection::PreviousRow(nsAtom* aSection,
                                             nsIContent* aCurrent) {
  // Keep going backwards until we've found a `tr` element. We want to always
  // run at least once, as we don't want to find ourselves.
  //
  // Each spin of the loop we step backwards one element. If we're at the top of
  // a section, we step out of it into the root, and if we step onto a section
  // matching `aSection`, we step into it. We keep spinning the loop until
  // either we reach the first element in mParent, or find a <tr> in an
  // appropriate section.
  nsIContent* prev = aCurrent;
  do {
    nsIContent* parent = prev->GetParent();
    prev = prev->GetPreviousSibling();

    // Ascend out of any sections we're currently in, if we've run out of
    // elements.
    if (!prev && parent != mParent) {
      prev = parent->GetPreviousSibling();
    }

    // Descend into a section if we stepped onto one.
    if (prev && prev->GetParent() == mParent && prev->IsHTMLElement(aSection)) {
      prev = prev->GetLastChild();
    }
  } while (prev && !IsAppropriateRow(aSection, prev));
  return prev;
}

int32_t TableRowsCollection::HandleInsert(nsIContent* aContainer,
                                          nsIContent* aChild,
                                          int32_t aIndexGuess) {
  if (!nsContentUtils::IsInSameAnonymousTree(mParent, aChild)) {
    return aIndexGuess;  // Nothing inserted, guess hasn't changed.
  }

  // If we're adding a section to the root, add each of the rows in that section
  // individually.
  if (aContainer == mParent &&
      aChild->IsAnyOfHTMLElements(nsGkAtoms::thead, nsGkAtoms::tbody,
                                  nsGkAtoms::tfoot)) {
    // If we're entering a tbody, we can persist the index guess we were passed,
    // as the newly added items are in the same section as us, however, if we're
    // entering thead or tfoot we will have to re-scan.
    bool isTBody = aChild->IsHTMLElement(nsGkAtoms::tbody);
    int32_t indexGuess = isTBody ? aIndexGuess : -1;

    for (nsIContent* inner = aChild->GetFirstChild(); inner;
         inner = inner->GetNextSibling()) {
      indexGuess = HandleInsert(aChild, inner, indexGuess);
    }

    return isTBody ? indexGuess : -1;
  }
  if (!aChild->IsHTMLElement(nsGkAtoms::tr)) {
    return aIndexGuess;  // Nothing inserted, guess hasn't changed.
  }

  // We should have only been passed an insertion from an interesting container,
  // so we can get the container we're inserting to fairly easily.
  nsAtom* section = aContainer == mParent ? nsGkAtoms::tbody
                                          : aContainer->NodeInfo()->NameAtom();

  // Determine the default index we would to insert after if we don't find any
  // previous row, and offset our section boundaries based on the section we're
  // planning to insert into.
  size_t index = 0;
  if (section == nsGkAtoms::thead) {
    mBodyStart++;
    mFootStart++;
  } else if (section == nsGkAtoms::tbody) {
    index = mBodyStart;
    mFootStart++;
  } else if (section == nsGkAtoms::tfoot) {
    index = mFootStart;
  } else {
    MOZ_ASSERT(false, "section should be one of thead, tbody, or tfoot");
  }

  // If we already have an index guess, we can skip scanning for the previous
  // row.
  if (aIndexGuess >= 0) {
    index = aIndexGuess;
  } else {
    // Find the previous row in the section we're inserting into. If we find it,
    // we can use it to override our insertion index. We don't need to modify
    // mBodyStart or mFootStart anymore, as they have already been correctly
    // updated based only on section.
    nsIContent* insertAfter = PreviousRow(section, aChild);
    if (insertAfter) {
      // NOTE: We want to ensure that appending elements is quick, so we search
      // from the end rather than from the beginning.
      index = mRows.LastIndexOf(insertAfter) + 1;
      MOZ_ASSERT(index != nsTArray<nsCOMPtr<nsIContent>>::NoIndex);
    }
  }

#ifdef DEBUG
  // Assert that we're inserting into the correct section.
  if (section == nsGkAtoms::thead) {
    MOZ_ASSERT(index < mBodyStart);
  } else if (section == nsGkAtoms::tbody) {
    MOZ_ASSERT(index >= mBodyStart);
    MOZ_ASSERT(index < mFootStart);
  } else if (section == nsGkAtoms::tfoot) {
    MOZ_ASSERT(index >= mFootStart);
    MOZ_ASSERT(index <= mRows.Length());
  }

  MOZ_ASSERT(mBodyStart <= mFootStart);
  MOZ_ASSERT(mFootStart <= mRows.Length() + 1);
#endif

  mRows.InsertElementAt(index, aChild);
  return index + 1;
}

// nsIMutationObserver

void TableRowsCollection::ContentAppended(nsIContent* aFirstNewContent) {
  nsIContent* container = aFirstNewContent->GetParent();
  if (!nsContentUtils::IsInSameAnonymousTree(mParent, aFirstNewContent) ||
      !InterestingContainer(container)) {
    return;
  }

  // We usually can't guess where we need to start inserting, unless we're
  // appending into mParent, in which case we can provide the guess that we
  // should insert at the end of the body, which can help us avoid potentially
  // expensive work in the common case.
  int32_t indexGuess = mParent == container ? mFootStart : -1;

  // Insert each of the newly added content one at a time. The indexGuess should
  // make insertions of a large number of elements cheaper.
  for (nsIContent* content = aFirstNewContent; content;
       content = content->GetNextSibling()) {
    indexGuess = HandleInsert(container, content, indexGuess);
  }
}

void TableRowsCollection::ContentInserted(nsIContent* aChild) {
  if (!nsContentUtils::IsInSameAnonymousTree(mParent, aChild) ||
      !InterestingContainer(aChild->GetParent())) {
    return;
  }

  HandleInsert(aChild->GetParent(), aChild);
}

void TableRowsCollection::ContentRemoved(nsIContent* aChild,
                                         nsIContent* aPreviousSibling) {
  if (!nsContentUtils::IsInSameAnonymousTree(mParent, aChild) ||
      !InterestingContainer(aChild->GetParent())) {
    return;
  }

  // If the element being removed is a `tr`, we can just remove it from our
  // list. It shouldn't change the order of anything.
  if (aChild->IsHTMLElement(nsGkAtoms::tr)) {
    size_t index = mRows.IndexOf(aChild);
    if (index != nsTArray<nsCOMPtr<nsIContent>>::NoIndex) {
      mRows.RemoveElementAt(index);
      if (mBodyStart > index) {
        mBodyStart--;
      }
      if (mFootStart > index) {
        mFootStart--;
      }
    }
    return;
  }

  // If the element being removed is a `thead`, `tbody`, or `tfoot`, we can
  // remove any `tr`s in our list which have that element as its parent node. In
  // any other situation, the removal won't affect us, so we can ignore it.
  if (!aChild->IsAnyOfHTMLElements(nsGkAtoms::thead, nsGkAtoms::tbody,
                                   nsGkAtoms::tfoot)) {
    return;
  }

  size_t beforeLength = mRows.Length();
  mRows.RemoveElementsBy(
      [&](nsIContent* element) { return element->GetParent() == aChild; });
  size_t removed = beforeLength - mRows.Length();
  if (aChild->IsHTMLElement(nsGkAtoms::thead)) {
    // NOTE: Need to move both tbody and tfoot, as we removed from head.
    mBodyStart -= removed;
    mFootStart -= removed;
  } else if (aChild->IsHTMLElement(nsGkAtoms::tbody)) {
    // NOTE: Need to move tfoot, as we removed from body.
    mFootStart -= removed;
  }
}

void TableRowsCollection::NodeWillBeDestroyed(const nsINode* aNode) {
  // Set mInitialized to false so CleanUp doesn't try to remove our mutation
  // observer, as we're going away. CleanUp() will reset mInitialized to true as
  // it returns.
  mInitialized = false;
  CleanUp();
}

/* --------------------------- HTMLTableElement ---------------------------- */

HTMLTableElement::HTMLTableElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsGenericHTMLElement(std::move(aNodeInfo)),
      mTableInheritedAttributes(nullptr) {
  SetHasWeirdParserInsertionMode();
}

HTMLTableElement::~HTMLTableElement() {
  if (mRows) {
    mRows->ParentDestroyed();
  }
  ReleaseInheritedAttributes();
}

JSObject* HTMLTableElement::WrapNode(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return HTMLTableElement_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLTableElement)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLTableElement,
                                                nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTBodies)
  if (tmp->mRows) {
    tmp->mRows->ParentDestroyed();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRows)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLTableElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTBodies)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRows)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(HTMLTableElement,
                                               nsGenericHTMLElement)

NS_IMPL_ELEMENT_CLONE(HTMLTableElement)

// the DOM spec says border, cellpadding, cellSpacing are all "wstring"
// in fact, they are integers or they are meaningless.  so we store them
// here as ints.

nsIHTMLCollection* HTMLTableElement::Rows() {
  if (!mRows) {
    mRows = new TableRowsCollection(this);
  }

  return mRows;
}

nsIHTMLCollection* HTMLTableElement::TBodies() {
  if (!mTBodies) {
    // Not using NS_GetContentList because this should not be cached
    mTBodies = new nsContentList(this, kNameSpaceID_XHTML, nsGkAtoms::tbody,
                                 nsGkAtoms::tbody, false);
  }

  return mTBodies;
}

already_AddRefed<nsGenericHTMLElement> HTMLTableElement::CreateTHead() {
  RefPtr<nsGenericHTMLElement> head = GetTHead();
  if (!head) {
    // Create a new head rowgroup.
    RefPtr<mozilla::dom::NodeInfo> nodeInfo;
    nsContentUtils::QNameChanged(mNodeInfo, nsGkAtoms::thead,
                                 getter_AddRefs(nodeInfo));

    head = NS_NewHTMLTableSectionElement(nodeInfo.forget());
    if (!head) {
      return nullptr;
    }

    nsCOMPtr<nsIContent> refNode = nullptr;
    for (refNode = nsINode::GetFirstChild(); refNode;
         refNode = refNode->GetNextSibling()) {
      if (refNode->IsHTMLElement() &&
          !refNode->IsHTMLElement(nsGkAtoms::caption) &&
          !refNode->IsHTMLElement(nsGkAtoms::colgroup)) {
        break;
      }
    }

    nsINode::InsertBefore(*head, refNode, IgnoreErrors());
  }
  return head.forget();
}

void HTMLTableElement::DeleteTHead() {
  RefPtr<HTMLTableSectionElement> tHead = GetTHead();
  if (tHead) {
    mozilla::IgnoredErrorResult rv;
    nsINode::RemoveChild(*tHead, rv);
  }
}

already_AddRefed<nsGenericHTMLElement> HTMLTableElement::CreateTFoot() {
  RefPtr<nsGenericHTMLElement> foot = GetTFoot();
  if (!foot) {
    // create a new foot rowgroup
    RefPtr<mozilla::dom::NodeInfo> nodeInfo;
    nsContentUtils::QNameChanged(mNodeInfo, nsGkAtoms::tfoot,
                                 getter_AddRefs(nodeInfo));

    foot = NS_NewHTMLTableSectionElement(nodeInfo.forget());
    if (!foot) {
      return nullptr;
    }
    AppendChildTo(foot, true, IgnoreErrors());
  }

  return foot.forget();
}

void HTMLTableElement::DeleteTFoot() {
  RefPtr<HTMLTableSectionElement> tFoot = GetTFoot();
  if (tFoot) {
    mozilla::IgnoredErrorResult rv;
    nsINode::RemoveChild(*tFoot, rv);
  }
}

already_AddRefed<nsGenericHTMLElement> HTMLTableElement::CreateCaption() {
  RefPtr<nsGenericHTMLElement> caption = GetCaption();
  if (!caption) {
    // Create a new caption.
    RefPtr<mozilla::dom::NodeInfo> nodeInfo;
    nsContentUtils::QNameChanged(mNodeInfo, nsGkAtoms::caption,
                                 getter_AddRefs(nodeInfo));

    caption = NS_NewHTMLTableCaptionElement(nodeInfo.forget());
    if (!caption) {
      return nullptr;
    }

    nsCOMPtr<nsINode> firsChild = nsINode::GetFirstChild();
    nsINode::InsertBefore(*caption, firsChild, IgnoreErrors());
  }
  return caption.forget();
}

void HTMLTableElement::DeleteCaption() {
  RefPtr<HTMLTableCaptionElement> caption = GetCaption();
  if (caption) {
    mozilla::IgnoredErrorResult rv;
    nsINode::RemoveChild(*caption, rv);
  }
}

already_AddRefed<nsGenericHTMLElement> HTMLTableElement::CreateTBody() {
  RefPtr<mozilla::dom::NodeInfo> nodeInfo =
      OwnerDoc()->NodeInfoManager()->GetNodeInfo(
          nsGkAtoms::tbody, nullptr, kNameSpaceID_XHTML, ELEMENT_NODE);
  MOZ_ASSERT(nodeInfo);

  RefPtr<nsGenericHTMLElement> newBody =
      NS_NewHTMLTableSectionElement(nodeInfo.forget());
  MOZ_ASSERT(newBody);

  nsCOMPtr<nsIContent> referenceNode = nullptr;
  for (nsIContent* child = nsINode::GetLastChild(); child;
       child = child->GetPreviousSibling()) {
    if (child->IsHTMLElement(nsGkAtoms::tbody)) {
      referenceNode = child->GetNextSibling();
      break;
    }
  }

  nsINode::InsertBefore(*newBody, referenceNode, IgnoreErrors());

  return newBody.forget();
}

already_AddRefed<nsGenericHTMLElement> HTMLTableElement::InsertRow(
    int32_t aIndex, ErrorResult& aError) {
  /* get the ref row at aIndex
     if there is one,
       get its parent
       insert the new row just before the ref row
     else
       get the first row group
       insert the new row as its first child
  */
  if (aIndex < -1) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  nsIHTMLCollection* rows = Rows();
  uint32_t rowCount = rows->Length();
  if ((uint32_t)aIndex > rowCount && aIndex != -1) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  // use local variable refIndex so we can remember original aIndex
  uint32_t refIndex = (uint32_t)aIndex;

  RefPtr<nsGenericHTMLElement> newRow;
  if (rowCount > 0) {
    if (refIndex == rowCount || aIndex == -1) {
      // we set refIndex to the last row so we can get the last row's
      // parent we then do an AppendChild below if (rowCount<aIndex)

      refIndex = rowCount - 1;
    }

    RefPtr<Element> refRow = rows->Item(refIndex);
    nsCOMPtr<nsINode> parent = refRow->GetParentNode();

    // create the row
    RefPtr<mozilla::dom::NodeInfo> nodeInfo;
    nsContentUtils::QNameChanged(mNodeInfo, nsGkAtoms::tr,
                                 getter_AddRefs(nodeInfo));

    newRow = NS_NewHTMLTableRowElement(nodeInfo.forget());

    if (newRow) {
      // If aIndex is -1 or equal to the number of rows, the new row
      // is appended.
      if (aIndex == -1 || uint32_t(aIndex) == rowCount) {
        parent->AppendChild(*newRow, aError);
      } else {
        // insert the new row before the reference row we found above
        parent->InsertBefore(*newRow, refRow, aError);
      }

      if (aError.Failed()) {
        return nullptr;
      }
    }
  } else {
    // the row count was 0, so
    // find the last row group and insert there as first child
    nsCOMPtr<nsIContent> rowGroup;
    for (nsIContent* child = nsINode::GetLastChild(); child;
         child = child->GetPreviousSibling()) {
      if (child->IsHTMLElement(nsGkAtoms::tbody)) {
        rowGroup = child;
        break;
      }
    }

    if (!rowGroup) {  // need to create a TBODY
      RefPtr<mozilla::dom::NodeInfo> nodeInfo;
      nsContentUtils::QNameChanged(mNodeInfo, nsGkAtoms::tbody,
                                   getter_AddRefs(nodeInfo));

      rowGroup = NS_NewHTMLTableSectionElement(nodeInfo.forget());
      if (rowGroup) {
        AppendChildTo(rowGroup, true, aError);
        if (aError.Failed()) {
          return nullptr;
        }
      }
    }

    if (rowGroup) {
      RefPtr<mozilla::dom::NodeInfo> nodeInfo;
      nsContentUtils::QNameChanged(mNodeInfo, nsGkAtoms::tr,
                                   getter_AddRefs(nodeInfo));

      newRow = NS_NewHTMLTableRowElement(nodeInfo.forget());
      if (newRow) {
        HTMLTableSectionElement* section =
            static_cast<HTMLTableSectionElement*>(rowGroup.get());
        nsIHTMLCollection* rows = section->Rows();
        nsCOMPtr<nsINode> refNode = rows->Item(0);
        rowGroup->InsertBefore(*newRow, refNode, aError);
      }
    }
  }

  return newRow.forget();
}

void HTMLTableElement::DeleteRow(int32_t aIndex, ErrorResult& aError) {
  if (aIndex < -1) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  nsIHTMLCollection* rows = Rows();
  uint32_t refIndex;
  if (aIndex == -1) {
    refIndex = rows->Length();
    if (refIndex == 0) {
      return;
    }

    --refIndex;
  } else {
    refIndex = (uint32_t)aIndex;
  }

  nsCOMPtr<nsIContent> row = rows->Item(refIndex);
  if (!row) {
    aError.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  row->RemoveFromParent();
}

bool HTMLTableElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                      const nsAString& aValue,
                                      nsIPrincipal* aMaybeScriptedPrincipal,
                                      nsAttrValue& aResult) {
  /* ignore summary, just a string */
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::cellspacing ||
        aAttribute == nsGkAtoms::cellpadding ||
        aAttribute == nsGkAtoms::border) {
      return aResult.ParseNonNegativeIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::height) {
      // Purposeful spec violation (spec says to use ParseNonzeroHTMLDimension)
      // to stay compatible with our old behavior and other browsers.  See
      // https://github.com/whatwg/html/issues/4715
      return aResult.ParseHTMLDimension(aValue);
    }
    if (aAttribute == nsGkAtoms::width) {
      return aResult.ParseNonzeroHTMLDimension(aValue);
    }

    if (aAttribute == nsGkAtoms::align) {
      return ParseTableHAlignValue(aValue, aResult);
    }
    if (aAttribute == nsGkAtoms::bgcolor ||
        aAttribute == nsGkAtoms::bordercolor) {
      return aResult.ParseColor(aValue);
    }
  }

  return nsGenericHTMLElement::ParseBackgroundAttribute(
             aNamespaceID, aAttribute, aValue, aResult) ||
         nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void HTMLTableElement::MapAttributesIntoRule(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  // XXX Bug 211636:  This function is used by a single style rule
  // that's used to match two different type of elements -- tables, and
  // table cells.  (nsHTMLTableCellElement overrides
  // WalkContentStyleRules so that this happens.)  This violates the
  // nsIStyleRule contract, since it's the same style rule object doing
  // the mapping in two different ways.  It's also incorrect since it's
  // testing the display type of the ComputedStyle rather than checking
  // which *element* it's matching (style rules should not stop matching
  // when the display type is changed).

  // cellspacing
  const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::cellspacing);
  if (value && value->Type() == nsAttrValue::eInteger &&
      !aDecls.PropertyIsSet(eCSSProperty_border_spacing)) {
    aDecls.SetPixelValue(eCSSProperty_border_spacing,
                         float(value->GetIntegerValue()));
  }
  // align; Check for enumerated type (it may be another type if
  // illegal)
  value = aAttributes->GetAttr(nsGkAtoms::align);
  if (value && value->Type() == nsAttrValue::eEnum) {
    if (value->GetEnumValue() == uint8_t(StyleTextAlign::Center) ||
        value->GetEnumValue() == uint8_t(StyleTextAlign::MozCenter)) {
      aDecls.SetAutoValueIfUnset(eCSSProperty_margin_left);
      aDecls.SetAutoValueIfUnset(eCSSProperty_margin_right);
    }
  }

  // bordercolor
  value = aAttributes->GetAttr(nsGkAtoms::bordercolor);
  nscolor color;
  if (value && value->GetColorValue(color)) {
    aDecls.SetColorValueIfUnset(eCSSProperty_border_top_color, color);
    aDecls.SetColorValueIfUnset(eCSSProperty_border_left_color, color);
    aDecls.SetColorValueIfUnset(eCSSProperty_border_bottom_color, color);
    aDecls.SetColorValueIfUnset(eCSSProperty_border_right_color, color);
  }

  // border
  const nsAttrValue* borderValue = aAttributes->GetAttr(nsGkAtoms::border);
  if (borderValue) {
    // border = 1 pixel default
    int32_t borderThickness = 1;

    if (borderValue->Type() == nsAttrValue::eInteger)
      borderThickness = borderValue->GetIntegerValue();

    // by default, set all border sides to the specified width
    aDecls.SetPixelValueIfUnset(eCSSProperty_border_top_width,
                                (float)borderThickness);
    aDecls.SetPixelValueIfUnset(eCSSProperty_border_left_width,
                                (float)borderThickness);
    aDecls.SetPixelValueIfUnset(eCSSProperty_border_bottom_width,
                                (float)borderThickness);
    aDecls.SetPixelValueIfUnset(eCSSProperty_border_right_width,
                                (float)borderThickness);
  }

  nsGenericHTMLElement::MapImageSizeAttributesInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aDecls);
}

NS_IMETHODIMP_(bool)
HTMLTableElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  static const MappedAttributeEntry attributes[] = {
      {nsGkAtoms::cellpadding}, {nsGkAtoms::cellspacing},
      {nsGkAtoms::border},      {nsGkAtoms::width},
      {nsGkAtoms::height},

      {nsGkAtoms::bordercolor},

      {nsGkAtoms::align},       {nullptr}};

  static const MappedAttributeEntry* const map[] = {
      attributes,
      sCommonAttributeMap,
      sBackgroundAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc HTMLTableElement::GetAttributeMappingFunction()
    const {
  return &MapAttributesIntoRule;
}

static void MapInheritedTableAttributesIntoRule(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::cellpadding);
  if (value && value->Type() == nsAttrValue::eInteger) {
    // We have cellpadding.  This will override our padding values if we
    // don't have any set.
    float pad = float(value->GetIntegerValue());

    aDecls.SetPixelValueIfUnset(eCSSProperty_padding_top, pad);
    aDecls.SetPixelValueIfUnset(eCSSProperty_padding_right, pad);
    aDecls.SetPixelValueIfUnset(eCSSProperty_padding_bottom, pad);
    aDecls.SetPixelValueIfUnset(eCSSProperty_padding_left, pad);
  }
}

nsMappedAttributes* HTMLTableElement::GetAttributesMappedForCell() {
  return mTableInheritedAttributes;
}

void HTMLTableElement::BuildInheritedAttributes() {
  NS_ASSERTION(!mTableInheritedAttributes,
               "potential leak, plus waste of work");
  MOZ_ASSERT(NS_IsMainThread());
  Document* document = GetComposedDoc();
  nsHTMLStyleSheet* sheet =
      document ? document->GetAttributeStyleSheet() : nullptr;
  RefPtr<nsMappedAttributes> newAttrs;
  if (sheet) {
    const nsAttrValue* value = mAttrs.GetAttr(nsGkAtoms::cellpadding);
    if (value) {
      RefPtr<nsMappedAttributes> modifiableMapped =
          new nsMappedAttributes(sheet, MapInheritedTableAttributesIntoRule);

      if (modifiableMapped) {
        nsAttrValue val(*value);
        bool oldValueSet;
        modifiableMapped->SetAndSwapAttr(nsGkAtoms::cellpadding, val,
                                         &oldValueSet);
      }
      newAttrs = sheet->UniqueMappedAttributes(modifiableMapped);
      NS_ASSERTION(newAttrs, "out of memory, but handling gracefully");

      if (newAttrs != modifiableMapped) {
        // Reset the stylesheet of modifiableMapped so that it doesn't
        // spend time trying to remove itself from the hash.  There is no
        // risk that modifiableMapped is in the hash since we created
        // it ourselves and it didn't come from the stylesheet (in which
        // case it would not have been modifiable).
        modifiableMapped->DropStyleSheetReference();
      }
    }
    mTableInheritedAttributes = newAttrs;
    NS_IF_ADDREF(mTableInheritedAttributes);
  }
}

void HTMLTableElement::ReleaseInheritedAttributes() {
  NS_IF_RELEASE(mTableInheritedAttributes);
}

nsresult HTMLTableElement::BindToTree(BindContext& aContext, nsINode& aParent) {
  ReleaseInheritedAttributes();
  nsresult rv = nsGenericHTMLElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);
  BuildInheritedAttributes();
  return NS_OK;
}

void HTMLTableElement::UnbindFromTree(bool aNullParent) {
  ReleaseInheritedAttributes();
  nsGenericHTMLElement::UnbindFromTree(aNullParent);
}

nsresult HTMLTableElement::BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                         const nsAttrValueOrString* aValue,
                                         bool aNotify) {
  if (aName == nsGkAtoms::cellpadding && aNameSpaceID == kNameSpaceID_None) {
    ReleaseInheritedAttributes();
  }
  return nsGenericHTMLElement::BeforeSetAttr(aNameSpaceID, aName, aValue,
                                             aNotify);
}

nsresult HTMLTableElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                        const nsAttrValue* aValue,
                                        const nsAttrValue* aOldValue,
                                        nsIPrincipal* aSubjectPrincipal,
                                        bool aNotify) {
  if (aName == nsGkAtoms::cellpadding && aNameSpaceID == kNameSpaceID_None) {
    BuildInheritedAttributes();
  }
  return nsGenericHTMLElement::AfterSetAttr(
      aNameSpaceID, aName, aValue, aOldValue, aSubjectPrincipal, aNotify);
}

}  // namespace mozilla::dom
