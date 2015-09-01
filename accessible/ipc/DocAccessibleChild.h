/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessibleChild_h
#define mozilla_a11y_DocAccessibleChild_h

#include "mozilla/a11y/DocAccessible.h"
#include "mozilla/a11y/PDocAccessibleChild.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace a11y {
class Accessible;
class HyperTextAccessible;
class TextLeafAccessible;
class ImageAccessible;
class TableAccessible;
class TableCellAccessible;
class AccShowEvent;

  /*
   * These objects handle content side communication for an accessible document,
   * and their lifetime is the same as the document they represent.
   */
class DocAccessibleChild : public PDocAccessibleChild
{
public:
  explicit DocAccessibleChild(DocAccessible* aDoc) :
    mDoc(aDoc)
  { MOZ_COUNT_CTOR(DocAccessibleChild); }
  ~DocAccessibleChild()
  {
    // Shutdown() should have been called, but maybe it isn't if the process is
    // killed?
    MOZ_ASSERT(!mDoc);
    if (mDoc)
      mDoc->SetIPCDoc(nullptr);
    MOZ_COUNT_DTOR(DocAccessibleChild);
  }

  void Shutdown()
  {
    mDoc->SetIPCDoc(nullptr);
    mDoc = nullptr;
    SendShutdown();
  }

  virtual void ActorDestroy(ActorDestroyReason) override
  {
    if (!mDoc)
      return;

    mDoc->SetIPCDoc(nullptr);
    mDoc = nullptr;
  }

  void ShowEvent(AccShowEvent* aShowEvent);

  /*
   * Return the state for the accessible with given ID.
   */
  virtual bool RecvState(const uint64_t& aID, uint64_t* aState) override;

  /*
   * Return the native state for the accessible with given ID.
   */
  virtual bool RecvNativeState(const uint64_t& aID, uint64_t* aState) override;

  /*
   * Get the name for the accessible with given id.
   */
  virtual bool RecvName(const uint64_t& aID, nsString* aName) override;

  virtual bool RecvValue(const uint64_t& aID, nsString* aValue) override;

  virtual bool RecvHelp(const uint64_t& aID, nsString* aHelp) override;

  /*
   * Get the description for the accessible with given id.
   */
  virtual bool RecvDescription(const uint64_t& aID, nsString* aDesc) override;
  virtual bool RecvRelationByType(const uint64_t& aID, const uint32_t& aType,
                                  nsTArray<uint64_t>* aTargets) override;
  virtual bool RecvRelations(const uint64_t& aID,
                             nsTArray<RelationTargets>* aRelations)
    override;

  virtual bool RecvIsSearchbox(const uint64_t& aID, bool* aRetVal) override;

  virtual bool RecvLandmarkRole(const uint64_t& aID, nsString* aLandmark) override;

  virtual bool RecvARIARoleAtom(const uint64_t& aID, nsString* aRole) override;

  virtual bool RecvGetLevelInternal(const uint64_t& aID, int32_t* aLevel) override;

  virtual bool RecvAttributes(const uint64_t& aID,
                              nsTArray<Attribute> *aAttributes) override;

  virtual bool RecvCaretLineNumber(const uint64_t& aID, int32_t* aLineNumber)
    override;
  virtual bool RecvCaretOffset(const uint64_t& aID, int32_t* aOffset)
    override;
  virtual bool RecvSetCaretOffset(const uint64_t& aID, const int32_t& aOffset,
                                  bool* aValid) override;

  virtual bool RecvCharacterCount(const uint64_t& aID, int32_t* aCount)
     override;
  virtual bool RecvSelectionCount(const uint64_t& aID, int32_t* aCount)
     override;

  virtual bool RecvTextSubstring(const uint64_t& aID,
                                 const int32_t& aStartOffset,
                                 const int32_t& aEndOffset, nsString* aText,
                                 bool* aValid) override;

  virtual bool RecvGetTextAfterOffset(const uint64_t& aID,
                                      const int32_t& aOffset,
                                      const int32_t& aBoundaryType,
                                      nsString* aText, int32_t* aStartOffset,
                                      int32_t* aEndOffset) override;
  virtual bool RecvGetTextAtOffset(const uint64_t& aID,
                                   const int32_t& aOffset,
                                   const int32_t& aBoundaryType,
                                   nsString* aText, int32_t* aStartOffset,
                                   int32_t* aEndOffset) override;
  virtual bool RecvGetTextBeforeOffset(const uint64_t& aID,
                                       const int32_t& aOffset,
                                       const int32_t& aBoundaryType,
                                       nsString* aText, int32_t* aStartOffset,
                                       int32_t* aEndOffset) override;

  virtual bool RecvCharAt(const uint64_t& aID,
                          const int32_t& aOffset,
                          uint16_t* aChar) override;

