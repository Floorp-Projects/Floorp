/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessibleChild_h
#define mozilla_a11y_DocAccessibleChild_h

#include "mozilla/a11y/DocAccessibleChildBase.h"

namespace mozilla {
namespace a11y {

class Accessible;
class HyperTextAccessible;
class TextLeafAccessible;
class ImageAccessible;
class TableAccessible;
class TableCellAccessible;

/*
 * These objects handle content side communication for an accessible document,
 * and their lifetime is the same as the document they represent.
 */
class DocAccessibleChild : public DocAccessibleChildBase
{
public:
  DocAccessibleChild(DocAccessible* aDoc, IProtocol* aManager)
    : DocAccessibleChildBase(aDoc)
  {
    MOZ_COUNT_CTOR_INHERITED(DocAccessibleChild, DocAccessibleChildBase);
    SetManager(aManager);
  }

  ~DocAccessibleChild()
  {
    MOZ_COUNT_DTOR_INHERITED(DocAccessibleChild, DocAccessibleChildBase);
  }

  virtual mozilla::ipc::IPCResult RecvRestoreFocus() override;

  /*
   * Return the state for the accessible with given ID.
   */
  virtual mozilla::ipc::IPCResult RecvState(const uint64_t& aID, uint64_t* aState) override;

  /*
   * Return the native state for the accessible with given ID.
   */
  virtual mozilla::ipc::IPCResult RecvNativeState(const uint64_t& aID, uint64_t* aState) override;

  /*
   * Get the name for the accessible with given id.
   */
  virtual mozilla::ipc::IPCResult RecvName(const uint64_t& aID, nsString* aName) override;

  virtual mozilla::ipc::IPCResult RecvValue(const uint64_t& aID, nsString* aValue) override;

  virtual mozilla::ipc::IPCResult RecvHelp(const uint64_t& aID, nsString* aHelp) override;

  /*
   * Get the description for the accessible with given id.
   */
  virtual mozilla::ipc::IPCResult RecvDescription(const uint64_t& aID, nsString* aDesc) override;
  virtual mozilla::ipc::IPCResult RecvRelationByType(const uint64_t& aID, const uint32_t& aType,
                                                     nsTArray<uint64_t>* aTargets) override;
  virtual mozilla::ipc::IPCResult RecvRelations(const uint64_t& aID,
                                                nsTArray<RelationTargets>* aRelations)
    override;

  virtual mozilla::ipc::IPCResult RecvIsSearchbox(const uint64_t& aID, bool* aRetVal) override;

  virtual mozilla::ipc::IPCResult RecvLandmarkRole(const uint64_t& aID, nsString* aLandmark) override;

  virtual mozilla::ipc::IPCResult RecvARIARoleAtom(const uint64_t& aID, nsString* aRole) override;

  virtual mozilla::ipc::IPCResult RecvGetLevelInternal(const uint64_t& aID, int32_t* aLevel) override;

  virtual mozilla::ipc::IPCResult RecvAttributes(const uint64_t& aID,
                                                 nsTArray<Attribute> *aAttributes) override;
  virtual mozilla::ipc::IPCResult RecvScrollTo(const uint64_t& aID, const uint32_t& aScrollType)
    override;
  virtual mozilla::ipc::IPCResult RecvScrollToPoint(const uint64_t& aID,
                                                    const uint32_t& aScrollType,
                                                    const int32_t& aX, const int32_t& aY) override;

  virtual mozilla::ipc::IPCResult RecvCaretLineNumber(const uint64_t& aID, int32_t* aLineNumber)
    override;
  virtual mozilla::ipc::IPCResult RecvCaretOffset(const uint64_t& aID, int32_t* aOffset)
    override;
  virtual mozilla::ipc::IPCResult RecvSetCaretOffset(const uint64_t& aID, const int32_t& aOffset)
    override;

  virtual mozilla::ipc::IPCResult RecvCharacterCount(const uint64_t& aID, int32_t* aCount)
     override;
  virtual mozilla::ipc::IPCResult RecvSelectionCount(const uint64_t& aID, int32_t* aCount)
     override;

