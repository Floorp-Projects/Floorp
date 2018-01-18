/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TextServicesDocument_h
#define mozilla_TextServicesDocument_h

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIEditActionListener.h"
#include "nsISupportsImpl.h"
#include "nsITextServicesDocument.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nscore.h"

class nsIContent;
class nsIContentIterator;
class nsIDOMCharacterData;
class nsIDOMDocument;
class nsIDOMNode;
class nsIEditor;
class nsINode;
class nsISelection;
class nsISelectionController;
class nsITextServicesFilter;

// 019718E3-CDB5-11d2-8D3C-000000000000
#define NS_TEXTSERVICESDOCUMENT_CID \
  { 0x019718e3, 0xcdb5, 0x11d2, \
    { 0x8d, 0x3c, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }

namespace mozilla {

class OffsetEntry;
class TextEditor;

/**
 * TODO: Explain what this class manages and rename it to better name.
 */
class TextServicesDocument final : public nsITextServicesDocument
                                 , public nsIEditActionListener
{
private:
  enum class IteratorStatus : uint8_t
  {
    // No iterator (I), or iterator doesn't point to anything valid.
    eDone = 0,
    // I points to first text node (TN) in current block (CB).
    eValid,
    // No TN in CB, I points to first TN in prev block.
    ePrev,
    // No TN in CB, I points to first TN in next block.
    eNext,
  };

  nsCOMPtr<nsIDOMDocument> mDOMDocument;
  nsCOMPtr<nsISelectionController> mSelCon;
  RefPtr<TextEditor> mTextEditor;
  nsCOMPtr<nsIContentIterator> mIterator;
  nsCOMPtr<nsIContent> mPrevTextBlock;
  nsCOMPtr<nsIContent> mNextTextBlock;
  nsTArray<OffsetEntry*> mOffsetTable;
  RefPtr<nsRange> mExtent;
  nsCOMPtr<nsITextServicesFilter> mTxtSvcFilter;

  int32_t mSelStartIndex;
  int32_t mSelStartOffset;
  int32_t mSelEndIndex;
  int32_t mSelEndOffset;

  IteratorStatus mIteratorStatus;

protected:
  virtual ~TextServicesDocument();

public:
  TextServicesDocument();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(TextServicesDocument,
                                           nsITextServicesDocument)

  /* nsITextServicesDocument method implementations. */
  NS_IMETHOD InitWithEditor(nsIEditor* aEditor) override;
  NS_IMETHOD GetDocument(nsIDOMDocument** aDoc) override;
  NS_IMETHOD SetExtent(nsRange* aRange) override;
  NS_IMETHOD ExpandRangeToWordBoundaries(nsRange* aRange) override;
  NS_IMETHOD SetFilter(nsITextServicesFilter* aFilter) override;
  NS_IMETHOD GetCurrentTextBlock(nsString* aStr) override;
  NS_IMETHOD FirstBlock() override;
  NS_IMETHOD LastSelectedBlock(TSDBlockSelectionStatus* aSelStatus,
                               int32_t* aSelOffset,
                               int32_t* aSelLength) override;
  NS_IMETHOD PrevBlock() override;
  NS_IMETHOD NextBlock() override;
  NS_IMETHOD IsDone(bool* aIsDone) override;
  NS_IMETHOD SetSelection(int32_t aOffset, int32_t aLength) override;
  NS_IMETHOD ScrollSelectionIntoView() override;
  NS_IMETHOD DeleteSelection() override;
  NS_IMETHOD InsertText(const nsString* aText) override;

  /* nsIEditActionListener method implementations. */
  NS_DECL_NSIEDITACTIONLISTENER

  static nsresult GetRangeEndPoints(nsRange* aRange,
                                    nsINode** aStartContainer,
                                    int32_t* aStartOffset,
                                    nsINode** aEndContainer,
                                    int32_t* aEndOffset);

private:
  nsresult CreateContentIterator(nsRange* aRange,
                                 nsIContentIterator** aIterator);

  already_AddRefed<nsINode> GetDocumentContentRootNode();
  already_AddRefed<nsRange> CreateDocumentContentRange();
  already_AddRefed<nsRange> CreateDocumentContentRootToNodeOffsetRange(
                              nsINode* aParent,
                              uint32_t aOffset,
                              bool aToStart);
  nsresult CreateDocumentContentIterator(nsIContentIterator** aIterator);

  nsresult AdjustContentIterator();

  static nsresult FirstTextNode(nsIContentIterator* aIterator,
                                IteratorStatus* aIteratorStatus);
  static nsresult LastTextNode(nsIContentIterator* aIterator,
                               IteratorStatus* aIteratorStatus);

  static nsresult FirstTextNodeInCurrentBlock(nsIContentIterator* aIterator);
  static nsresult FirstTextNodeInPrevBlock(nsIContentIterator* aIterator);
  static nsresult FirstTextNodeInNextBlock(nsIContentIterator* aIterator);

  nsresult GetFirstTextNodeInPrevBlock(nsIContent** aContent);
  nsresult GetFirstTextNodeInNextBlock(nsIContent** aContent);

  static bool IsBlockNode(nsIContent* aContent);
  static bool IsTextNode(nsIContent* aContent);
  static bool IsTextNode(nsIDOMNode* aNode);

  static bool DidSkip(nsIContentIterator* aFilteredIter);
  static void ClearDidSkip(nsIContentIterator* aFilteredIter);

  static bool HasSameBlockNodeParent(nsIContent* aContent1,
                                     nsIContent* aContent2);

  nsresult SetSelectionInternal(int32_t aOffset, int32_t aLength,
                                bool aDoUpdate);
  nsresult GetSelection(TSDBlockSelectionStatus* aSelStatus,
                        int32_t* aSelOffset, int32_t* aSelLength);
  nsresult GetCollapsedSelection(TSDBlockSelectionStatus* aSelStatus,
                                 int32_t* aSelOffset, int32_t* aSelLength);
  nsresult GetUncollapsedSelection(TSDBlockSelectionStatus* aSelStatus,
                                   int32_t* aSelOffset, int32_t* aSelLength);

  bool SelectionIsCollapsed();
  bool SelectionIsValid();

  static nsresult CreateOffsetTable(nsTArray<OffsetEntry*>* aOffsetTable,
                                    nsIContentIterator* aIterator,
                                    IteratorStatus* aIteratorStatus,
                                    nsRange* aIterRange, nsString* aStr);
  static nsresult ClearOffsetTable(nsTArray<OffsetEntry*>* aOffsetTable);

  static nsresult NodeHasOffsetEntry(nsTArray<OffsetEntry*>* aOffsetTable,
                                     nsINode* aNode,
                                     bool* aHasEntry,
                                     int32_t* aEntryIndex);

  nsresult RemoveInvalidOffsetEntries();
  nsresult SplitOffsetEntry(int32_t aTableIndex, int32_t aOffsetIntoEntry);

  static nsresult FindWordBounds(nsTArray<OffsetEntry*>* aOffsetTable,
                                 nsString* aBlockStr,
                                 nsINode* aNode, int32_t aNodeOffset,
                                 nsINode** aWordStartNode,
                                 int32_t* aWordStartOffset,
                                 nsINode** aWordEndNode,
                                 int32_t* aWordEndOffset);
};

} // namespace mozilla

#endif // #ifndef mozilla_TextServicesDocument_h
