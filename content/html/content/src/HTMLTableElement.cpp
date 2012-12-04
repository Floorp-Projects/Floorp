/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "mozilla/dom/HTMLTableElement.h"
#include "nsIDOMHTMLTableSectionElement.h"
#include "nsAttrValueInlines.h"
#include "nsRuleData.h"
#include "nsHTMLStyleSheet.h"
#include "nsMappedAttributes.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/HTMLCollectionBinding.h"
#include "mozilla/dom/HTMLTableElementBinding.h"
#include "nsContentUtils.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Table)

namespace mozilla {
namespace dom {

/* ------------------------------ TableRowsCollection -------------------------------- */
/**
 * This class provides a late-bound collection of rows in a table.
 * mParent is NOT ref-counted to avoid circular references
 */
class TableRowsCollection : public nsIHTMLCollection,
                            public nsWrapperCache
{
public:
  TableRowsCollection(HTMLTableElement *aParent);
  virtual ~TableRowsCollection();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIDOMHTMLCOLLECTION

  virtual Element* GetElementAt(uint32_t aIndex);
  virtual nsINode* GetParentObject()
  {
    return mParent;
  }

  virtual JSObject* NamedItem(JSContext* cx, const nsAString& name,
                              ErrorResult& error);
  virtual void GetSupportedNames(nsTArray<nsString>& aNames);

  NS_IMETHOD    ParentDestroyed();

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TableRowsCollection)

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext *cx,
                               JS::Handle<JSObject*> scope) MOZ_OVERRIDE
  {
    return mozilla::dom::HTMLCollectionBinding::Wrap(cx, scope, this);
  }

protected:
  // Those rows that are not in table sections
  HTMLTableElement* mParent;
  nsRefPtr<nsContentList> mOrphanRows;  
};


TableRowsCollection::TableRowsCollection(HTMLTableElement *aParent)
  : mParent(aParent)
  , mOrphanRows(new nsContentList(mParent,
                                  kNameSpaceID_XHTML,
                                  nsGkAtoms::tr,
                                  nsGkAtoms::tr,
                                  false))
{
  SetIsDOMBinding();
}

TableRowsCollection::~TableRowsCollection()
{
  // we do NOT have a ref-counted reference to mParent, so do NOT
  // release it!  this is to avoid circular references.  The
  // instantiator who provided mParent is responsible for managing our
  // reference for us.
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(TableRowsCollection, mOrphanRows)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TableRowsCollection)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TableRowsCollection)

NS_INTERFACE_TABLE_HEAD(TableRowsCollection)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_TABLE2(TableRowsCollection, nsIHTMLCollection,
                      nsIDOMHTMLCollection)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(TableRowsCollection)
NS_INTERFACE_MAP_END

// Macro that can be used to avoid copy/pasting code to iterate over the
// rowgroups.  _code should be the code to execute for each rowgroup.  The
// rowgroup's rows will be in the nsIDOMHTMLCollection* named "rows".  Note
// that this may be null at any time.  This macro assumes an nsresult named
// |rv| is in scope.
#define DO_FOR_EACH_ROWGROUP(_code)                                  \
  do {                                                               \
    if (mParent) {                                                   \
      /* THead */                                                    \
      HTMLTableSectionElement* rowGroup = mParent->GetTHead();       \
      nsIHTMLCollection* rows;                                       \
      if (rowGroup) {                                                \
        rows = rowGroup->Rows();                                     \
        do { /* gives scoping */                                     \
          _code                                                      \
        } while (0);                                                 \
      }                                                              \
      /* TBodies */                                                  \
      for (nsIContent* _node = mParent->nsINode::GetFirstChild();    \
           _node; _node = _node->GetNextSibling()) {                 \
        if (_node->IsHTML(nsGkAtoms::tbody)) {                       \
          rowGroup = static_cast<HTMLTableSectionElement*>(_node);   \
          rows = rowGroup->Rows();                                   \
          do { /* gives scoping */                                   \
            _code                                                    \
          } while (0);                                               \
        }                                                            \
      }                                                              \
      /* orphan rows */                                              \
      rows = mOrphanRows;                                            \
      do { /* gives scoping */                                       \
        _code                                                        \
      } while (0);                                                   \
      /* TFoot */                                                    \
      rowGroup = mParent->GetTFoot();                                \
      rows = nullptr;                                                \
      if (rowGroup) {                                                \
        rows = rowGroup->Rows();                                     \
        do { /* gives scoping */                                     \
          _code                                                      \
        } while (0);                                                 \
      }                                                              \
    }                                                                \
  } while (0)

