/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WSRunObject_h
#define WSRunObject_h

#include "mozilla/dom/Text.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorBase.h"
#include "mozilla/EditorDOMPoint.h"  // for EditorDOMPoint

namespace mozilla {

class HTMLEditor;
class HTMLEditRules;

// class WSRunObject represents the entire whitespace situation
// around a given point.  It collects up a list of nodes that contain
// whitespace and categorizes in up to 3 different WSFragments (detailed
// below).  Each WSFragment is a collection of whitespace that is
// either all insignificant, or that is significant.  A WSFragment could
// consist of insignificant whitespace because it is after a block
// boundary or after a break.  Or it could be insignificant because it
// is before a block.  Or it could be significant because it is
// surrounded by text, or starts and ends with nbsps, etc.

// Throughout I refer to LeadingWS, NormalWS, TrailingWS.  LeadingWS &
// TrailingWS are runs of ascii ws that are insignificant (do not render)
// because they are adjacent to block boundaries, or after a break.  NormalWS is
// ws that does cause soem rendering.  Note that not all the ws in a NormalWS
// run need render.  For example, two ascii spaces surrounded by text on both
// sides will only render as one space (in non-preformatted stlye html), yet
// both spaces count as NormalWS.  Together, they render as the one visible
// space.

/**
 * A type-safe bitfield indicating various types of whitespace or other things.
 * Used as a member variable in WSRunObject and WSFragment.
 *
 * XXX: If this idea is useful in other places, we should generalize it using a
 * template.
 */
class WSType {
 public:
  enum Enum {
    none = 0,
    leadingWS = 1,        // leading insignificant ws, ie, after block or br
    trailingWS = 1 << 1,  // trailing insignificant ws, ie, before block
    normalWS = 1 << 2,    // normal significant ws, ie, after text, image, ...
    text = 1 << 3,        // indicates regular (non-ws) text
    special = 1 << 4,     // indicates an inline non-container, like image
    br = 1 << 5,          // indicates a br node
    otherBlock = 1 << 6,  // indicates a block other than one ws run is in
    thisBlock = 1 << 7,   // indicates the block ws run is in
    block = otherBlock | thisBlock  // block found
  };

  /**
   * Implicit constructor, because the enums are logically just WSTypes
   * themselves, and are only a separate type because there's no other obvious
   * way to name specific WSType values.
   */
  MOZ_IMPLICIT WSType(const Enum& aEnum = none) : mEnum(aEnum) {}

  // operator==, &, and | need to access mEnum
  friend bool operator==(const WSType& aLeft, const WSType& aRight);
  friend const WSType operator&(const WSType& aLeft, const WSType& aRight);
  friend const WSType operator|(const WSType& aLeft, const WSType& aRight);
  WSType& operator=(const WSType& aOther) {
    // This handles self-assignment fine
    mEnum = aOther.mEnum;
    return *this;
  }
  WSType& operator&=(const WSType& aOther) {
    mEnum &= aOther.mEnum;
    return *this;
  }
  WSType& operator|=(const WSType& aOther) {
    mEnum |= aOther.mEnum;
    return *this;
  }

 private:
  uint16_t mEnum;
  void bool_conversion_helper() {}