  virtual mozilla::ipc::IPCResult RecvTextSubstring(const uint64_t& aID,
                                                    const int32_t& aStartOffset,
                                                    const int32_t& aEndOffset, nsString* aText,
                                                    bool* aValid) override;

  virtual mozilla::ipc::IPCResult RecvGetTextAfterOffset(const uint64_t& aID,
                                                         const int32_t& aOffset,
                                                         const int32_t& aBoundaryType,
                                                         nsString* aText, int32_t* aStartOffset,
                                                         int32_t* aEndOffset) override;
  virtual mozilla::ipc::IPCResult RecvGetTextAtOffset(const uint64_t& aID,
                                                      const int32_t& aOffset,
                                                      const int32_t& aBoundaryType,
                                                      nsString* aText, int32_t* aStartOffset,
                                                      int32_t* aEndOffset) override;
  virtual mozilla::ipc::IPCResult RecvGetTextBeforeOffset(const uint64_t& aID,
                                                          const int32_t& aOffset,
                                                          const int32_t& aBoundaryType,
                                                          nsString* aText, int32_t* aStartOffset,
                                                          int32_t* aEndOffset) override;

  virtual mozilla::ipc::IPCResult RecvCharAt(const uint64_t& aID,
                                             const int32_t& aOffset,
                                             uint16_t* aChar) override;

  virtual mozilla::ipc::IPCResult RecvTextAttributes(const uint64_t& aID,
                                                     const bool& aIncludeDefAttrs,
                                                     const int32_t& aOffset,
                                                     nsTArray<Attribute>* aAttributes,
                                                     int32_t* aStartOffset,
                                                     int32_t* aEndOffset)
    override;

  virtual mozilla::ipc::IPCResult RecvDefaultTextAttributes(const uint64_t& aID,
                                                            nsTArray<Attribute>* aAttributes)
    override;

  virtual mozilla::ipc::IPCResult RecvTextBounds(const uint64_t& aID,
                                                 const int32_t& aStartOffset,
                                                 const int32_t& aEndOffset,
                                                 const uint32_t& aCoordType,
                                                 nsIntRect* aRetVal) override;

  virtual mozilla::ipc::IPCResult RecvCharBounds(const uint64_t& aID,
                                                 const int32_t& aOffset,
                                                 const uint32_t& aCoordType,
                                                 nsIntRect* aRetVal) override;

  virtual mozilla::ipc::IPCResult RecvOffsetAtPoint(const uint64_t& aID,
                                                    const int32_t& aX,
                                                    const int32_t& aY,
                                                    const uint32_t& aCoordType,
                                                    int32_t* aRetVal) override;

  virtual mozilla::ipc::IPCResult RecvSelectionBoundsAt(const uint64_t& aID,
                                                        const int32_t& aSelectionNum,
                                                        bool* aSucceeded,
                                                        nsString* aData,
                                                        int32_t* aStartOffset,
                                                        int32_t* aEndOffset) override;

  virtual mozilla::ipc::IPCResult RecvSetSelectionBoundsAt(const uint64_t& aID,
                                                           const int32_t& aSelectionNum,
                                                           const int32_t& aStartOffset,
                                                           const int32_t& aEndOffset,
                                                           bool* aSucceeded) override;

  virtual mozilla::ipc::IPCResult RecvAddToSelection(const uint64_t& aID,
                                                     const int32_t& aStartOffset,
                                                     const int32_t& aEndOffset,
                                                     bool* aSucceeded) override;

  virtual mozilla::ipc::IPCResult RecvRemoveFromSelection(const uint64_t& aID,
                                                          const int32_t& aSelectionNum,
                                                          bool* aSucceeded) override;

  virtual mozilla::ipc::IPCResult RecvScrollSubstringTo(const uint64_t& aID,
                                                        const int32_t& aStartOffset,
                                                        const int32_t& aEndOffset,
                                                        const uint32_t& aScrollType) override;

  virtual mozilla::ipc::IPCResult RecvScrollSubstringToPoint(const uint64_t& aID,
                                                             const int32_t& aStartOffset,
                                                             const int32_t& aEndOffset,
                                                             const uint32_t& aCoordinateType,
                                                             const int32_t& aX,
                                                             const int32_t& aY) override;