static uint32_t
CountRowsInRowGroup(nsIDOMHTMLCollection* rows)
{
  uint32_t length = 0;
  
  if (rows) {
    rows->GetLength(&length);
  }
  
  return length;
}

// we re-count every call.  A better implementation would be to set
// ourselves up as an observer of contentAppended, contentInserted,
// and contentDeleted
NS_IMETHODIMP 
TableRowsCollection::GetLength(uint32_t* aLength)
{
  *aLength=0;

  DO_FOR_EACH_ROWGROUP(
    *aLength += CountRowsInRowGroup(rows);
  );

  return NS_OK;
}

// Returns the item at index aIndex if available. If null is returned,
// then aCount will be set to the number of rows in this row collection.
// Otherwise, the value of aCount is undefined.
static Element*
GetItemOrCountInRowGroup(nsIDOMHTMLCollection* rows,
                         uint32_t aIndex, uint32_t* aCount)
{
  *aCount = 0;

  if (rows) {
    rows->GetLength(aCount);
    if (aIndex < *aCount) {
      nsIHTMLCollection* list = static_cast<nsIHTMLCollection*>(rows);
      return list->GetElementAt(aIndex);
    }
  }
  
  return nullptr;
}

Element*
TableRowsCollection::GetElementAt(uint32_t aIndex)
{
  DO_FOR_EACH_ROWGROUP(
    uint32_t count;
    Element* node = GetItemOrCountInRowGroup(rows, aIndex, &count);
    if (node) {
      return node; 
    }

    NS_ASSERTION(count <= aIndex, "GetItemOrCountInRowGroup screwed up");
    aIndex -= count;
  );

  return nullptr;
}

NS_IMETHODIMP 
TableRowsCollection::Item(uint32_t aIndex, nsIDOMNode** aReturn)
{
  nsISupports* node = GetElementAt(aIndex);
  if (!node) {
    *aReturn = nullptr;

    return NS_OK;
  }

  return CallQueryInterface(node, aReturn);
}

JSObject*
TableRowsCollection::NamedItem(JSContext* cx, const nsAString& name,
                               ErrorResult& error)
{
  DO_FOR_EACH_ROWGROUP(
    nsCOMPtr<nsIHTMLCollection> collection = do_QueryInterface(rows);
    if (collection) {
      // We'd like to call the nsIHTMLCollection::NamedItem that returns a
      // JSObject*, but that relies on collection having a cached wrapper, which
      // we can't guarantee here.
      nsCOMPtr<nsIDOMNode> item;
      error = collection->NamedItem(name, getter_AddRefs(item));
      if (error.Failed()) {
        return nullptr;
      }
      if (item) {
        JS::Rooted<JSObject*> wrapper(cx, nsWrapperCache::GetWrapper());
        JSAutoCompartment ac(cx, wrapper);
        JS::Rooted<JS::Value> v(cx);
        if (!mozilla::dom::WrapObject(cx, wrapper, item, &v)) {
          error.Throw(NS_ERROR_FAILURE);
          return nullptr;
        }
        return &v.toObject();
      }
    }
  );
  return nullptr;
}

void
TableRowsCollection::GetSupportedNames(nsTArray<nsString>& aNames)
{
  DO_FOR_EACH_ROWGROUP(
    nsTArray<nsString> names;
    nsCOMPtr<nsIHTMLCollection> coll = do_QueryInterface(rows);
    if (coll) {
      coll->GetSupportedNames(names);
      for (uint32_t i = 0; i < names.Length(); ++i) {
        if (!aNames.Contains(names[i])) {
          aNames.AppendElement(names[i]);
        }
      }
    }
  );
}


NS_IMETHODIMP 
TableRowsCollection::NamedItem(const nsAString& aName,
                               nsIDOMNode** aReturn)
{
  DO_FOR_EACH_ROWGROUP(
    nsCOMPtr<nsIHTMLCollection> collection = do_QueryInterface(rows);
    if (collection) {
      nsresult rv = collection->NamedItem(aName, aReturn);
      if (NS_FAILED(rv) || *aReturn) {
        return rv;
      }
    }
  );

  *aReturn = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
TableRowsCollection::ParentDestroyed()
{
  // see comment in destructor, do NOT release mParent!
  mParent = nullptr;

  return NS_OK;
}

/* --------------------------- HTMLTableElement ---------------------------- */

HTMLTableElement::HTMLTableElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo),
    mTableInheritedAttributes(TABLE_ATTRS_DIRTY)
{
  SetIsDOMBinding();
}