 public:
  // Allow boolean conversion with no numeric conversion
  typedef void (WSType::*bool_type)();
  operator bool_type() const {
    return mEnum ? &WSType::bool_conversion_helper : nullptr;
  }
};

/**
 * These are declared as global functions so "WSType::Enum == WSType" et al.
 * will work using the implicit constructor.
 */
inline bool operator==(const WSType& aLeft, const WSType& aRight) {
  return aLeft.mEnum == aRight.mEnum;
}

inline bool operator!=(const WSType& aLeft, const WSType& aRight) {
  return !(aLeft == aRight);
}

inline const WSType operator&(const WSType& aLeft, const WSType& aRight) {
  WSType ret;
  ret.mEnum = aLeft.mEnum & aRight.mEnum;
  return ret;
}

inline const WSType operator|(const WSType& aLeft, const WSType& aRight) {
  WSType ret;
  ret.mEnum = aLeft.mEnum | aRight.mEnum;
  return ret;
}

/**
 * Make sure that & and | of WSType::Enum creates a WSType instead of an int,
 * because operators between WSType and int shouldn't work
 */
inline const WSType operator&(const WSType::Enum& aLeft,
                              const WSType::Enum& aRight) {
  return WSType(aLeft) & WSType(aRight);
}

inline const WSType operator|(const WSType::Enum& aLeft,
                              const WSType::Enum& aRight) {
  return WSType(aLeft) | WSType(aRight);
}

class MOZ_STACK_CLASS WSRunScanner {
 public:
  /**
   * The constructors take 2 DOM points.  They represent a range in an editing
   * host.  aScanEndPoint (aScanEndNode and aScanEndOffset) must be later
   * point from aScanStartPoint (aScanStartNode and aScanStartOffset).
   * The end point is currently used only with InsertText().  Therefore,
   * currently, this assumes that the range does not cross block boundary.  If
   * you use aScanEndPoint newly, please test enough.
   */
  template <typename PT, typename CT>
  WSRunScanner(const HTMLEditor* aHTMLEditor,
               const EditorDOMPointBase<PT, CT>& aScanStartPoint,
               const EditorDOMPointBase<PT, CT>& aScanEndPoint);
  template <typename PT, typename CT>
  WSRunScanner(const HTMLEditor* aHTMLEditor,
               const EditorDOMPointBase<PT, CT>& aScanStartPoint)
      : WSRunScanner(aHTMLEditor, aScanStartPoint, aScanStartPoint) {}
  WSRunScanner(const HTMLEditor* aHTMLEditor, nsINode* aScanStartNode,
               int32_t aScanStartOffset, nsINode* aScanEndNode,
               int32_t aScanEndOffset)
      : WSRunScanner(aHTMLEditor,
                     EditorRawDOMPoint(aScanStartNode, aScanStartOffset),
                     EditorRawDOMPoint(aScanEndNode, aScanEndOffset)) {}
  WSRunScanner(const HTMLEditor* aHTMLEditor, nsINode* aScanStartNode,
               int32_t aScanStartOffset)
      : WSRunScanner(aHTMLEditor,
                     EditorRawDOMPoint(aScanStartNode, aScanStartOffset),
                     EditorRawDOMPoint(aScanStartNode, aScanStartOffset)) {}
  ~WSRunScanner();

  // NextVisibleNode() returns the first piece of visible thing after aPoint.
  // If there is no visible ws qualifying it returns what is after the ws run.
  // If outVisNode and/or outvisOffset is unused, callers can use nullptr.
  // Note that {outVisNode,outVisOffset} is set to just BEFORE the visible
  // object. Also outVisOffset might be invalid offset unless outVisNode is
  // end reason node.
  template <typename PT, typename CT>
  void NextVisibleNode(const EditorDOMPointBase<PT, CT>& aPoint,
                       nsCOMPtr<nsINode>* outVisNode, int32_t* outVisOffset,
                       WSType* outType) const;

  template <typename PT, typename CT>
  void NextVisibleNode(const EditorDOMPointBase<PT, CT>& aPoint,
                       WSType* outType) const {
    NextVisibleNode(aPoint, nullptr, nullptr, outType);
  }

  // PriorVisibleNode() returns the first piece of visible thing before aPoint.
  // If there is no visible ws qualifying it returns what is before the ws run.
  // If outVisNode and/or outvisOffset is unused, callers can use nullptr.
  // Note that {outVisNode,outVisOffset} is set to just AFTER the visible
  // object. Also outVisOffset might be invalid offset unless outVisNode is
  // start reason node.
  template <typename PT, typename CT>
  void PriorVisibleNode(const EditorDOMPointBase<PT, CT>& aPoint,
                        nsCOMPtr<nsINode>* outVisNode, int32_t* outVisOffset,
                        WSType* outType) const;

  template <typename PT, typename CT>
  void PriorVisibleNode(const EditorDOMPointBase<PT, CT>& aPoint,
                        WSType* outType) const {
    PriorVisibleNode(aPoint, nullptr, nullptr, outType);
  }

 protected:
  // WSFragment represents a single run of ws (all leadingws, or all normalws,
  // or all trailingws, or all leading+trailingws).  Note that this single run
  // may still span multiple nodes.
  struct WSFragment final {
    nsCOMPtr<nsINode> mStartNode;  // node where ws run starts
    nsCOMPtr<nsINode> mEndNode;    // node where ws run ends
    int32_t mStartOffset;          // offset where ws run starts
    int32_t mEndOffset;            // offset where ws run ends
    // type of ws, and what is to left and right of it
    WSType mType, mLeftType, mRightType;
    // other ws runs to left or right.  may be null.
    WSFragment *mLeft, *mRight;

    WSFragment()
        : mStartOffset(0), mEndOffset(0), mLeft(nullptr), mRight(nullptr) {}