  virtual mozilla::ipc::IPCResult RecvText(const uint64_t& aID,
                                           nsString* aText) override;

  virtual mozilla::ipc::IPCResult RecvReplaceText(const uint64_t& aID,
                                                  const nsString& aText) override;

  virtual mozilla::ipc::IPCResult RecvInsertText(const uint64_t& aID,
                                                 const nsString& aText,
                                                 const int32_t& aPosition, bool* aValid) override;

  virtual mozilla::ipc::IPCResult RecvCopyText(const uint64_t& aID,
                                               const int32_t& aStartPos,
                                               const int32_t& aEndPos, bool* aValid) override;

  virtual mozilla::ipc::IPCResult RecvCutText(const uint64_t& aID,
                                              const int32_t& aStartPos,
                                              const int32_t& aEndPos, bool* aValid) override;

  virtual mozilla::ipc::IPCResult RecvDeleteText(const uint64_t& aID,
                                                 const int32_t& aStartPos,
                                                 const int32_t& aEndPos, bool* aValid) override;

  virtual mozilla::ipc::IPCResult RecvPasteText(const uint64_t& aID,
                                                const int32_t& aPosition, bool* aValid) override;

  virtual mozilla::ipc::IPCResult RecvImagePosition(const uint64_t& aID,
                                                    const uint32_t& aCoordType,
                                                    nsIntPoint* aRetVal) override;

  virtual mozilla::ipc::IPCResult RecvImageSize(const uint64_t& aID,
                                                nsIntSize* aRetVal) override;

  virtual mozilla::ipc::IPCResult RecvStartOffset(const uint64_t& aID,
                                                  uint32_t* aRetVal,
                                                  bool* aOk) override;
  virtual mozilla::ipc::IPCResult RecvEndOffset(const uint64_t& aID,
                                                uint32_t* aRetVal,
                                                bool* aOk) override;
  virtual mozilla::ipc::IPCResult RecvIsLinkValid(const uint64_t& aID,
                                                  bool* aRetVal) override;
  virtual mozilla::ipc::IPCResult RecvAnchorCount(const uint64_t& aID,
                                                  uint32_t* aRetVal, bool* aOk) override;
  virtual mozilla::ipc::IPCResult RecvAnchorURIAt(const uint64_t& aID,
                                                  const uint32_t& aIndex,
                                                  nsCString* aURI,
                                                  bool* aOk) override;
  virtual mozilla::ipc::IPCResult RecvAnchorAt(const uint64_t& aID,
                                               const uint32_t& aIndex,
                                               uint64_t* aIDOfAnchor,
                                               bool* aOk) override;

  virtual mozilla::ipc::IPCResult RecvLinkCount(const uint64_t& aID,
                                                uint32_t* aCount) override;

  virtual mozilla::ipc::IPCResult RecvLinkAt(const uint64_t& aID,
                                             const uint32_t& aIndex,
                                             uint64_t* aIDOfLink,
                                             bool* aOk) override;

  virtual mozilla::ipc::IPCResult RecvLinkIndexOf(const uint64_t& aID,
                                                  const uint64_t& aLinkID,
                                                  int32_t* aIndex) override;

  virtual mozilla::ipc::IPCResult RecvLinkIndexAtOffset(const uint64_t& aID,
                                                        const uint32_t& aOffset,
                                                        int32_t* aIndex) override;

  virtual mozilla::ipc::IPCResult RecvTableOfACell(const uint64_t& aID,
                                                   uint64_t* aTableID,
                                                   bool* aOk) override;

  virtual mozilla::ipc::IPCResult RecvColIdx(const uint64_t& aID, uint32_t* aIndex) override;

  virtual mozilla::ipc::IPCResult RecvRowIdx(const uint64_t& aID, uint32_t* aIndex) override;

  virtual mozilla::ipc::IPCResult RecvColExtent(const uint64_t& aID, uint32_t* aExtent) override;

  virtual mozilla::ipc::IPCResult RecvGetPosition(const uint64_t& aID,
                                                  uint32_t* aColIdx, uint32_t* aRowIdx) override;

