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
class ImageAccessible;

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
    mDoc->SetIPCDoc(nullptr);
    MOZ_COUNT_DTOR(DocAccessibleChild);
  }

  void ShowEvent(AccShowEvent* aShowEvent);

  /*
   * Return the state for the accessible with given ID.
   */
  virtual bool RecvState(const uint64_t& aID, uint64_t* aState) override;

  /*
   * Get the name for the accessible with given id.
   */
  virtual bool RecvName(const uint64_t& aID, nsString* aName) override;

  virtual bool RecvValue(const uint64_t& aID, nsString* aValue) override;
  
  /*
   * Get the description for the accessible with given id.
   */
  virtual bool RecvDescription(const uint64_t& aID, nsString* aDesc) override;
  virtual bool RecvRelationByType(const uint64_t& aID, const uint32_t& aType,
                                  nsTArray<uint64_t>* aTargets) override;
  virtual bool RecvRelations(const uint64_t& aID,
                             nsTArray<RelationTargets>* aRelations)
    override;

  virtual bool RecvAttributes(const uint64_t& aID,
                              nsTArray<Attribute> *aAttributes) override;

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
                                 const int32_t& aEndOffset, nsString* aText)
    override;

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

  virtual bool RecvReplaceText(const uint64_t& aID,
                               const nsString& aText) override;

  virtual bool RecvInsertText(const uint64_t& aID,
                              const nsString& aText,
                              const int32_t& aPosition) override;

  virtual bool RecvCopyText(const uint64_t& aID,
                            const int32_t& aStartPos,
                            const int32_t& aEndPos) override;

  virtual bool RecvCutText(const uint64_t& aID,
                           const int32_t& aStartPos,
                           const int32_t& aEndPos) override;

  virtual bool RecvDeleteText(const uint64_t& aID,
                              const int32_t& aStartPos,
                              const int32_t& aEndPos) override;

  virtual bool RecvPasteText(const uint64_t& aID,
                             const int32_t& aPosition) override;

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

private:

  Accessible* IdToAccessible(const uint64_t& aID) const;
  Accessible* IdToAccessibleLink(const uint64_t& aID) const;
  HyperTextAccessible* IdToHyperTextAccessible(const uint64_t& aID) const;
  ImageAccessible* IdToImageAccessible(const uint64_t& aID) const;
  TableCellAccessible* IdToTableCellAccessible(const uint64_t& aID) const;

  bool PersistentPropertiesToArray(nsIPersistentProperties* aProps,
                                   nsTArray<Attribute>* aAttributes);

  DocAccessible* mDoc;
};

}
}

#endif