    EditorRawDOMPoint StartPoint() const {
      return EditorRawDOMPoint(mStartNode, mStartOffset);
    }
    EditorRawDOMPoint EndPoint() const {
      return EditorRawDOMPoint(mEndNode, mEndOffset);
    }
  };

  // A WSPoint struct represents a unique location within the ws run.  It is
  // always within a textnode that is one of the nodes stored in the list
  // in the wsRunObject.  For convenience, the character at that point is also
  // stored in the struct.
  struct MOZ_STACK_CLASS WSPoint final {
    RefPtr<dom::Text> mTextNode;
    uint32_t mOffset;
    char16_t mChar;

    WSPoint() : mTextNode(nullptr), mOffset(0), mChar(0) {}

    WSPoint(dom::Text* aTextNode, int32_t aOffset, char16_t aChar)
        : mTextNode(aTextNode), mOffset(aOffset), mChar(aChar) {}
  };

  /**
   * FindNearestRun() looks for a WSFragment which is closest to specified
   * direction from aPoint.
   *
   * @param aPoint      The point to start to look for.
   * @param aForward    true if caller needs to look for a WSFragment after the
   *                    point in the DOM tree.  Otherwise, i.e., before the
   *                    point, false.
   * @return            Found WSFragment instance.
   *                    If aForward is true and:
   *                      if aPoint is end of a run, returns next run.
   *                      if aPoint is start of a run, returns the run.
   *                      if aPoint is before the first run, returns the first
   *                      run.
   *                      If aPoint is after the last run, returns nullptr.
   *                    If aForward is false and:
   *                      if aPoint is end of a run, returns the run.
   *                      if aPoint is start of a run, returns its next run.
   *                      if aPoint is before the first run, returns nullptr.
   *                      if aPoint is after the last run, returns the last run.
   */
  template <typename PT, typename CT>
  WSFragment* FindNearestRun(const EditorDOMPointBase<PT, CT>& aPoint,
                             bool aForward) const;

  /**
   * GetNextCharPoint() returns next character's point of aPoint.  If there is
   * no character after aPoint, mTextNode is set to nullptr.
   */
  template <typename PT, typename CT>
  WSPoint GetNextCharPoint(const EditorDOMPointBase<PT, CT>& aPoint) const;
  WSPoint GetNextCharPoint(const WSPoint& aPoint) const;

  /**
   * GetNextCharPointInternal() and GetPreviousCharPointInternal() are
   * helper methods of GetNextCharPoint(const EditorRawDOMPoint&) and
   * GetPreviousCharPoint(const EditorRawDOMPoint&).  When the container
   * isn't in mNodeArray, they call one of these methods.  Then, these
   * methods look for nearest text node in mNodeArray from aPoint.
   * Then, will call GetNextCharPoint(const WSPoint&) or
   * GetPreviousCharPoint(const WSPoint&) and returns its result.
   */
  template <typename PT, typename CT>
  WSPoint GetNextCharPointInternal(
      const EditorDOMPointBase<PT, CT>& aPoint) const;
  template <typename PT, typename CT>
  WSPoint GetPreviousCharPointInternal(
      const EditorDOMPointBase<PT, CT>& aPoint) const;

  nsresult GetWSNodes();

  /**
   * Return the node which we will handle white-space under. This is the
   * closest block within the DOM subtree we're editing, or if none is
   * found, the (inline) root of the editable subtree.
   */
  nsINode* GetWSBoundingParent() const;

  static bool IsBlockNode(nsINode* aNode);

  nsIContent* GetPreviousWSNodeInner(nsINode* aStartNode,
                                     nsINode* aBlockParent) const;
  nsIContent* GetPreviousWSNode(const EditorDOMPoint& aPoint,
                                nsINode* aBlockParent) const;
  nsIContent* GetNextWSNodeInner(nsINode* aStartNode,
                                 nsINode* aBlockParent) const;
  nsIContent* GetNextWSNode(const EditorDOMPoint& aPoint,
                            nsINode* aBlockParent) const;

  /**
   * GetPreviousCharPoint() returns previous character's point of of aPoint.
   * If there is no character before aPoint, mTextNode is set to nullptr.
   */
  template <typename PT, typename CT>
  WSPoint GetPreviousCharPoint(const EditorDOMPointBase<PT, CT>& aPoint) const;
  WSPoint GetPreviousCharPoint(const WSPoint& aPoint) const;

  char16_t GetCharAt(dom::Text* aTextNode, int32_t aOffset) const;