  virtual mozilla::ipc::IPCResult RecvGetColRowExtents(const uint64_t& aID,
                                                       uint32_t* aColIdx, uint32_t* aRowIdx,
                                                       uint32_t* aColExtent, uint32_t* aRowExtent) override;

  virtual mozilla::ipc::IPCResult RecvRowExtent(const uint64_t& aID, uint32_t* aExtent) override;

  virtual mozilla::ipc::IPCResult RecvColHeaderCells(const uint64_t& aID,
                                                     nsTArray<uint64_t>* aCells) override;

  virtual mozilla::ipc::IPCResult RecvRowHeaderCells(const uint64_t& aID,
                                                     nsTArray<uint64_t>* aCells) override;

  virtual mozilla::ipc::IPCResult RecvIsCellSelected(const uint64_t& aID,
                                                     bool* aSelected) override;

  virtual mozilla::ipc::IPCResult RecvTableCaption(const uint64_t& aID,
                                                   uint64_t* aCaptionID,
                                                   bool* aOk) override;
  virtual mozilla::ipc::IPCResult RecvTableSummary(const uint64_t& aID,
                                                   nsString* aSummary) override;
  virtual mozilla::ipc::IPCResult RecvTableColumnCount(const uint64_t& aID,
                                                       uint32_t* aColCount) override;
  virtual mozilla::ipc::IPCResult RecvTableRowCount(const uint64_t& aID,
                                                    uint32_t* aRowCount) override;
  virtual mozilla::ipc::IPCResult RecvTableCellAt(const uint64_t& aID,
                                                  const uint32_t& aRow,
                                                  const uint32_t& aCol,
                                                  uint64_t* aCellID,
                                                  bool* aOk) override;
  virtual mozilla::ipc::IPCResult RecvTableCellIndexAt(const uint64_t& aID,
                                                       const uint32_t& aRow,
                                                       const uint32_t& aCol,
                                                       int32_t* aIndex) override;
  virtual mozilla::ipc::IPCResult RecvTableColumnIndexAt(const uint64_t& aID,
                                                         const uint32_t& aCellIndex,
                                                         int32_t* aCol) override;
  virtual mozilla::ipc::IPCResult RecvTableRowIndexAt(const uint64_t& aID,
                                                      const uint32_t& aCellIndex,
                                                      int32_t* aRow) override;
  virtual mozilla::ipc::IPCResult RecvTableRowAndColumnIndicesAt(const uint64_t& aID,
                                                                 const uint32_t& aCellIndex,
                                                                 int32_t* aRow,
                                                                 int32_t* aCol) override;
  virtual mozilla::ipc::IPCResult RecvTableColumnExtentAt(const uint64_t& aID,
                                                          const uint32_t& aRow,
                                                          const uint32_t& aCol,
                                                          uint32_t* aExtent) override;
  virtual mozilla::ipc::IPCResult RecvTableRowExtentAt(const uint64_t& aID,
                                                       const uint32_t& aRow,
                                                       const uint32_t& aCol,
                                                       uint32_t* aExtent) override;
  virtual mozilla::ipc::IPCResult RecvTableColumnDescription(const uint64_t& aID,
                                                             const uint32_t& aCol,
                                                             nsString* aDescription) override;
  virtual mozilla::ipc::IPCResult RecvTableRowDescription(const uint64_t& aID,
                                                          const uint32_t& aRow,
                                                          nsString* aDescription) override;
  virtual mozilla::ipc::IPCResult RecvTableColumnSelected(const uint64_t& aID,
                                                          const uint32_t& aCol,
                                                          bool* aSelected) override;
  virtual mozilla::ipc::IPCResult RecvTableRowSelected(const uint64_t& aID,
                                                       const uint32_t& aRow,
                                                       bool* aSelected) override;
  virtual mozilla::ipc::IPCResult RecvTableCellSelected(const uint64_t& aID,
                                                        const uint32_t& aRow,
                                                        const uint32_t& aCol,
                                                        bool* aSelected) override;
  virtual mozilla::ipc::IPCResult RecvTableSelectedCellCount(const uint64_t& aID,
                                                             uint32_t* aSelectedCells) override;
  virtual mozilla::ipc::IPCResult RecvTableSelectedColumnCount(const uint64_t& aID,
                                                               uint32_t* aSelectedColumns) override;
  virtual mozilla::ipc::IPCResult RecvTableSelectedRowCount(const uint64_t& aID,
                                                            uint32_t* aSelectedRows) override;
  virtual mozilla::ipc::IPCResult RecvTableSelectedCells(const uint64_t& aID,
                                                         nsTArray<uint64_t>* aCellIDs) override;
  virtual mozilla::ipc::IPCResult RecvTableSelectedCellIndices(const uint64_t& aID,
                                                               nsTArray<uint32_t>* aCellIndices) override;
  virtual mozilla::ipc::IPCResult RecvTableSelectedColumnIndices(const uint64_t& aID,
                                                                 nsTArray<uint32_t>* aColumnIndices) override;
  virtual mozilla::ipc::IPCResult RecvTableSelectedRowIndices(const uint64_t& aID,
                                                              nsTArray<uint32_t>* aRowIndices) override;
  virtual mozilla::ipc::IPCResult RecvTableSelectColumn(const uint64_t& aID,
                                                        const uint32_t& aCol) override;
  virtual mozilla::ipc::IPCResult RecvTableSelectRow(const uint64_t& aID,
                                                     const uint32_t& aRow) override;
  virtual mozilla::ipc::IPCResult RecvTableUnselectColumn(const uint64_t& aID,
                                                          const uint32_t& aCol) override;
  virtual mozilla::ipc::IPCResult RecvTableUnselectRow(const uint64_t& aID,
                                                       const uint32_t& aRow) override;
  virtual mozilla::ipc::IPCResult RecvTableIsProbablyForLayout(const uint64_t& aID,
                                                               bool* aForLayout) override;
  virtual mozilla::ipc::IPCResult RecvAtkTableColumnHeader(const uint64_t& aID,
                                                           const int32_t& aCol,
                                                           uint64_t* aHeader,
                                                           bool* aOk) override;
  virtual mozilla::ipc::IPCResult RecvAtkTableRowHeader(const uint64_t& aID,
                                                        const int32_t& aRow,
                                                        uint64_t* aHeader,
                                                        bool* aOk) override;