  virtual bool RecvTextAttributes(const uint64_t& aID,
                                  const bool& aIncludeDefAttrs,
                                  const int32_t& aOffset,
                                  nsTArray<Attribute>* aAttributes,
                                  int32_t* aStartOffset,
                                  int32_t* aEndOffset)
    override;

  virtual bool RecvDefaultTextAttributes(const uint64_t& aID,
                                         nsTArray<Attribute>* aAttributes)
    override;

  virtual bool RecvTextBounds(const uint64_t& aID,
                              const int32_t& aStartOffset,
                              const int32_t& aEndOffset,
                              const uint32_t& aCoordType,
                              nsIntRect* aRetVal) override;

  virtual bool RecvCharBounds(const uint64_t& aID,
                              const int32_t& aOffset,
                              const uint32_t& aCoordType,
                              nsIntRect* aRetVal) override;

  virtual bool RecvOffsetAtPoint(const uint64_t& aID,
                                 const int32_t& aX,
                                 const int32_t& aY,
                                 const uint32_t& aCoordType,
                                 int32_t* aRetVal) override;

  virtual bool RecvSelectionBoundsAt(const uint64_t& aID,
                                     const int32_t& aSelectionNum,
                                     bool* aSucceeded,
                                     nsString* aData,
                                     int32_t* aStartOffset,
                                     int32_t* aEndOffset) override;

  virtual bool RecvSetSelectionBoundsAt(const uint64_t& aID,
                                        const int32_t& aSelectionNum,
                                        const int32_t& aStartOffset,
                                        const int32_t& aEndOffset,
                                        bool* aSucceeded) override;

  virtual bool RecvAddToSelection(const uint64_t& aID,
                                  const int32_t& aStartOffset,
                                  const int32_t& aEndOffset,
                                  bool* aSucceeded) override;

  virtual bool RecvRemoveFromSelection(const uint64_t& aID,
                                       const int32_t& aSelectionNum,
                                       bool* aSucceeded) override;

  virtual bool RecvScrollSubstringTo(const uint64_t& aID,
                                     const int32_t& aStartOffset,
                                     const int32_t& aEndOffset,
                                     const uint32_t& aScrollType) override;

  virtual bool RecvScrollSubstringToPoint(const uint64_t& aID,
                                          const int32_t& aStartOffset,
                                          const int32_t& aEndOffset,
                                          const uint32_t& aCoordinateType,
                                          const int32_t& aX,
                                          const int32_t& aY) override;

  virtual bool RecvText(const uint64_t& aID,
                        nsString* aText) override;

  virtual bool RecvReplaceText(const uint64_t& aID,
                               const nsString& aText) override;

  virtual bool RecvInsertText(const uint64_t& aID,
                              const nsString& aText,
                              const int32_t& aPosition, bool* aValid) override;

  virtual bool RecvCopyText(const uint64_t& aID,
                            const int32_t& aStartPos,
                            const int32_t& aEndPos, bool* aValid) override;

  virtual bool RecvCutText(const uint64_t& aID,
                           const int32_t& aStartPos,
                           const int32_t& aEndPos, bool* aValid) override;

  virtual bool RecvDeleteText(const uint64_t& aID,
                              const int32_t& aStartPos,
                              const int32_t& aEndPos, bool* aValid) override;

  virtual bool RecvPasteText(const uint64_t& aID,
                             const int32_t& aPosition, bool* aValid) override;

  virtual bool RecvImagePosition(const uint64_t& aID,
                                 const uint32_t& aCoordType,
                                 nsIntPoint* aRetVal) override;

  virtual bool RecvImageSize(const uint64_t& aID,
                             nsIntSize* aRetVal) override;

  virtual bool RecvStartOffset(const uint64_t& aID,
                               uint32_t* aRetVal,
                               bool* aOk) override;
  virtual bool RecvEndOffset(const uint64_t& aID,
                             uint32_t* aRetVal,
                             bool* aOk) override;
  virtual bool RecvIsLinkValid(const uint64_t& aID,
                               bool* aRetVal) override;
  virtual bool RecvAnchorCount(const uint64_t& aID,
                               uint32_t* aRetVal, bool* aOk) override;
  virtual bool RecvAnchorURIAt(const uint64_t& aID,
                               const uint32_t& aIndex,
                               nsCString* aURI,
                               bool* aOk) override;
  virtual bool RecvAnchorAt(const uint64_t& aID,
                            const uint32_t& aIndex,
                            uint64_t* aIDOfAnchor,
                            bool* aOk) override;

  virtual bool RecvLinkCount(const uint64_t& aID,
                             uint32_t* aCount) override;

  virtual bool RecvLinkAt(const uint64_t& aID,
                          const uint32_t& aIndex,
                          uint64_t* aIDOfLink,
                          bool* aOk) override;

