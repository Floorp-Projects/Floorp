/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_ProxyAccessible_h
#define mozilla_a11y_ProxyAccessible_h

#include "mozilla/a11y/Role.h"
#include "nsIAccessibleText.h"
#include "nsIAccessibleTypes.h"
#include "Accessible.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsRect.h"
#include "Accessible.h"

namespace mozilla {
namespace a11y {

class Accessible;
class Attribute;
class DocAccessibleParent;
enum class RelationType;

class ProxyAccessible
{
public:

  ProxyAccessible(uint64_t aID, ProxyAccessible* aParent,
                  DocAccessibleParent* aDoc, role aRole) :
     mParent(aParent), mDoc(aDoc), mWrapper(0), mID(aID), mRole(aRole),
     mOuterDoc(false), mIsDoc(false)
  {
    MOZ_COUNT_CTOR(ProxyAccessible);
  }
  ~ProxyAccessible()
  {
    MOZ_COUNT_DTOR(ProxyAccessible);
    MOZ_ASSERT(!mWrapper);
  }

  void AddChildAt(uint32_t aIdx, ProxyAccessible* aChild)
  { mChildren.InsertElementAt(aIdx, aChild); }

  uint32_t ChildrenCount() const { return mChildren.Length(); }
  ProxyAccessible* ChildAt(uint32_t aIdx) const { return mChildren[aIdx]; }
  ProxyAccessible* FirstChild() const
    { return mChildren.Length() ? mChildren[0] : nullptr; }
  ProxyAccessible* LastChild() const
    { return mChildren.Length() ? mChildren[mChildren.Length() - 1] : nullptr; }
  ProxyAccessible* PrevSibling() const
  {
    size_t idx = IndexInParent();
    return idx > 0 ? Parent()->mChildren[idx - 1] : nullptr;
  }
  ProxyAccessible* NextSibling() const
  {
    size_t idx = IndexInParent();
    return idx + 1 < Parent()->mChildren.Length() ? Parent()->mChildren[idx + 1]
    : nullptr;
  }

  // XXX evaluate if this is fast enough.
  size_t IndexInParent() const { return Parent()->mChildren.IndexOf(this); }
  uint32_t EmbeddedChildCount() const;
  int32_t IndexOfEmbeddedChild(const ProxyAccessible*);
  ProxyAccessible* EmbeddedChildAt(size_t aChildIdx);
  bool MustPruneChildren() const;

  void Shutdown();

  void SetChildDoc(DocAccessibleParent*);

  /**
   * Remove The given child.
   */
  void RemoveChild(ProxyAccessible* aChild)
    { mChildren.RemoveElement(aChild); }

  /**
   * Return the proxy for the parent of the wrapped accessible.
   */
  ProxyAccessible* Parent() const { return mParent; }

  Accessible* OuterDocOfRemoteBrowser() const;

  /**
   * Get the role of the accessible we're proxying.
   */
  role Role() const { return mRole; }

  /**
   * Return true if this is an embedded object.
   */
  bool IsEmbeddedObject() const
  {
    role role = Role();
    return role != roles::TEXT_LEAF &&
           role != roles::WHITESPACE &&
           role != roles::STATICTEXT;
  }

  /*
   * Return the states for the proxied accessible.
   */
  uint64_t State() const;

  /*
   * Return the native states for the proxied accessible.
   */
  uint64_t NativeState() const;

  /*
   * Set aName to the name of the proxied accessible.
   */
  void Name(nsString& aName) const;

  /*
   * Set aValue to the value of the proxied accessible.
   */
  void Value(nsString& aValue) const;

  /*
   * Set aHelp to the help string of the proxied accessible.
   */
  void Help(nsString& aHelp) const;

  /**
   * Set aDesc to the description of the proxied accessible.
   */
  void Description(nsString& aDesc) const;

  /**
   * Get the set of attributes on the proxied accessible.
   */
  void Attributes(nsTArray<Attribute> *aAttrs) const;

  /**
   * Return set of targets of given relation type.
   */
  nsTArray<ProxyAccessible*> RelationByType(RelationType aType) const;

  /**
   * Get all relations for this accessible.
   */
  void Relations(nsTArray<RelationType>* aTypes,
                 nsTArray<nsTArray<ProxyAccessible*>>* aTargetSets) const;

  bool IsSearchbox() const;

  nsIAtom* LandmarkRole() const;

  nsIAtom* ARIARoleAtom() const;

  int32_t GetLevelInternal();
  void ScrollTo(uint32_t aScrollType);
  void ScrollToPoint(uint32_t aScrollType, int32_t aX, int32_t aY);

  int32_t CaretLineNumber();
  int32_t CaretOffset();
  void SetCaretOffset(int32_t aOffset);

  int32_t CharacterCount();
  int32_t SelectionCount();

  /**
   * Get the text between the given offsets.
   */
  bool TextSubstring(int32_t aStartOffset, int32_t aEndOfset,
                     nsString& aText) const;