  void GetRuns();
  void ClearRuns();
  void MakeSingleWSRun(WSType aType);

  // The list of nodes containing ws in this run.
  nsTArray<RefPtr<dom::Text>> mNodeArray;

  // The node passed to our constructor.
  EditorDOMPoint mScanStartPoint;
  EditorDOMPoint mScanEndPoint;
  // Together, the above represent the point at which we are building up ws
  // info.

  // The editing host when the instance is created.
  RefPtr<Element> mEditingHost;

  // true if we are in preformatted whitespace context.
  bool mPRE;

  // Node/offset where ws starts and ends.
  nsCOMPtr<nsINode> mStartNode;
  int32_t mStartOffset;
  // Reason why ws starts (eText, eOtherBlock, etc.).
  WSType mStartReason;
  // The node that implicated by start reason.
  nsCOMPtr<nsINode> mStartReasonNode;

  // Node/offset where ws ends.
  nsCOMPtr<nsINode> mEndNode;
  int32_t mEndOffset;
  // Reason why ws ends (eText, eOtherBlock, etc.).
  WSType mEndReason;
  // The node that implicated by end reason.
  nsCOMPtr<nsINode> mEndReasonNode;

  // Location of first nbsp in ws run, if any.
  RefPtr<dom::Text> mFirstNBSPNode;
  int32_t mFirstNBSPOffset;

  // Location of last nbsp in ws run, if any.
  RefPtr<dom::Text> mLastNBSPNode;
  int32_t mLastNBSPOffset;

  // The first WSFragment in the run.
  WSFragment* mStartRun;

  // The last WSFragment in the run, may be same as first.
  WSFragment* mEndRun;

  // Non-owning.
  const HTMLEditor* mHTMLEditor;
};

class MOZ_STACK_CLASS WSRunObject final : public WSRunScanner {
 protected:
  typedef EditorBase::AutoTransactionsConserveSelection
      AutoTransactionsConserveSelection;

 public:
  enum BlockBoundary { kBeforeBlock, kBlockStart, kBlockEnd, kAfterBlock };

  enum { eBefore = 1 };
  enum { eAfter = 1 << 1 };
  enum { eBoth = eBefore | eAfter };

  /**
   * The constructors take 2 DOM points.  They represent a range in an editing
   * host.  aScanEndPoint (aScanEndNode and aScanEndOffset) must be later
   * point from aScanStartPoint (aScanStartNode and aScanStartOffset).
   * The end point is currently used only with InsertText().  Therefore,
   * currently, this assumes that the range does not cross block boundary.  If
   * you use aScanEndPoint newly, please test enough.
   * TODO: PrepareToJoinBlocks(), PrepareToDeleteRange() and
   *       PrepareToDeleteNode() should be redesigned with aScanEndPoint.
   */
  template <typename PT, typename CT>
  WSRunObject(HTMLEditor* aHTMLEditor,
              const EditorDOMPointBase<PT, CT>& aScanStartPoint,
              const EditorDOMPointBase<PT, CT>& aScanEndPoint);
  template <typename PT, typename CT>
  WSRunObject(HTMLEditor* aHTMLEditor,
              const EditorDOMPointBase<PT, CT>& aScanStartPoint)
      : WSRunObject(aHTMLEditor, aScanStartPoint, aScanStartPoint) {}
  WSRunObject(HTMLEditor* aHTMLEditor, nsINode* aScanStartNode,
              int32_t aScanStartOffset, nsINode* aScanEndNode,
              int32_t aScanEndOffset)
      : WSRunObject(aHTMLEditor,
                    EditorRawDOMPoint(aScanStartNode, aScanStartOffset),
                    EditorRawDOMPoint(aScanEndNode, aScanEndOffset)) {}
  WSRunObject(HTMLEditor* aHTMLEditor, nsINode* aScanStartNode,
              int32_t aScanStartOffset)
      : WSRunObject(aHTMLEditor,
                    EditorRawDOMPoint(aScanStartNode, aScanStartOffset),
                    EditorRawDOMPoint(aScanStartNode, aScanStartOffset)) {}

  // ScrubBlockBoundary removes any non-visible whitespace at the specified
  // location relative to a block node.
  MOZ_CAN_RUN_SCRIPT
  static nsresult ScrubBlockBoundary(HTMLEditor* aHTMLEditor,
                                     BlockBoundary aBoundary, nsINode* aBlock,
                                     int32_t aOffset = -1);