  virtual mozilla::ipc::IPCResult RecvSelectedItems(const uint64_t& aID,
                                                    nsTArray<uint64_t>* aSelectedItemIDs) override;

  virtual mozilla::ipc::IPCResult RecvSelectedItemCount(const uint64_t& aID,
                                                        uint32_t* aCount) override;

  virtual mozilla::ipc::IPCResult RecvGetSelectedItem(const uint64_t& aID,
                                                      const uint32_t& aIndex,
                                                      uint64_t* aSelected,
                                                      bool* aOk) override;

  virtual mozilla::ipc::IPCResult RecvIsItemSelected(const uint64_t& aID,
                                                     const uint32_t& aIndex,
                                                     bool* aSelected) override;

  virtual mozilla::ipc::IPCResult RecvAddItemToSelection(const uint64_t& aID,
                                                         const uint32_t& aIndex,
                                                         bool* aSuccess) override;

  virtual mozilla::ipc::IPCResult RecvRemoveItemFromSelection(const uint64_t& aID,
                                                              const uint32_t& aIndex,
                                                              bool* aSuccess) override;

  virtual mozilla::ipc::IPCResult RecvSelectAll(const uint64_t& aID,
                                                bool* aSuccess) override;

  virtual mozilla::ipc::IPCResult RecvUnselectAll(const uint64_t& aID,
                                                  bool* aSuccess) override;

  virtual mozilla::ipc::IPCResult RecvTakeSelection(const uint64_t& aID) override;
  virtual mozilla::ipc::IPCResult RecvSetSelected(const uint64_t& aID,
                                                  const bool& aSelect) override;

  virtual mozilla::ipc::IPCResult RecvDoAction(const uint64_t& aID,
                                               const uint8_t& aIndex,
                                               bool* aSuccess) override;

  virtual mozilla::ipc::IPCResult RecvActionCount(const uint64_t& aID,
                                                  uint8_t* aCount) override;