HTMLTableElement::~HTMLTableElement()
{
  if (mRows) {
    mRows->ParentDestroyed();
  }
  ReleaseInheritedAttributes();
}

JSObject*
HTMLTableElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return HTMLTableElementBinding::Wrap(aCx, aScope, this);
}

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLTableElement, nsGenericHTMLElement)
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

NS_IMPL_ADDREF_INHERITED(HTMLTableElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLTableElement, Element)

// QueryInterface implementation for HTMLTableElement
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLTableElement)
  NS_HTML_CONTENT_INTERFACES(nsGenericHTMLElement)
  NS_INTERFACE_TABLE_INHERITED1(HTMLTableElement, nsIDOMHTMLTableElement)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
NS_ELEMENT_INTERFACE_MAP_END


NS_IMPL_ELEMENT_CLONE(HTMLTableElement)


// the DOM spec says border, cellpadding, cellSpacing are all "wstring"
// in fact, they are integers or they are meaningless.  so we store them
// here as ints.

NS_IMETHODIMP
HTMLTableElement::SetAlign(const nsAString& aAlign)
{
  ErrorResult rv;
  SetAlign(aAlign, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
HTMLTableElement::GetAlign(nsAString& aAlign)
{
  nsString align;
  GetAlign(align);
  aAlign = align;
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableElement::SetBgColor(const nsAString& aBgColor)
{
  ErrorResult rv;
  SetBgColor(aBgColor, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
HTMLTableElement::GetBgColor(nsAString& aBgColor)
{
  nsString bgColor;
  GetBgColor(bgColor);
  aBgColor = bgColor;
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableElement::SetBorder(const nsAString& aBorder)
{
  ErrorResult rv;
  SetBorder(aBorder, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
HTMLTableElement::GetBorder(nsAString& aBorder)
{
  nsString border;
  GetBorder(border);
  aBorder = border;
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableElement::SetCellPadding(const nsAString& aCellPadding)
{
  ErrorResult rv;
  SetCellPadding(aCellPadding, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
HTMLTableElement::GetCellPadding(nsAString& aCellPadding)
{
  nsString cellPadding;
  GetCellPadding(cellPadding);
  aCellPadding = cellPadding;
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableElement::SetCellSpacing(const nsAString& aCellSpacing)
{
  ErrorResult rv;
  SetCellSpacing(aCellSpacing, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
HTMLTableElement::GetCellSpacing(nsAString& aCellSpacing)
{
  nsString cellSpacing;
  GetCellSpacing(cellSpacing);
  aCellSpacing = cellSpacing;
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableElement::SetFrame(const nsAString& aFrame)
{
  ErrorResult rv;
  SetFrame(aFrame, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
HTMLTableElement::GetFrame(nsAString& aFrame)
{
  nsString frame;
  GetFrame(frame);
  aFrame = frame;
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableElement::SetRules(const nsAString& aRules)
{
  ErrorResult rv;
  SetRules(aRules, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
HTMLTableElement::GetRules(nsAString& aRules)
{
  nsString rules;
  GetRules(rules);
  aRules = rules;
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableElement::SetSummary(const nsAString& aSummary)
{
  ErrorResult rv;
  SetSummary(aSummary, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
HTMLTableElement::GetSummary(nsAString& aSummary)
{
  nsString summary;
  GetSummary(summary);
  aSummary = summary;
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableElement::SetWidth(const nsAString& aWidth)
{
  ErrorResult rv;
  SetWidth(aWidth, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
HTMLTableElement::GetWidth(nsAString& aWidth)
{
  nsString width;
  GetWidth(width);
  aWidth = width;
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableElement::GetCaption(nsIDOMHTMLTableCaptionElement** aValue)
{
  nsCOMPtr<nsIDOMHTMLTableCaptionElement> caption = GetCaption();
  caption.forget(aValue);
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableElement::SetCaption(nsIDOMHTMLTableCaptionElement* aValue)
{
  HTMLTableCaptionElement* caption =
    static_cast<HTMLTableCaptionElement*>(aValue);
  SetCaption(caption);
  return NS_OK;
}

NS_IMETHODIMP
HTMLTableElement::GetTHead(nsIDOMHTMLTableSectionElement** aValue)
{
  NS_IF_ADDREF(*aValue = GetTHead());

  return NS_OK;
}

NS_IMETHODIMP
HTMLTableElement::SetTHead(nsIDOMHTMLTableSectionElement* aValue)
{
  HTMLTableSectionElement* section =
    static_cast<HTMLTableSectionElement*>(aValue);
  ErrorResult rv;
  SetTHead(section, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
HTMLTableElement::GetTFoot(nsIDOMHTMLTableSectionElement** aValue)
{
  NS_IF_ADDREF(*aValue = GetTFoot());

  return NS_OK;
}

NS_IMETHODIMP
HTMLTableElement::SetTFoot(nsIDOMHTMLTableSectionElement* aValue)
{
  HTMLTableSectionElement* section =
    static_cast<HTMLTableSectionElement*>(aValue);
  ErrorResult rv;
  SetTFoot(section, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
HTMLTableElement::GetRows(nsIDOMHTMLCollection** aValue)
{
  NS_ADDREF(*aValue = Rows());
  return NS_OK;
}

nsIHTMLCollection*
HTMLTableElement::Rows()
{
  if (!mRows) {
    mRows = new TableRowsCollection(this);
  }

  return mRows;
}

NS_IMETHODIMP
HTMLTableElement::GetTBodies(nsIDOMHTMLCollection** aValue)
{
  NS_ADDREF(*aValue = TBodies());
  return NS_OK;
}

nsIHTMLCollection*
HTMLTableElement::TBodies()
{
  if (!mTBodies) {
    // Not using NS_GetContentList because this should not be cached
    mTBodies = new nsContentList(this,
                                 kNameSpaceID_XHTML,
                                 nsGkAtoms::tbody,
                                 nsGkAtoms::tbody,
                                 false);
  }

  return mTBodies;
}

already_AddRefed<nsGenericHTMLElement>
HTMLTableElement::CreateTHead()
{
  nsRefPtr<nsGenericHTMLElement> head = GetTHead();
  if (!head) {
    // Create a new head rowgroup.
    nsCOMPtr<nsINodeInfo> nodeInfo;
    nsContentUtils::NameChanged(mNodeInfo, nsGkAtoms::thead,
                                getter_AddRefs(nodeInfo));

    head = NS_NewHTMLTableSectionElement(nodeInfo.forget());
    if (!head) {
      return nullptr;
    }

    ErrorResult rv;
    nsINode::InsertBefore(*head, nsINode::GetFirstChild(), rv);
  }
  return head.forget();
}

NS_IMETHODIMP
HTMLTableElement::CreateTHead(nsIDOMHTMLElement** aValue)
{
  nsRefPtr<nsGenericHTMLElement> thead = CreateTHead();
  return thead ? CallQueryInterface(thead, aValue) : NS_OK;
}

NS_IMETHODIMP
HTMLTableElement::DeleteTHead()
{
  HTMLTableSectionElement* tHead = GetTHead();
  if (tHead) {
    mozilla::ErrorResult rv;
    nsINode::RemoveChild(*tHead, rv);
    MOZ_ASSERT(!rv.Failed());
  }

  return NS_OK;
}

already_AddRefed<nsGenericHTMLElement>
HTMLTableElement::CreateTFoot()
{
  nsRefPtr<nsGenericHTMLElement> foot = GetTFoot();
  if (!foot) {
    // create a new foot rowgroup
    nsCOMPtr<nsINodeInfo> nodeInfo;
    nsContentUtils::NameChanged(mNodeInfo, nsGkAtoms::tfoot,
                                getter_AddRefs(nodeInfo));

    foot = NS_NewHTMLTableSectionElement(nodeInfo.forget());
    if (!foot) {
      return nullptr;
    }
    AppendChildTo(foot, true);
  }

  return foot.forget();
}

NS_IMETHODIMP
HTMLTableElement::CreateTFoot(nsIDOMHTMLElement** aValue)
{
  nsRefPtr<nsGenericHTMLElement> tfoot = CreateTFoot();
  return tfoot ? CallQueryInterface(tfoot, aValue) : NS_OK;
}

NS_IMETHODIMP
HTMLTableElement::DeleteTFoot()
{
  HTMLTableSectionElement* tFoot = GetTFoot();
  if (tFoot) {
    mozilla::ErrorResult rv;
    nsINode::RemoveChild(*tFoot, rv);
    MOZ_ASSERT(!rv.Failed());
  }

  return NS_OK;
}

already_AddRefed<nsGenericHTMLElement>
HTMLTableElement::CreateCaption()
{
  nsRefPtr<nsGenericHTMLElement> caption = GetCaption();
  if (!caption) {
    // Create a new caption.
    nsCOMPtr<nsINodeInfo> nodeInfo;
    nsContentUtils::NameChanged(mNodeInfo, nsGkAtoms::caption,
                                getter_AddRefs(nodeInfo));

    caption = NS_NewHTMLTableCaptionElement(nodeInfo.forget());
    if (!caption) {
      return nullptr;
    }

    AppendChildTo(caption, true);
  }
  return caption.forget();
}

NS_IMETHODIMP
HTMLTableElement::CreateCaption(nsIDOMHTMLElement** aValue)
{
  nsRefPtr<nsGenericHTMLElement> caption = CreateCaption();
  return caption ? CallQueryInterface(caption, aValue) : NS_OK;
}

NS_IMETHODIMP
HTMLTableElement::DeleteCaption()
{
  HTMLTableCaptionElement* caption = GetCaption();
  if (caption) {
    mozilla::ErrorResult rv;
    nsINode::RemoveChild(*caption, rv);
    MOZ_ASSERT(!rv.Failed());
  }

  return NS_OK;
}

already_AddRefed<nsGenericHTMLElement>
HTMLTableElement::CreateTBody()
{
  nsCOMPtr<nsINodeInfo> nodeInfo =
    OwnerDoc()->NodeInfoManager()->GetNodeInfo(nsGkAtoms::tbody, nullptr,
                                               kNameSpaceID_XHTML,
                                               nsIDOMNode::ELEMENT_NODE);
  MOZ_ASSERT(nodeInfo);

  nsCOMPtr<nsGenericHTMLElement> newBody =
    NS_NewHTMLTableSectionElement(nodeInfo.forget());
  MOZ_ASSERT(newBody);

  nsIContent* referenceNode = nullptr;
  for (nsIContent* child = nsINode::GetLastChild();
       child;
       child = child->GetPreviousSibling()) {
    if (child->IsHTML(nsGkAtoms::tbody)) {
      referenceNode = child->GetNextSibling();
      break;
    }
  }

  ErrorResult rv;
  nsINode::InsertBefore(*newBody, referenceNode, rv);

  return newBody.forget();
}

already_AddRefed<nsGenericHTMLElement>
HTMLTableElement::InsertRow(int32_t aIndex, ErrorResult& aError)
{
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

  nsRefPtr<nsGenericHTMLElement> newRow;
  if (rowCount > 0) {
    if (refIndex == rowCount || aIndex == -1) {
      // we set refIndex to the last row so we can get the last row's
      // parent we then do an AppendChild below if (rowCount<aIndex)

      refIndex = rowCount - 1;
    }

    Element* refRow = rows->Item(refIndex);
    nsINode* parent = refRow->GetParentNode();

    // create the row
    nsCOMPtr<nsINodeInfo> nodeInfo;
    nsContentUtils::NameChanged(mNodeInfo, nsGkAtoms::tr,
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
    // find the first row group and insert there as first child
    nsCOMPtr<nsIContent> rowGroup;
    for (nsIContent* child = nsINode::GetFirstChild();
         child;
         child = child->GetNextSibling()) {
      nsINodeInfo *childInfo = child->NodeInfo();
      nsIAtom *localName = childInfo->NameAtom();
      if (childInfo->NamespaceID() == kNameSpaceID_XHTML &&
          (localName == nsGkAtoms::thead ||
           localName == nsGkAtoms::tbody ||
           localName == nsGkAtoms::tfoot)) {
        rowGroup = child;
        break;
      }
    }

    if (!rowGroup) { // need to create a TBODY
      nsCOMPtr<nsINodeInfo> nodeInfo;
      nsContentUtils::NameChanged(mNodeInfo, nsGkAtoms::tbody,
                                  getter_AddRefs(nodeInfo));

      rowGroup = NS_NewHTMLTableSectionElement(nodeInfo.forget());
      if (rowGroup) {
        aError = AppendChildTo(rowGroup, true);
        if (aError.Failed()) {
          return nullptr;
        }
      }
    }

    if (rowGroup) {
      nsCOMPtr<nsINodeInfo> nodeInfo;
      nsContentUtils::NameChanged(mNodeInfo, nsGkAtoms::tr,
                                  getter_AddRefs(nodeInfo));

      newRow = NS_NewHTMLTableRowElement(nodeInfo.forget());
      if (newRow) {
        HTMLTableSectionElement* section =
          static_cast<HTMLTableSectionElement*>(rowGroup.get());
        nsIHTMLCollection* rows = section->Rows();
        rowGroup->InsertBefore(*newRow, rows->Item(0), aError);
      }
    }
  }

  return newRow.forget();
}

NS_IMETHODIMP
HTMLTableElement::InsertRow(int32_t aIndex, nsIDOMHTMLElement** aValue)
{
  ErrorResult rv;
  nsRefPtr<nsGenericHTMLElement> newRow = InsertRow(aIndex, rv);
  return rv.Failed() ? rv.ErrorCode() : CallQueryInterface(newRow, aValue);
}

void
HTMLTableElement::DeleteRow(int32_t aIndex, ErrorResult& aError)
{
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

NS_IMETHODIMP
HTMLTableElement::DeleteRow(int32_t aValue)
{
  ErrorResult rv;
  DeleteRow(aValue, rv);
  return rv.ErrorCode();
}

static const nsAttrValue::EnumTable kFrameTable[] = {
  { "void",   NS_STYLE_TABLE_FRAME_NONE },
  { "above",  NS_STYLE_TABLE_FRAME_ABOVE },
  { "below",  NS_STYLE_TABLE_FRAME_BELOW },
  { "hsides", NS_STYLE_TABLE_FRAME_HSIDES },
  { "lhs",    NS_STYLE_TABLE_FRAME_LEFT },
  { "rhs",    NS_STYLE_TABLE_FRAME_RIGHT },
  { "vsides", NS_STYLE_TABLE_FRAME_VSIDES },
  { "box",    NS_STYLE_TABLE_FRAME_BOX },
  { "border", NS_STYLE_TABLE_FRAME_BORDER },
  { 0 }
};

static const nsAttrValue::EnumTable kRulesTable[] = {
  { "none",   NS_STYLE_TABLE_RULES_NONE },
  { "groups", NS_STYLE_TABLE_RULES_GROUPS },
  { "rows",   NS_STYLE_TABLE_RULES_ROWS },
  { "cols",   NS_STYLE_TABLE_RULES_COLS },
  { "all",    NS_STYLE_TABLE_RULES_ALL },
  { 0 }
};


bool
HTMLTableElement::ParseAttribute(int32_t aNamespaceID,
                                 nsIAtom* aAttribute,
                                 const nsAString& aValue,
                                 nsAttrValue& aResult)
{
  /* ignore summary, just a string */
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::cellspacing ||
        aAttribute == nsGkAtoms::cellpadding ||
        aAttribute == nsGkAtoms::border) {
      return aResult.ParseNonNegativeIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::height) {
      return aResult.ParseSpecialIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::width) {
      if (aResult.ParseSpecialIntValue(aValue)) {
        // treat 0 width as auto
        nsAttrValue::ValueType type = aResult.Type();
        return !((type == nsAttrValue::eInteger &&
                  aResult.GetIntegerValue() == 0) ||
                 (type == nsAttrValue::ePercent &&
                  aResult.GetPercentValue() == 0.0f));
      }
      return false;
    }
    
    if (aAttribute == nsGkAtoms::align) {
      return ParseTableHAlignValue(aValue, aResult);
    }
    if (aAttribute == nsGkAtoms::bgcolor ||
        aAttribute == nsGkAtoms::bordercolor) {
      return aResult.ParseColor(aValue);
    }
    if (aAttribute == nsGkAtoms::frame) {
      return aResult.ParseEnumValue(aValue, kFrameTable, false);
    }
    if (aAttribute == nsGkAtoms::rules) {
      return aResult.ParseEnumValue(aValue, kRulesTable, false);
    }
    if (aAttribute == nsGkAtoms::hspace ||
        aAttribute == nsGkAtoms::vspace) {
      return aResult.ParseIntWithBounds(aValue, 0);
    }
  }

  return nsGenericHTMLElement::ParseBackgroundAttribute(aNamespaceID,
                                                        aAttribute, aValue,
                                                        aResult) ||
         nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}



static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  // XXX Bug 211636:  This function is used by a single style rule
  // that's used to match two different type of elements -- tables, and
  // table cells.  (nsHTMLTableCellElement overrides
  // WalkContentStyleRules so that this happens.)  This violates the
  // nsIStyleRule contract, since it's the same style rule object doing
  // the mapping in two different ways.  It's also incorrect since it's
  // testing the display type of the style context rather than checking
  // which *element* it's matching (style rules should not stop matching
  // when the display type is changed).

  nsPresContext* presContext = aData->mPresContext;
  nsCompatibility mode = presContext->CompatibilityMode();

  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(TableBorder)) {
    // cellspacing
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::cellspacing);
    nsCSSValue* borderSpacing = aData->ValueForBorderSpacing();
    if (value && value->Type() == nsAttrValue::eInteger &&
        borderSpacing->GetUnit() == eCSSUnit_Null) {
      borderSpacing->
        SetFloatValue(float(value->GetIntegerValue()), eCSSUnit_Pixel);
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Margin)) {
    // align; Check for enumerated type (it may be another type if
    // illegal)
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);

    if (value && value->Type() == nsAttrValue::eEnum) {
      if (value->GetEnumValue() == NS_STYLE_TEXT_ALIGN_CENTER ||
          value->GetEnumValue() == NS_STYLE_TEXT_ALIGN_MOZ_CENTER) {
        nsCSSValue* marginLeft = aData->ValueForMarginLeftValue();
        if (marginLeft->GetUnit() == eCSSUnit_Null)
          marginLeft->SetAutoValue();
        nsCSSValue* marginRight = aData->ValueForMarginRightValue();
        if (marginRight->GetUnit() == eCSSUnit_Null)
          marginRight->SetAutoValue();
      }
    }

    // hspace is mapped into left and right margin,
    // vspace is mapped into top and bottom margins
    // - *** Quirks Mode only ***
    if (eCompatibility_NavQuirks == mode) {
      value = aAttributes->GetAttr(nsGkAtoms::hspace);

      if (value && value->Type() == nsAttrValue::eInteger) {
        nsCSSValue* marginLeft = aData->ValueForMarginLeftValue();
        if (marginLeft->GetUnit() == eCSSUnit_Null)
          marginLeft->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel); 
        nsCSSValue* marginRight = aData->ValueForMarginRightValue();
        if (marginRight->GetUnit() == eCSSUnit_Null)
          marginRight->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
      }

      value = aAttributes->GetAttr(nsGkAtoms::vspace);

      if (value && value->Type() == nsAttrValue::eInteger) {
        nsCSSValue* marginTop = aData->ValueForMarginTop();
        if (marginTop->GetUnit() == eCSSUnit_Null)
          marginTop->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel); 
        nsCSSValue* marginBottom = aData->ValueForMarginBottom();
        if (marginBottom->GetUnit() == eCSSUnit_Null)
          marginBottom->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel); 
      }
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Position)) {
    // width: value
    nsCSSValue* width = aData->ValueForWidth();
    if (width->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::width);
      if (value && value->Type() == nsAttrValue::eInteger)
        width->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
      else if (value && value->Type() == nsAttrValue::ePercent)
        width->SetPercentValue(value->GetPercentValue());
    }

    // height: value
    nsCSSValue* height = aData->ValueForHeight();
    if (height->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::height);
      if (value && value->Type() == nsAttrValue::eInteger)
        height->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
      else if (value && value->Type() == nsAttrValue::ePercent)
        height->SetPercentValue(value->GetPercentValue());
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Border)) {
    // bordercolor
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::bordercolor);
    nscolor color;
    if (value && presContext->UseDocumentColors() &&
        value->GetColorValue(color)) {
      nsCSSValue* borderLeftColor = aData->ValueForBorderLeftColorValue();
      if (borderLeftColor->GetUnit() == eCSSUnit_Null)
        borderLeftColor->SetColorValue(color);
      nsCSSValue* borderRightColor = aData->ValueForBorderRightColorValue();
      if (borderRightColor->GetUnit() == eCSSUnit_Null)
        borderRightColor->SetColorValue(color);
      nsCSSValue* borderTopColor = aData->ValueForBorderTopColor();
      if (borderTopColor->GetUnit() == eCSSUnit_Null)
        borderTopColor->SetColorValue(color);
      nsCSSValue* borderBottomColor = aData->ValueForBorderBottomColor();
      if (borderBottomColor->GetUnit() == eCSSUnit_Null)
        borderBottomColor->SetColorValue(color);
    }

    // border
    const nsAttrValue* borderValue = aAttributes->GetAttr(nsGkAtoms::border);
    if (borderValue) {
      // border = 1 pixel default
      int32_t borderThickness = 1;

      if (borderValue->Type() == nsAttrValue::eInteger)
        borderThickness = borderValue->GetIntegerValue();

      // by default, set all border sides to the specified width
      nsCSSValue* borderLeftWidth = aData->ValueForBorderLeftWidthValue();
      if (borderLeftWidth->GetUnit() == eCSSUnit_Null)
        borderLeftWidth->SetFloatValue((float)borderThickness, eCSSUnit_Pixel);
      nsCSSValue* borderRightWidth = aData->ValueForBorderRightWidthValue();
      if (borderRightWidth->GetUnit() == eCSSUnit_Null)
        borderRightWidth->SetFloatValue((float)borderThickness, eCSSUnit_Pixel);
      nsCSSValue* borderTopWidth = aData->ValueForBorderTopWidth();
      if (borderTopWidth->GetUnit() == eCSSUnit_Null)
        borderTopWidth->SetFloatValue((float)borderThickness, eCSSUnit_Pixel);
      nsCSSValue* borderBottomWidth = aData->ValueForBorderBottomWidth();
      if (borderBottomWidth->GetUnit() == eCSSUnit_Null)
        borderBottomWidth->SetFloatValue((float)borderThickness, eCSSUnit_Pixel);
    }
  }
  nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
HTMLTableElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::cellpadding },
    { &nsGkAtoms::cellspacing },
    { &nsGkAtoms::border },
    { &nsGkAtoms::width },
    { &nsGkAtoms::height },
    { &nsGkAtoms::hspace },
    { &nsGkAtoms::vspace },
    
    { &nsGkAtoms::bordercolor },
    
    { &nsGkAtoms::align },
    { nullptr }
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
    sBackgroundAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc
HTMLTableElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

static void
MapInheritedTableAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Padding)) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::cellpadding);
    if (value && value->Type() == nsAttrValue::eInteger) {
      // We have cellpadding.  This will override our padding values if we
      // don't have any set.
      nsCSSValue padVal(float(value->GetIntegerValue()), eCSSUnit_Pixel);

      nsCSSValue* paddingLeft = aData->ValueForPaddingLeftValue();
      if (paddingLeft->GetUnit() == eCSSUnit_Null) {
        *paddingLeft = padVal;
      }

      nsCSSValue* paddingRight = aData->ValueForPaddingRightValue();
      if (paddingRight->GetUnit() == eCSSUnit_Null) {
        *paddingRight = padVal;
      }

      nsCSSValue* paddingTop = aData->ValueForPaddingTop();
      if (paddingTop->GetUnit() == eCSSUnit_Null) {
        *paddingTop = padVal;
      }

      nsCSSValue* paddingBottom = aData->ValueForPaddingBottom();
      if (paddingBottom->GetUnit() == eCSSUnit_Null) {
        *paddingBottom = padVal;
      }
    }
  }
}

nsMappedAttributes*
HTMLTableElement::GetAttributesMappedForCell()
{
  if (mTableInheritedAttributes) {
    if (mTableInheritedAttributes == TABLE_ATTRS_DIRTY)
      BuildInheritedAttributes();
    if (mTableInheritedAttributes != TABLE_ATTRS_DIRTY)
      return mTableInheritedAttributes;
  }
  return nullptr;
}

void
HTMLTableElement::BuildInheritedAttributes()
{
  NS_ASSERTION(mTableInheritedAttributes == TABLE_ATTRS_DIRTY,
               "potential leak, plus waste of work");
  nsIDocument *document = GetCurrentDoc();
  nsHTMLStyleSheet* sheet = document ?
                              document->GetAttributeStyleSheet() : nullptr;
  nsRefPtr<nsMappedAttributes> newAttrs;
  if (sheet) {
    const nsAttrValue* value = mAttrsAndChildren.GetAttr(nsGkAtoms::cellpadding);
    if (value) {
      nsRefPtr<nsMappedAttributes> modifiableMapped = new
      nsMappedAttributes(sheet, MapInheritedTableAttributesIntoRule);

      if (modifiableMapped) {
        nsAttrValue val(*value);
        modifiableMapped->SetAndTakeAttr(nsGkAtoms::cellpadding, val);
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

void
HTMLTableElement::ReleaseInheritedAttributes()
{
  if (mTableInheritedAttributes &&
      mTableInheritedAttributes != TABLE_ATTRS_DIRTY)
    NS_RELEASE(mTableInheritedAttributes);
  mTableInheritedAttributes = TABLE_ATTRS_DIRTY;
}

nsresult
HTMLTableElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                             nsIContent* aBindingParent,
                             bool aCompileEventHandlers)
{
  ReleaseInheritedAttributes();
  return nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                          aBindingParent,
                                          aCompileEventHandlers);
}

void
HTMLTableElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  ReleaseInheritedAttributes();
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

nsresult
HTMLTableElement::BeforeSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValueOrString* aValue,
                                bool aNotify)
{
  if (aName == nsGkAtoms::cellpadding && aNameSpaceID == kNameSpaceID_None) {
    ReleaseInheritedAttributes();
  }
  return nsGenericHTMLElement::BeforeSetAttr(aNameSpaceID, aName, aValue,
                                             aNotify);
}

nsresult
HTMLTableElement::AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                               const nsAttrValue* aValue,
                               bool aNotify)
{
  if (aName == nsGkAtoms::cellpadding && aNameSpaceID == kNameSpaceID_None) {
    BuildInheritedAttributes();
  }
  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName, aValue,
                                            aNotify);
}

} // namespace dom
} // namespace mozilla