  // PrepareToJoinBlocks fixes up ws at the end of aLeftBlock and the
  // beginning of aRightBlock in preperation for them to be joined.  Example
  // of fixup: trailingws in aLeftBlock needs to be removed.
  MOZ_CAN_RUN_SCRIPT
  static nsresult PrepareToJoinBlocks(HTMLEditor* aHTMLEditor,
                                      dom::Element* aLeftBlock,
                                      dom::Element* aRightBlock);

  // PrepareToDeleteRange fixes up ws before {aStartNode,aStartOffset}
  // and after {aEndNode,aEndOffset} in preperation for content
  // in that range to be deleted.  Note that the nodes and offsets
  // are adjusted in response to any dom changes we make while
  // adjusting ws.
  // example of fixup: trailingws before {aStartNode,aStartOffset}
  //                   needs to be removed.
  MOZ_CAN_RUN_SCRIPT
  static nsresult PrepareToDeleteRange(HTMLEditor* aHTMLEditor,
                                       nsCOMPtr<nsINode>* aStartNode,
                                       int32_t* aStartOffset,
                                       nsCOMPtr<nsINode>* aEndNode,
                                       int32_t* aEndOffset);

  // PrepareToDeleteNode fixes up ws before and after aContent in preparation
  // for aContent to be deleted.  Example of fixup: trailingws before
  // aContent needs to be removed.
  MOZ_CAN_RUN_SCRIPT
  static nsresult PrepareToDeleteNode(HTMLEditor* aHTMLEditor,
                                      nsIContent* aContent);

  // PrepareToSplitAcrossBlocks fixes up ws before and after
  // {aSplitNode,aSplitOffset} in preparation for a block parent to be split.
  // Note that the aSplitNode and aSplitOffset are adjusted in response to
  // any DOM changes we make while adjusting ws.  Example of fixup: normalws
  // before {aSplitNode,aSplitOffset} needs to end with nbsp.
  MOZ_CAN_RUN_SCRIPT
  static nsresult PrepareToSplitAcrossBlocks(HTMLEditor* aHTMLEditor,
                                             nsCOMPtr<nsINode>* aSplitNode,
                                             int32_t* aSplitOffset);

  /**
   * InsertBreak() inserts a <br> node at (before) aPointToInsert and delete
   * unnecessary whitespaces around there and/or replaces whitespaces with
   * non-breaking spaces.  Note that if the point is in a text node, the
   * text node will be split and insert new <br> node between the left node
   * and the right node.
   *
   * @param aSelection      The selection for the editor.
   * @param aPointToInsert  The point to insert new <br> element.  Note that
   *                        it'll be inserted before this point.  I.e., the
   *                        point will be the point of new <br>.
   * @param aSelect         If eNone, this won't change selection.
   *                        If eNext, selection will be collapsed after the
   *                        <br> element.
   *                        If ePrevious, selection will be collapsed at the
   *                        <br> element.
   * @return                The new <br> node.  If failed to create new <br>
   *                        node, returns nullptr.
   */
  MOZ_CAN_RUN_SCRIPT already_AddRefed<dom::Element> InsertBreak(
      Selection& aSelection, const EditorDOMPoint& aPointToInsert,
      nsIEditor::EDirection aSelect);

  /**
   * InsertText() inserts aStringToInsert to mScanStartPoint and makes any
   * needed adjustments to white spaces around both mScanStartPoint and
   * mScanEndPoint. E.g., trailing white spaces before mScanStartPoint needs to
   * be removed.  This calls EditorBase::InsertTextWithTransaction() after
   * adjusting white spaces.  So, please refer the method's explanation to know
   * what this method exactly does.
   *
   * @param aDocument       The document of this editor.
   * @param aStringToInsert The string to insert.
   * @param aPointAfterInsertedString
   *                        The point after inserted aStringToInsert.
   *                        So, when this method actually inserts string,
   *                        this is set to a point in the text node.
   *                        Otherwise, this may be set to mScanStartPoint.
   * @return                When this succeeds to insert the string or
   *                        does nothing during composition, returns NS_OK.
   *                        Otherwise, an error code.
   */
  MOZ_CAN_RUN_SCRIPT nsresult
  InsertText(dom::Document& aDocument, const nsAString& aStringToInsert,
             EditorRawDOMPoint* aPointAfterInsertedString = nullptr);

  // DeleteWSBackward deletes a single visible piece of ws before the ws
  // point (the point to create the wsRunObject, passed to its constructor).
  // It makes any needed conversion to adjacent ws to retain its
  // significance.
  MOZ_CAN_RUN_SCRIPT nsresult DeleteWSBackward();