  virtual mozilla::ipc::IPCResult RecvActionDescriptionAt(const uint64_t& aID,
                                                          const uint8_t& aIndex,
                                                          nsString* aDescription) override;

  virtual mozilla::ipc::IPCResult RecvActionNameAt(const uint64_t& aID,
                                                   const uint8_t& aIndex,
                                                   nsString* aName) override;

  virtual mozilla::ipc::IPCResult RecvAccessKey(const uint64_t& aID,
                                                uint32_t* aKey,
                                                uint32_t* aModifierMask) override;

  virtual mozilla::ipc::IPCResult RecvKeyboardShortcut(const uint64_t& aID,
                                                       uint32_t* aKey,
                                                       uint32_t* aModifierMask) override;

  virtual mozilla::ipc::IPCResult RecvAtkKeyBinding(const uint64_t& aID,
                                                    nsString* aResult) override;

  virtual mozilla::ipc::IPCResult RecvCurValue(const uint64_t& aID,
                                               double* aValue) override;

  virtual mozilla::ipc::IPCResult RecvSetCurValue(const uint64_t& aID,
                                                  const double& aValue,
                                                  bool* aRetVal) override;

  virtual mozilla::ipc::IPCResult RecvMinValue(const uint64_t& aID,
                                               double* aValue) override;

  virtual mozilla::ipc::IPCResult RecvMaxValue(const uint64_t& aID,
                                               double* aValue) override;

  virtual mozilla::ipc::IPCResult RecvStep(const uint64_t& aID,
                                           double* aStep) override;

  virtual mozilla::ipc::IPCResult RecvTakeFocus(const uint64_t& aID) override;

  virtual mozilla::ipc::IPCResult RecvFocusedChild(const uint64_t& aID,
                                                   uint64_t* aChild,
                                                   bool* aOk) override;

  virtual mozilla::ipc::IPCResult RecvLanguage(const uint64_t& aID, nsString* aLocale) override;
  virtual mozilla::ipc::IPCResult RecvDocType(const uint64_t& aID, nsString* aType) override;
  virtual mozilla::ipc::IPCResult RecvTitle(const uint64_t& aID, nsString* aTitle) override;
  virtual mozilla::ipc::IPCResult RecvURL(const uint64_t& aID, nsString* aURL) override;
  virtual mozilla::ipc::IPCResult RecvMimeType(const uint64_t& aID, nsString* aMime) override;
  virtual mozilla::ipc::IPCResult RecvURLDocTypeMimeType(const uint64_t& aID,
                                                         nsString* aURL,
                                                         nsString* aDocType,
                                                         nsString* aMimeType) override;

  virtual mozilla::ipc::IPCResult RecvAccessibleAtPoint(const uint64_t& aID,
                                                        const int32_t& aX,
                                                        const int32_t& aY,
                                                        const bool& aNeedsScreenCoords,
                                                        const uint32_t& aWhich,
                                                        uint64_t* aResult,
                                                        bool* aOk) override;

  virtual mozilla::ipc::IPCResult RecvExtents(const uint64_t& aID,
                                              const bool& aNeedsScreenCoords,
                                              int32_t* aX,
                                              int32_t* aY,
                                              int32_t* aWidth,
                                              int32_t* aHeight) override;
  virtual mozilla::ipc::IPCResult RecvDOMNodeID(const uint64_t& aID, nsString* aDOMNodeID) override;
private:

  Accessible* IdToAccessible(const uint64_t& aID) const;
  Accessible* IdToAccessibleLink(const uint64_t& aID) const;
  Accessible* IdToAccessibleSelect(const uint64_t& aID) const;
  HyperTextAccessible* IdToHyperTextAccessible(const uint64_t& aID) const;
  TextLeafAccessible* IdToTextLeafAccessible(const uint64_t& aID) const;
  ImageAccessible* IdToImageAccessible(const uint64_t& aID) const;
  TableCellAccessible* IdToTableCellAccessible(const uint64_t& aID) const;
  TableAccessible* IdToTableAccessible(const uint64_t& aID) const;

  bool PersistentPropertiesToArray(nsIPersistentProperties* aProps,
                                   nsTArray<Attribute>* aAttributes);
};

}
}

#endif