  void GetTextAfterOffset(int32_t aOffset, AccessibleTextBoundary aBoundaryType,
                          nsString& aText, int32_t* aStartOffset,
                          int32_t* aEndOffset);

  void GetTextAtOffset(int32_t aOffset, AccessibleTextBoundary aBoundaryType,
                       nsString& aText, int32_t* aStartOffset,
                       int32_t* aEndOffset);

  void GetTextBeforeOffset(int32_t aOffset, AccessibleTextBoundary aBoundaryType,
                           nsString& aText, int32_t* aStartOffset,
                           int32_t* aEndOffset);

  char16_t CharAt(int32_t aOffset);

  void TextAttributes(bool aIncludeDefAttrs,
                      const int32_t aOffset,
                      nsTArray<Attribute>* aAttributes,
                      int32_t* aStartOffset,
                      int32_t* aEndOffset);
  void DefaultTextAttributes(nsTArray<Attribute>* aAttrs);

  nsIntRect TextBounds(int32_t aStartOffset, int32_t aEndOffset,
                       uint32_t aCoordType = nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE);

  nsIntRect CharBounds(int32_t aOffset, uint32_t aCoordType);

  int32_t OffsetAtPoint(int32_t aX, int32_t aY, uint32_t aCoordType);

  bool SelectionBoundsAt(int32_t aSelectionNum,
                         nsString& aData,
                         int32_t* aStartOffset,
                         int32_t* aEndOffset);

  bool SetSelectionBoundsAt(int32_t aSelectionNum,
                            int32_t aStartOffset,
                            int32_t aEndOffset);

  bool AddToSelection(int32_t aStartOffset,
                      int32_t aEndOffset);

  bool RemoveFromSelection(int32_t aSelectionNum);

  void ScrollSubstringTo(int32_t aStartOffset, int32_t aEndOffset,
                         uint32_t aScrollType);

  void ScrollSubstringToPoint(int32_t aStartOffset,
                              int32_t aEndOffset,
                              uint32_t aCoordinateType,
                              int32_t aX, int32_t aY);

  void Text(nsString* aText);

  void ReplaceText(const nsString& aText);

  bool InsertText(const nsString& aText, int32_t aPosition);

  bool CopyText(int32_t aStartPos, int32_t aEndPos);

  bool CutText(int32_t aStartPos, int32_t aEndPos);

  bool DeleteText(int32_t aStartPos, int32_t aEndPos);

  bool PasteText(int32_t aPosition);

  nsIntPoint ImagePosition(uint32_t aCoordType);

  nsIntSize ImageSize();

  uint32_t StartOffset(bool* aOk);

  uint32_t EndOffset(bool* aOk);

  bool IsLinkValid();

  // XXX checking mRole alone may not result in same behavior as Accessibles
  // due to ARIA roles. See bug 1210477.
  inline bool IsTable() const
  {
    return mRole == roles::TABLE || mRole == roles::MATHML_TABLE;
  }
  inline bool IsTableRow() const
  {
    return (mRole == roles::ROW ||
            mRole == roles::MATHML_TABLE_ROW ||
            mRole == roles::MATHML_LABELED_ROW);
  }
  inline bool IsTableCell() const
  {
    return (mRole == roles::CELL ||
            mRole == roles::COLUMNHEADER ||
            mRole == roles::ROWHEADER ||
            mRole == roles::GRID_CELL ||
            mRole == roles::MATHML_CELL);
  }

  uint32_t AnchorCount(bool* aOk);

  void AnchorURIAt(uint32_t aIndex, nsCString& aURI, bool* aOk);

  ProxyAccessible* AnchorAt(uint32_t aIndex);

  uint32_t LinkCount();

  ProxyAccessible* LinkAt(const uint32_t& aIndex);

  int32_t LinkIndexOf(ProxyAccessible* aLink);

  int32_t LinkIndexAtOffset(uint32_t aOffset);

  ProxyAccessible* TableOfACell();

  uint32_t ColIdx();

  uint32_t RowIdx();

  uint32_t ColExtent();

  uint32_t RowExtent();

  void ColHeaderCells(nsTArray<ProxyAccessible*>* aCells);

  void RowHeaderCells(nsTArray<ProxyAccessible*>* aCells);

  bool IsCellSelected();