  // DeleteWSForward deletes a single visible piece of ws after the ws point
  // (the point to create the wsRunObject, passed to its constructor).  It
  // makes any needed conversion to adjacent ws to retain its significance.
  MOZ_CAN_RUN_SCRIPT nsresult DeleteWSForward();

  // AdjustWhitespace examines the ws object for nbsp's that can
  // be safely converted to regular ascii space and converts them.
  MOZ_CAN_RUN_SCRIPT nsresult AdjustWhitespace();

  Element* GetEditingHost() const { return mEditingHost; }

 protected:
  using WSPoint = WSRunScanner::WSPoint;

  MOZ_CAN_RUN_SCRIPT nsresult PrepareToDeleteRangePriv(WSRunObject* aEndObject);
  MOZ_CAN_RUN_SCRIPT nsresult PrepareToSplitAcrossBlocksPriv();

  /**
   * DeleteRange() removes the range between aStartPoint and aEndPoint.
   * When aStartPoint and aEndPoint are same point, does nothing.
   * When aStartPoint and aEndPoint are in same text node, removes characters
   * between them.
   * When aStartPoint is in a text node, removes the text data after the point.
   * When aEndPoint is in a text node, removes the text data before the point.
   * Removes any nodes between them.
   */
  MOZ_CAN_RUN_SCRIPT nsresult DeleteRange(const EditorDOMPoint& aStartPoint,
                                          const EditorDOMPoint& aEndPoint);

  /**
   * InsertNBSPAndRemoveFollowingASCIIWhitespaces() inserts an NBSP first.
   * Then, if following characters are ASCII whitespaces, will remove them.
   */
  MOZ_CAN_RUN_SCRIPT
  nsresult InsertNBSPAndRemoveFollowingASCIIWhitespaces(WSPoint aPoint);

  /**
   * GetASCIIWhitespacesBounds() retrieves whitespaces before and/or after the
   * point specified by aNode and aOffset.
   *
   * @param aDir            Specify eBefore if you want to scan text backward.
   *                        Specify eAfter if you want to scan text forward.
   *                        Specify eBoth if you want to scan text to both
   *                        direction.
   * @param aPoint          The point to start to scan whitespaces from.
   * @param outStartNode    [out] The container of first ASCII whitespace.
   *                              If there is no whitespaces, returns nullptr.
   * @param outStartOffset  [out] The offset of first ASCII whitespace in
   *                              outStartNode.
   * @param outEndNode      [out] The container of last ASCII whitespace.
   *                              If there is no whitespaces, returns nullptr.
   * @param outEndOffset    [out] The offset of last ASCII whitespace in
   *                              outEndNode.
   */
  template <typename PT, typename CT>
  void GetASCIIWhitespacesBounds(int16_t aDir,
                                 const EditorDOMPointBase<PT, CT>& aPoint,
                                 dom::Text** outStartNode,
                                 int32_t* outStartOffset,
                                 dom::Text** outEndNode,
                                 int32_t* outEndOffset) const;

  MOZ_CAN_RUN_SCRIPT nsresult CheckTrailingNBSPOfRun(WSFragment* aRun);

  /**
   * ReplacePreviousNBSPIfUnncessary() replaces previous character of aPoint
   * if it's a NBSP and it's unnecessary.
   *
   * @param aRun        Current text run.  aPoint must be in this run.
   * @param aPoint      Current insertion point.  Its previous character is
   *                    unnecessary NBSP will be checked.
   */
  MOZ_CAN_RUN_SCRIPT nsresult ReplacePreviousNBSPIfUnncessary(
      WSFragment* aRun, const EditorDOMPoint& aPoint);

  MOZ_CAN_RUN_SCRIPT
  nsresult CheckLeadingNBSP(WSFragment* aRun, nsINode* aNode, int32_t aOffset);

  MOZ_CAN_RUN_SCRIPT nsresult Scrub();

  EditorDOMPoint StartPoint() const {
    return EditorDOMPoint(mStartNode, mStartOffset);
  }
  EditorDOMPoint EndPoint() const {
    return EditorDOMPoint(mEndNode, mEndOffset);
  }

  // Non-owning.
  HTMLEditor* mHTMLEditor;

  // Opening this class up for pillaging.
  friend class HTMLEditRules;
  // Opening this class up for more pillaging.
  friend class HTMLEditor;
};

}  // namespace mozilla

#endif  // #ifndef WSRunObject_h