  virtual bool RecvLinkIndexOf(const uint64_t& aID,
                               const uint64_t& aLinkID,
                               int32_t* aIndex) override;

  virtual bool RecvLinkIndexAtOffset(const uint64_t& aID,
                                     const uint32_t& aOffset,
                                     int32_t* aIndex) override;

  virtual bool RecvTableOfACell(const uint64_t& aID,
                                uint64_t* aTableID,
                                bool* aOk) override;

  virtual bool RecvColIdx(const uint64_t& aID, uint32_t* aIndex) override;

  virtual bool RecvRowIdx(const uint64_t& aID, uint32_t* aIndex) override;

  virtual bool RecvColExtent(const uint64_t& aID, uint32_t* aExtent) override;

  virtual bool RecvRowExtent(const uint64_t& aID, uint32_t* aExtent) override;

  virtual bool RecvColHeaderCells(const uint64_t& aID,
                                  nsTArray<uint64_t>* aCells) override;

  virtual bool RecvRowHeaderCells(const uint64_t& aID,
                                  nsTArray<uint64_t>* aCells) override;

  virtual bool RecvIsCellSelected(const uint64_t& aID,
                                  bool* aSelected) override;

  virtual bool RecvTableCaption(const uint64_t& aID,
                                uint64_t* aCaptionID,
                                bool* aOk) override;
  virtual bool RecvTableSummary(const uint64_t& aID,
                                nsString* aSummary) override;
  virtual bool RecvTableColumnCount(const uint64_t& aID,
                                    uint32_t* aColCount) override;
  virtual bool RecvTableRowCount(const uint64_t& aID,
                                 uint32_t* aRowCount) override;
  virtual bool RecvTableCellAt(const uint64_t& aID,
                               const uint32_t& aRow,
                               const uint32_t& aCol,
                               uint64_t* aCellID,
                               bool* aOk) override;
  virtual bool RecvTableCellIndexAt(const uint64_t& aID,
                                    const uint32_t& aRow,
                                    const uint32_t& aCol,
                                    int32_t* aIndex) override;
  virtual bool RecvTableColumnIndexAt(const uint64_t& aID,
                                      const uint32_t& aCellIndex,
                                      int32_t* aCol) override;
  virtual bool RecvTableRowIndexAt(const uint64_t& aID,
                                   const uint32_t& aCellIndex,
                                   int32_t* aRow) override;
  virtual bool RecvTableRowAndColumnIndicesAt(const uint64_t& aID,
                                             const uint32_t& aCellIndex,
                                             int32_t* aRow,
                                             int32_t* aCol) override;
  virtual bool RecvTableColumnExtentAt(const uint64_t& aID,
                                       const uint32_t& aRow,
                                       const uint32_t& aCol,
                                       uint32_t* aExtent) override;
  virtual bool RecvTableRowExtentAt(const uint64_t& aID,
                                    const uint32_t& aRow,
                                    const uint32_t& aCol,
                                    uint32_t* aExtent) override;
  virtual bool RecvTableColumnDescription(const uint64_t& aID,
                                          const uint32_t& aCol,
                                          nsString* aDescription) override;
  virtual bool RecvTableRowDescription(const uint64_t& aID,
                                       const uint32_t& aRow,
                                       nsString* aDescription) override;
  virtual bool RecvTableColumnSelected(const uint64_t& aID,
                                       const uint32_t& aCol,
                                       bool* aSelected) override;
  virtual bool RecvTableRowSelected(const uint64_t& aID,
                                    const uint32_t& aRow,
                                    bool* aSelected) override;
  virtual bool RecvTableCellSelected(const uint64_t& aID,
                                     const uint32_t& aRow,
                                     const uint32_t& aCol,
                                     bool* aSelected) override;
  virtual bool RecvTableSelectedCellCount(const uint64_t& aID,
                                          uint32_t* aSelectedCells) override;
  virtual bool RecvTableSelectedColumnCount(const uint64_t& aID,
                                            uint32_t* aSelectedColumns) override;
  virtual bool RecvTableSelectedRowCount(const uint64_t& aID,
                                         uint32_t* aSelectedRows) override;
  virtual bool RecvTableSelectedCells(const uint64_t& aID,
                                      nsTArray<uint64_t>* aCellIDs) override;
  virtual bool RecvTableSelectedCellIndices(const uint64_t& aID,
                                            nsTArray<uint32_t>* aCellIndices) override;
  virtual bool RecvTableSelectedColumnIndices(const uint64_t& aID,
                                              nsTArray<uint32_t>* aColumnIndices) override;
  virtual bool RecvTableSelectedRowIndices(const uint64_t& aID,
                                           nsTArray<uint32_t>* aRowIndices) override;
  virtual bool RecvTableSelectColumn(const uint64_t& aID,
                                     const uint32_t& aCol) override;
  virtual bool RecvTableSelectRow(const uint64_t& aID,
                                  const uint32_t& aRow) override;
  virtual bool RecvTableUnselectColumn(const uint64_t& aID,
                                       const uint32_t& aCol) override;
  virtual bool RecvTableUnselectRow(const uint64_t& aID,
                                    const uint32_t& aRow) override;
  virtual bool RecvTableIsProbablyForLayout(const uint64_t& aID,
                                            bool* aForLayout) override;