  ProxyAccessible* TableCaption();
  void TableSummary(nsString& aSummary);
  uint32_t TableColumnCount();
  uint32_t TableRowCount();
  ProxyAccessible* TableCellAt(uint32_t aRow, uint32_t aCol);
  int32_t TableCellIndexAt(uint32_t aRow, uint32_t aCol);
  int32_t TableColumnIndexAt(uint32_t aCellIndex);
  int32_t TableRowIndexAt(uint32_t aCellIndex);
  void TableRowAndColumnIndicesAt(uint32_t aCellIndex,
                                  int32_t* aRow, int32_t* aCol);
  uint32_t TableColumnExtentAt(uint32_t aRow, uint32_t aCol);
  uint32_t TableRowExtentAt(uint32_t aRow, uint32_t aCol);
  void TableColumnDescription(uint32_t aCol, nsString& aDescription);
  void TableRowDescription(uint32_t aRow, nsString& aDescription);
  bool TableColumnSelected(uint32_t aCol);
  bool TableRowSelected(uint32_t aRow);
  bool TableCellSelected(uint32_t aRow, uint32_t aCol);
  uint32_t TableSelectedCellCount();
  uint32_t TableSelectedColumnCount();
  uint32_t TableSelectedRowCount();
  void TableSelectedCells(nsTArray<ProxyAccessible*>* aCellIDs);
  void TableSelectedCellIndices(nsTArray<uint32_t>* aCellIndices);
  void TableSelectedColumnIndices(nsTArray<uint32_t>* aColumnIndices);
  void TableSelectedRowIndices(nsTArray<uint32_t>* aRowIndices);
  void TableSelectColumn(uint32_t aCol);
  void TableSelectRow(uint32_t aRow);
  void TableUnselectColumn(uint32_t aCol);
  void TableUnselectRow(uint32_t aRow);
  bool TableIsProbablyForLayout();
  ProxyAccessible* AtkTableColumnHeader(int32_t aCol);
  ProxyAccessible* AtkTableRowHeader(int32_t aRow);

  void SelectedItems(nsTArray<ProxyAccessible*>* aSelectedItems);
  uint32_t SelectedItemCount();
  ProxyAccessible* GetSelectedItem(uint32_t aIndex);
  bool IsItemSelected(uint32_t aIndex);
  bool AddItemToSelection(uint32_t aIndex);
  bool RemoveItemFromSelection(uint32_t aIndex);
  bool SelectAll();
  bool UnselectAll();

  void TakeSelection();
  void SetSelected(bool aSelect);

  bool DoAction(uint8_t aIndex);
  uint8_t ActionCount();
  void ActionDescriptionAt(uint8_t aIndex, nsString& aDescription);
  void ActionNameAt(uint8_t aIndex, nsString& aName);
  KeyBinding AccessKey();
  KeyBinding KeyboardShortcut();
  void AtkKeyBinding(nsString& aBinding);

  double CurValue();
  bool SetCurValue(double aValue);
  double MinValue();
  double MaxValue();
  double Step();

  void TakeFocus();
  ProxyAccessible* FocusedChild();
  ProxyAccessible* ChildAtPoint(int32_t aX, int32_t aY,
                                Accessible::EWhichChildAtPoint aWhichChild);
  nsIntRect Bounds();

  void Language(nsString& aLocale);
  void DocType(nsString& aType);
  void Title(nsString& aTitle);
  void URL(nsString& aURL);
  void MimeType(nsString aMime);
  void URLDocTypeMimeType(nsString& aURL, nsString& aDocType,
                          nsString& aMimeType);

  ProxyAccessible* AccessibleAtPoint(int32_t aX, int32_t aY,
                                     bool aNeedsScreenCoords);

  void Extents(bool aNeedsScreenCoords, int32_t* aX, int32_t* aY,
               int32_t* aWidth, int32_t* aHeight);

  /**
   * Return the id of the dom node this accessible represents.  Note this
   * should probably only be used for testing.
   */
  void DOMNodeID(nsString& aID);

  /**
   * Allow the platform to store a pointers worth of data on us.
   */
  uintptr_t GetWrapper() const { return mWrapper; }
  void SetWrapper(uintptr_t aWrapper) { mWrapper = aWrapper; }

  /*
   * Return the ID of the accessible being proxied.
   */
  uint64_t ID() const { return mID; }

  /**
   * Return the document containing this proxy, or the proxy itself if it is a
   * document.
   */
  DocAccessibleParent* Document() const { return mDoc; }

  /**
   * Return true if this proxy is a DocAccessibleParent.
   */
  bool IsDoc() const { return mIsDoc; }
  DocAccessibleParent* AsDoc() const { return IsDoc() ? mDoc : nullptr; }

protected:
  explicit ProxyAccessible(DocAccessibleParent* aThisAsDoc) :
    mParent(nullptr), mDoc(aThisAsDoc), mWrapper(0), mID(0),
    mRole(roles::DOCUMENT), mOuterDoc(false), mIsDoc(true)
  { MOZ_COUNT_CTOR(ProxyAccessible); }

protected:
  ProxyAccessible* mParent;

private:
  nsTArray<ProxyAccessible*> mChildren;
  DocAccessibleParent* mDoc;
  uintptr_t mWrapper;
  uint64_t mID;
  role mRole : 30;
  bool mOuterDoc : 1;
  const bool mIsDoc: 1;
};

enum Interfaces
{
  HYPERTEXT = 1,
  HYPERLINK = 1 << 1,
  IMAGE = 1 << 2,
  VALUE = 1 << 3,
  TABLE = 1 << 4,
  TABLECELL = 1 << 5,
  DOCUMENT = 1 << 6,
  SELECTION = 1 << 7,
  ACTION = 1 << 8,
};

}
}

#endif