  virtual bool RecvSelectedItems(const uint64_t& aID,
                                 nsTArray<uint64_t>* aSelectedItemIDs) override;

  virtual bool RecvSelectedItemCount(const uint64_t& aID,
                                     uint32_t* aCount) override;

  virtual bool RecvGetSelectedItem(const uint64_t& aID,
                                   const uint32_t& aIndex,
                                   uint64_t* aSelected,
                                   bool* aOk) override;

  virtual bool RecvIsItemSelected(const uint64_t& aID,
                                  const uint32_t& aIndex,
                                  bool* aSelected) override;

  virtual bool RecvAddItemToSelection(const uint64_t& aID,
                                      const uint32_t& aIndex,
                                      bool* aSuccess) override;

  virtual bool RecvRemoveItemFromSelection(const uint64_t& aID,
                                           const uint32_t& aIndex,
                                           bool* aSuccess) override;

  virtual bool RecvSelectAll(const uint64_t& aID,
                             bool* aSuccess) override;

  virtual bool RecvUnselectAll(const uint64_t& aID,
                               bool* aSuccess) override;

  virtual bool RecvDoAction(const uint64_t& aID,
                            const uint8_t& aIndex,
                            bool* aSuccess) override;

  virtual bool RecvActionCount(const uint64_t& aID,
                               uint8_t* aCount) override;

  virtual bool RecvActionDescriptionAt(const uint64_t& aID,
                                       const uint8_t& aIndex,
                                       nsString* aDescription) override;

  virtual bool RecvActionNameAt(const uint64_t& aID,
                                const uint8_t& aIndex,
                                nsString* aName) override;

  virtual bool RecvAccessKey(const uint64_t& aID,
                             uint32_t* aKey,
                             uint32_t* aModifierMask) override;

  virtual bool RecvKeyboardShortcut(const uint64_t& aID,
                                    uint32_t* aKey,
                                    uint32_t* aModifierMask) override;

  virtual bool RecvCurValue(const uint64_t& aID,
                            double* aValue) override;

  virtual bool RecvSetCurValue(const uint64_t& aID,
                               const double& aValue,
                               bool* aRetVal) override;

  virtual bool RecvMinValue(const uint64_t& aID,
                            double* aValue) override;

  virtual bool RecvMaxValue(const uint64_t& aID,
                            double* aValue) override;

  virtual bool RecvStep(const uint64_t& aID,
                        double* aStep) override;

  virtual bool RecvTakeFocus(const uint64_t& aID) override;

  virtual bool RecvEmbeddedChildCount(const uint64_t& aID, uint32_t* aCount)
    override final;

  virtual bool RecvIndexOfEmbeddedChild(const uint64_t& aID,
                                        const uint64_t& aChildID,
                                        uint32_t* aChildIdx) override final;

  virtual bool RecvEmbeddedChildAt(const uint64_t& aID, const uint32_t& aIdx,
                                   uint64_t* aChildID) override final;

  virtual bool RecvFocusedChild(const uint64_t& aID,
                                uint64_t* aChild,
                                bool* aOk) override;

  virtual bool RecvLanguage(const uint64_t& aID, nsString* aLocale) override;
  virtual bool RecvDocType(const uint64_t& aID, nsString* aType) override;
  virtual bool RecvTitle(const uint64_t& aID, nsString* aTitle) override;
  virtual bool RecvURL(const uint64_t& aID, nsString* aURL) override;
  virtual bool RecvMimeType(const uint64_t& aID, nsString* aMime) override;
  virtual bool RecvURLDocTypeMimeType(const uint64_t& aID,
                                      nsString* aURL,
                                      nsString* aDocType,
                                      nsString* aMimeType) override;

  virtual bool RecvAccessibleAtPoint(const uint64_t& aID,
                                     const int32_t& aX,
                                     const int32_t& aY,
                                     const bool& aNeedsScreenCoords,
                                     const uint32_t& aWhich,
                                     uint64_t* aResult,
                                     bool* aOk) override;

  virtual bool RecvExtents(const uint64_t& aID,
                           const bool& aNeedsScreenCoords,
                           int32_t* aX,
                           int32_t* aY,
                           int32_t* aWidth,
                           int32_t* aHeight) override;
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

  DocAccessible* mDoc;
};

}
}

#endif
