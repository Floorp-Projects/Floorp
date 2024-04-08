/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EditAction_h
#define mozilla_EditAction_h

#include "mozilla/EventForwards.h"
#include "mozilla/StaticPrefs_dom.h"

namespace mozilla {

/**
 * EditAction indicates which operation or command causes running the methods
 * of editors.
 */
enum class EditAction {
  // eNone indicates no edit action is being handled.
  eNone,

  // eNotEditing indicates that something is retrieved, doing something at
  // destroying or focus move etc, i.e., not edit action is being handled but
  // editor is doing something.
  eNotEditing,

  // eInitializing indicates that the editor instance is being initialized.
  eInitializing,

  // eInsertText indicates to insert some characters.
  eInsertText,

  // eInsertParagraphSeparator indicates to insert a paragraph separator such
  // as <p>, <div>.
  eInsertParagraphSeparator,

  // eInsertLineBreak indicates to insert \n into TextEditor or a <br> element
  // in HTMLEditor.
  eInsertLineBreak,

  // eDeleteSelection indicates to delete selected content or content around
  // caret if selection is collapsed.
  eDeleteSelection,

  // eDeleteBackward indicates to remove previous character element of caret.
  // This may be set even when Selection is not collapsed.
  eDeleteBackward,

  // eDeleteForward indicates to remove next character or element of caret.
  // This may be set even when Selection is not collapsed.
  eDeleteForward,

  // eDeleteWordBackward indicates to remove previous word.  If caret is in
  // a word, remove characters between word start and caret.
  // This may be set even when Selection is not collapsed.
  eDeleteWordBackward,

  // eDeleteWordForward indicates to remove next word.  If caret is in a
  // word, remove characters between caret and word end.
  // This may be set even when Selection is not collapsed.
  eDeleteWordForward,

  // eDeleteToBeginningOfSoftLine indicates to remove characters between
  // caret and previous visual line break.
  // This may be set even when Selection is not collapsed.
  eDeleteToBeginningOfSoftLine,

  // eDeleteToEndOfSoftLine indicates to remove characters between caret and
  // next visual line break.
  // This may be set even when Selection is not collapsed.
  eDeleteToEndOfSoftLine,

  // eDeleteByDrag indicates to remove selection by dragging the content
  // to different place.
  eDeleteByDrag,

  // eStartComposition indicates that user starts composition.
  eStartComposition,

  // eUpdateComposition indicates that user updates composition with
  // new non-empty composition string and IME selections.
  eUpdateComposition,

  // eCommitComposition indicates that user commits composition.
  eCommitComposition,

  // eCancelComposition indicates that user cancels composition.
  eCancelComposition,

  // eDeleteByComposition indicates that user starts composition with
  // empty string and there was selected content.
  eDeleteByComposition,

  // eUndo/eRedo indicate to undo/redo a transaction.
  eUndo,
  eRedo,

  // eSetTextDirection indicates that setting text direction (LTR or RTL).
  eSetTextDirection,

  // eCut indicates to delete selected content and copy it to the clipboard.
  eCut,

  // eCopy indicates to copy selected content to the clipboard.
  eCopy,

  // ePaste indicates to paste clipboard data.
  ePaste,

  // ePasteAsQuotation indicates to paste clipboard data as quotation.
  ePasteAsQuotation,

  // eDrop indicates that user drops dragging item into the editor.
  eDrop,

  // eIndent indicates that to indent selected line(s).
  eIndent,

  // eOutdent indicates that to outdent selected line(s).
  eOutdent,

  // eReplaceText indicates to replace a part of range in editor with
  // specific text.  For example, user select a correct word in suggestions
  // of spellchecker or a suggestion in list of autocomplete.
  eReplaceText,

  // eInsertTableRowElement indicates to insert table rows (i.e., <tr>
  // elements).
  eInsertTableRowElement,

  // eRemoveTableRowElement indicates to remove table row elements.
  eRemoveTableRowElement,

  // eInsertTableColumn indicates to insert cell elements to each row.
  eInsertTableColumn,

  // eRemoveTableColumn indicates to remove cell elements from each row.
  eRemoveTableColumn,

  // eResizingElement indicates that user starts to resize or keep resizing
  // with dragging a resizer which is provided by Gecko.
  eResizingElement,

  // eResizeElement indicates that user resizes an element size with finishing
  // dragging a resizer which is provided by Gecko.
  eResizeElement,

  // eMovingElement indicates that user starts to move or keep moving an
  // element with grabber which is provided by Gecko.
  eMovingElement,

  // eMoveElement indicates that user finishes moving an element with grabber
  // which is provided by Gecko.
  eMoveElement,

  // The following edit actions are not user's operation.  They are caused
  // by if UI does something or web apps does something with JS.

  // eUnknown indicates some special edit actions, e.g., batching of some
  // nsI*Editor method calls.  This shouldn't be set while handling a user
  // operation.
  eUnknown,

  // eSetAttribute indicates to set attribute value of an element node.
  eSetAttribute,

  // eRemoveAttribute indicates to remove attribute from an element node.
  eRemoveAttribute,

  // eInsertNode indicates to insert a node into the tree.
  eInsertNode,

  // eDeleteNode indicates to remove a node form the tree.
  eRemoveNode,

  // eInsertBlockElement indicates to insert a block-level element like <div>,
  // <pre>, <li>, <dd> etc.
  eInsertBlockElement,

  // eInsertHorizontalRuleElement indicates to insert a <hr> element.
  eInsertHorizontalRuleElement,

  // eInsertLinkElement indicates to insert an anchor element which has
  // href attribute.
  eInsertLinkElement,

  // eInsertUnorderedListElement and eInsertOrderedListElement indicate to
  // insert <ul> or <ol> element.
  eInsertUnorderedListElement,
  eInsertOrderedListElement,

  // eRemoveUnorderedListElement and eRemoveOrderedListElement indicate to
  // remove <ul> or <ol> element.
  eRemoveUnorderedListElement,
  eRemoveOrderedListElement,

  // eRemoveListElement indicates to remove <ul>, <ol> and/or <dl> element.
  eRemoveListElement,

  // eInsertBlockquoteElement indicates to insert a <blockquote> element.
  eInsertBlockquoteElement,

  // eNormalizeTable indicates to normalize table.  E.g., if a row does
  // not have enough number of cells, inserts empty cells.
  eNormalizeTable,

  // eRemoveTableElement indicates to remove <table> element.
  eRemoveTableElement,

  // eRemoveTableCellContents indicates to remove any children in a table
  // cell element.
  eDeleteTableCellContents,

  // eInsertTableCellElement indicates to insert table cell elements (i.e.,
  // <td> or <th>).
  eInsertTableCellElement,

  // eRemoveTableCellEelement indicates to remove table cell elements.
  eRemoveTableCellElement,

  // eJoinTableCellElements indicates to join table cell elements.
  eJoinTableCellElements,

  // eSplitTableCellElement indicates to split table cell elements.
  eSplitTableCellElement,

  // eSetTableCellElementType indicates to set table cell element type to
  // <td> or <th>.
  eSetTableCellElementType,

  // Those edit actions are mapped to the methods in nsITableEditor which
  // access table layout information.
  eSelectTableCell,
  eSelectTableRow,
  eSelectTableColumn,
  eSelectTable,
  eSelectAllTableCells,
  eGetCellIndexes,
  eGetTableSize,
  eGetCellAt,
  eGetCellDataAt,
  eGetFirstRow,
  eGetSelectedOrParentTableElement,
  eGetSelectedCellsType,
  eGetFirstSelectedCellInTable,
  eGetSelectedCells,

  // eSetInlineStyleProperty indicates to set CSS another inline style property
  // which is not defined below.
  eSetInlineStyleProperty,

  // eRemoveInlineStyleProperty indicates to remove a CSS text property which
  // is not defined below.
  eRemoveInlineStyleProperty,

  // <b> or font-weight.
  eSetFontWeightProperty,
  eRemoveFontWeightProperty,

  // <i> or text-style: italic/oblique.
  eSetTextStyleProperty,
  eRemoveTextStyleProperty,

  // <u> or text-decoration: underline.
  eSetTextDecorationPropertyUnderline,
  eRemoveTextDecorationPropertyUnderline,

  // <strike> or text-decoration: line-through.
  eSetTextDecorationPropertyLineThrough,
  eRemoveTextDecorationPropertyLineThrough,

  // <sup> or text-align: super.
  eSetVerticalAlignPropertySuper,
  eRemoveVerticalAlignPropertySuper,

  // <sub> or text-align: sub.
  eSetVerticalAlignPropertySub,
  eRemoveVerticalAlignPropertySub,

  // <font face="foo"> or font-family.
  eSetFontFamilyProperty,
  eRemoveFontFamilyProperty,

  // <font color="foo"> or color.
  eSetColorProperty,
  eRemoveColorProperty,

  // <span style="background-color: foo">
  eSetBackgroundColorPropertyInline,
  eRemoveBackgroundColorPropertyInline,

  // eRemoveAllInlineStyleProperties indicates to remove all CSS inline
  // style properties.
  eRemoveAllInlineStyleProperties,

  // eIncrementFontSize indicates to increment font-size.
  eIncrementFontSize,

  // eDecrementFontSize indicates to decrement font-size.
  eDecrementFontSize,

  // eSetAlignment indicates to set alignment of selected content but different
  // from the following.
  eSetAlignment,

  // eAlign* and eJustify indicates to align contents in block with left
  // edge, right edge, center or justify the text.
  eAlignLeft,
  eAlignRight,
  eAlignCenter,
  eJustify,

  // eSetBackgroundColor indicates to set background color.
  eSetBackgroundColor,

  // eSetPositionToAbsoluteOrStatic indicates to set position property value
  // to "absolute" or "static".
  eSetPositionToAbsoluteOrStatic,

  // eIncreaseOrDecreaseZIndex indicates to change z-index of an element.
  eIncreaseOrDecreaseZIndex,

  // eEnableOrDisableCSS indicates to enable or disable CSS mode of HTMLEditor.
  eEnableOrDisableCSS,

  // eEnableOrDisableAbsolutePositionEditor indicates to enable or disable
  // absolute positioned element editing UI.
  eEnableOrDisableAbsolutePositionEditor,

  // eEnableOrDisableResizer indicates to enable or disable resizers of
  // <img>, <table> and absolutely positioned element.
  eEnableOrDisableResizer,

  // eEnableOrDisableInlineTableEditingUI indicates to enable or disable
  // inline table editing UI.
  eEnableOrDisableInlineTableEditingUI,

  // eSetCharacterSet indicates to set character-set of the document.
  eSetCharacterSet,

  // eSetWrapWidth indicates to set wrap width.
  eSetWrapWidth,

  // eRewrap indicates to rewrap for current wrap width.
  eRewrap,

  // eSetText indicates to set new text of TextEditor, e.g., setting
  // HTMLInputElement.value.
  eSetText,

  // eSetHTML indicates to set body of HTMLEditor.
  eSetHTML,

  // eInsertHTML indicates to insert HTML source code.
  eInsertHTML,

  // eHidePassword indicates that editor hides password with mask characters.
  eHidePassword,

  // eCreatePaddingBRElementForEmptyEditor indicates that editor wants to
  // create a padding <br> element for empty editor after it modifies its
  // content.
  eCreatePaddingBRElementForEmptyEditor,
};

// This is int32_t instead of int16_t because nsIInlineSpellChecker.idl's
// spellCheckAfterEditorChange is defined to take it as a long.
// TODO: Make each name eFoo and investigate whether the numeric values
//       still have some meaning.
enum class EditSubAction : int32_t {
  // eNone indicates not edit sub-action is being handled.  This is useful
  // of initial value of member variables.
  eNone,

  // eUndo and eRedo indicate entire actions of undo/redo operation.
  eUndo,
  eRedo,

  // eInsertNode indicates to insert a new node into the DOM tree.
  eInsertNode,

  // eCreateNode indicates to create a new node and insert it into the DOM tree.
  eCreateNode,

  // eDeleteNode indicates to remove a node from the DOM tree.
  eDeleteNode,

  // eMoveNode indicates to move a node connected in the DOM tree to different
  // place.
  eMoveNode,

  // eSplitNode indicates to split a node to 2 nodes.
  eSplitNode,

  // eJoinNodes indicates to join 2 nodes.
  eJoinNodes,

  // eDeleteText indicates to delete some characters form a text node.
  eDeleteText,

  // eInsertText indicates to insert some characters.
  eInsertText,

  // eInsertTextComingFromIME indicates to insert or update composition string
  // with new text which is new composition string or commit string.
  eInsertTextComingFromIME,

  // eDeleteSelectedContent indicates to remove selected content.
  eDeleteSelectedContent,

  // eSetTextProperty indicates to set a style from text.
  eSetTextProperty,

  // eRemoveTextProperty indicates to remove a style from text.
  eRemoveTextProperty,

  // eRemoveAllTextProperties indicate to remove all styles from text.
  eRemoveAllTextProperties,

  // eComputeTextToOutput indicates to compute the editor value as plain text
  // or something requested format.
  eComputeTextToOutput,

  // eSetText indicates to set editor value to new value.
  eSetText,

  // eInsertLineBreak indicates to insert a line break, <br> or \n to break
  // current line.
  eInsertLineBreak,

  // eInsertParagraphSeparator indicates to insert paragraph separator, <br> or
  // \n at least to break current line in HTMLEditor.
  eInsertParagraphSeparator,

  // eCreateOrChangeList indicates to create new list or change existing list
  // type.
  eCreateOrChangeList,

  // eIndent and eOutdent indicates to indent or outdent the target with
  // using <blockquote>, <ul>, <ol> or just margin of start edge.
  eIndent,
  eOutdent,

  // eSetOrClearAlignment aligns content or clears alignment with align
  // attribute or text-align.
  eSetOrClearAlignment,

  // eCreateOrRemoveBlock creates new block or removes existing block and
  // move its descendants to where the block was.
  eCreateOrRemoveBlock,

  // eFormatBlockForHTMLCommand wraps selected lines into format block, replaces
  // format blocks around selection with new format block or deletes format
  // blocks around selection.
  eFormatBlockForHTMLCommand,

  // eMergeBlockContents is not an actual sub-action, but this is used by
  // HTMLEditor::MoveBlock() to request special handling in
  // HTMLEditor::MaybeSplitElementsAtEveryBRElement().
  eMergeBlockContents,

  // eRemoveList removes specific type of list but keep its content.
  eRemoveList,

  // eCreateOrChangeDefinitionListItem indicates to format current hard line(s)
  // `<dd>` or `<dt>`.  This may cause creating or changing existing list
  // element to new `<dl>` element.
  eCreateOrChangeDefinitionListItem,

  // eInsertElement indicates to insert an element.
  eInsertElement,

  // eInsertQuotation indicates to insert an element and make it "quoted text".
  eInsertQuotation,

  // eInsertQuotedText indicates to insert text which has already been quoted.
  eInsertQuotedText,

  // ePasteHTMLContent indicates to paste HTML content in clipboard.
  ePasteHTMLContent,

  // eInsertHTMLSource indicates to create a document fragment from given HTML
  // source and insert into the DOM tree.  So, this is similar to innerHTML.
  eInsertHTMLSource,

  // eReplaceHeadWithHTMLSource indicates to create a document fragment from
  // given HTML source and replace content of <head> with it.
  eReplaceHeadWithHTMLSource,

  // eSetPositionToAbsolute and eSetPositionToStatic indicates to set position
  // property to absolute or static.
  eSetPositionToAbsolute,
  eSetPositionToStatic,

  // eDecreaseZIndex and eIncreaseZIndex indicate to decrease and increase
  // z-index value.
  eDecreaseZIndex,
  eIncreaseZIndex,

  // eCreatePaddingBRElementForEmptyEditor indicates to create a padding <br>
  // element for empty editor.
  eCreatePaddingBRElementForEmptyEditor,
};

// You can use this macro as:
//   case NS_EDIT_ACTION_CASES_ACCESSING_TABLE_DATA_WITHOUT_EDITING:
// clang-format off
#define NS_EDIT_ACTION_CASES_ACCESSING_TABLE_DATA_WITHOUT_EDITING \
       mozilla::EditAction::eSelectTableCell:                     \
  case mozilla::EditAction::eSelectTableRow:                      \
  case mozilla::EditAction::eSelectTableColumn:                   \
  case mozilla::EditAction::eSelectTable:                         \
  case mozilla::EditAction::eSelectAllTableCells:                 \
  case mozilla::EditAction::eGetCellIndexes:                      \
  case mozilla::EditAction::eGetTableSize:                        \
  case mozilla::EditAction::eGetCellAt:                           \
  case mozilla::EditAction::eGetCellDataAt:                       \
  case mozilla::EditAction::eGetFirstRow:                         \
  case mozilla::EditAction::eGetSelectedOrParentTableElement:     \
  case mozilla::EditAction::eGetSelectedCellsType:                \
  case mozilla::EditAction::eGetFirstSelectedCellInTable:         \
  case mozilla::EditAction::eGetSelectedCells
// clang-format on

inline EditorInputType ToInputType(EditAction aEditAction) {
  switch (aEditAction) {
    case EditAction::eInsertText:
      return EditorInputType::eInsertText;
    case EditAction::eReplaceText:
      return EditorInputType::eInsertReplacementText;
    case EditAction::eInsertLineBreak:
      return EditorInputType::eInsertLineBreak;
    case EditAction::eInsertParagraphSeparator:
      return EditorInputType::eInsertParagraph;
    case EditAction::eInsertOrderedListElement:
    case EditAction::eRemoveOrderedListElement:
      return EditorInputType::eInsertOrderedList;
    case EditAction::eInsertUnorderedListElement:
    case EditAction::eRemoveUnorderedListElement:
      return EditorInputType::eInsertUnorderedList;
    case EditAction::eInsertHorizontalRuleElement:
      return EditorInputType::eInsertHorizontalRule;
    case EditAction::eDrop:
      return EditorInputType::eInsertFromDrop;
    case EditAction::ePaste:
      return EditorInputType::eInsertFromPaste;
    case EditAction::ePasteAsQuotation:
      return EditorInputType::eInsertFromPasteAsQuotation;
    case EditAction::eUpdateComposition:
      return EditorInputType::eInsertCompositionText;
    case EditAction::eCommitComposition:
      if (StaticPrefs::dom_input_events_conform_to_level_1()) {
        return EditorInputType::eInsertCompositionText;
      }
      return EditorInputType::eInsertFromComposition;
    case EditAction::eCancelComposition:
      if (StaticPrefs::dom_input_events_conform_to_level_1()) {
        return EditorInputType::eInsertCompositionText;
      }
      return EditorInputType::eDeleteCompositionText;
    case EditAction::eDeleteByComposition:
      if (StaticPrefs::dom_input_events_conform_to_level_1()) {
        // XXX Or EditorInputType::eDeleteContent?  I don't know which IME may
        //     causes this situation.
        return EditorInputType::eInsertCompositionText;
      }
      return EditorInputType::eDeleteByComposition;
    case EditAction::eInsertLinkElement:
      return EditorInputType::eInsertLink;
    case EditAction::eDeleteWordBackward:
      return EditorInputType::eDeleteWordBackward;
    case EditAction::eDeleteWordForward:
      return EditorInputType::eDeleteWordForward;
    case EditAction::eDeleteToBeginningOfSoftLine:
      return EditorInputType::eDeleteSoftLineBackward;
    case EditAction::eDeleteToEndOfSoftLine:
      return EditorInputType::eDeleteSoftLineForward;
    case EditAction::eDeleteByDrag:
      return EditorInputType::eDeleteByDrag;
    case EditAction::eCut:
      return EditorInputType::eDeleteByCut;
    case EditAction::eDeleteSelection:
    case EditAction::eRemoveTableRowElement:
    case EditAction::eRemoveTableColumn:
    case EditAction::eRemoveTableElement:
    case EditAction::eDeleteTableCellContents:
    case EditAction::eRemoveTableCellElement:
      return EditorInputType::eDeleteContent;
    case EditAction::eDeleteBackward:
      return EditorInputType::eDeleteContentBackward;
    case EditAction::eDeleteForward:
      return EditorInputType::eDeleteContentForward;
    case EditAction::eUndo:
      return EditorInputType::eHistoryUndo;
    case EditAction::eRedo:
      return EditorInputType::eHistoryRedo;
    case EditAction::eSetFontWeightProperty:
    case EditAction::eRemoveFontWeightProperty:
      return EditorInputType::eFormatBold;
    case EditAction::eSetTextStyleProperty:
    case EditAction::eRemoveTextStyleProperty:
      return EditorInputType::eFormatItalic;
    case EditAction::eSetTextDecorationPropertyUnderline:
    case EditAction::eRemoveTextDecorationPropertyUnderline:
      return EditorInputType::eFormatUnderline;
    case EditAction::eSetTextDecorationPropertyLineThrough:
    case EditAction::eRemoveTextDecorationPropertyLineThrough:
      return EditorInputType::eFormatStrikeThrough;
    case EditAction::eSetVerticalAlignPropertySuper:
    case EditAction::eRemoveVerticalAlignPropertySuper:
      return EditorInputType::eFormatSuperscript;
    case EditAction::eSetVerticalAlignPropertySub:
    case EditAction::eRemoveVerticalAlignPropertySub:
      return EditorInputType::eFormatSubscript;
    case EditAction::eJustify:
      return EditorInputType::eFormatJustifyFull;
    case EditAction::eAlignCenter:
      return EditorInputType::eFormatJustifyCenter;
    case EditAction::eAlignRight:
      return EditorInputType::eFormatJustifyRight;
    case EditAction::eAlignLeft:
      return EditorInputType::eFormatJustifyLeft;
    case EditAction::eIndent:
      return EditorInputType::eFormatIndent;
    case EditAction::eOutdent:
      return EditorInputType::eFormatOutdent;
    case EditAction::eRemoveAllInlineStyleProperties:
      return EditorInputType::eFormatRemove;
    case EditAction::eSetTextDirection:
      return EditorInputType::eFormatSetBlockTextDirection;
    case EditAction::eSetBackgroundColorPropertyInline:
    case EditAction::eRemoveBackgroundColorPropertyInline:
      return EditorInputType::eFormatBackColor;
    case EditAction::eSetColorProperty:
    case EditAction::eRemoveColorProperty:
      return EditorInputType::eFormatFontColor;
    case EditAction::eSetFontFamilyProperty:
    case EditAction::eRemoveFontFamilyProperty:
      return EditorInputType::eFormatFontName;
    default:
      return EditorInputType::eUnknown;
  }
}

inline bool MayEditActionDeleteAroundCollapsedSelection(
    const EditAction aEditAction) {
  switch (aEditAction) {
    case EditAction::eDeleteSelection:
    case EditAction::eDeleteBackward:
    case EditAction::eDeleteForward:
    case EditAction::eDeleteWordBackward:
    case EditAction::eDeleteWordForward:
    case EditAction::eDeleteToBeginningOfSoftLine:
    case EditAction::eDeleteToEndOfSoftLine:
      return true;
    default:
      return false;
  }
}

inline bool IsEditActionInOrderToEditSomething(const EditAction aEditAction) {
  switch (aEditAction) {
    case EditAction::eNotEditing:
    case NS_EDIT_ACTION_CASES_ACCESSING_TABLE_DATA_WITHOUT_EDITING:
      return false;
    default:
      return true;
  }
}

inline bool IsEditActionTableEditing(const EditAction aEditAction) {
  switch (aEditAction) {
    case EditAction::eInsertTableRowElement:
    case EditAction::eRemoveTableRowElement:
    case EditAction::eInsertTableColumn:
    case EditAction::eRemoveTableColumn:
    case EditAction::eRemoveTableElement:
    case EditAction::eRemoveTableCellElement:
    case EditAction::eDeleteTableCellContents:
    case EditAction::eInsertTableCellElement:
    case EditAction::eJoinTableCellElements:
    case EditAction::eSplitTableCellElement:
    case EditAction::eSetTableCellElementType:
      return true;
    default:
      return false;
  }
}

inline bool MayEditActionDeleteSelection(const EditAction aEditAction) {
  switch (aEditAction) {
    case EditAction::eNone:
    case EditAction::eNotEditing:
    case EditAction::eInitializing:
    case NS_EDIT_ACTION_CASES_ACCESSING_TABLE_DATA_WITHOUT_EDITING:
      return false;

    // EditActions modifying around selection.
    case EditAction::eInsertText:
    case EditAction::eInsertParagraphSeparator:
    case EditAction::eInsertLineBreak:
    case EditAction::eDeleteSelection:
    case EditAction::eDeleteBackward:
    case EditAction::eDeleteForward:
    case EditAction::eDeleteWordBackward:
    case EditAction::eDeleteWordForward:
    case EditAction::eDeleteToBeginningOfSoftLine:
    case EditAction::eDeleteToEndOfSoftLine:
    case EditAction::eDeleteByDrag:
      return true;

    case EditAction::eStartComposition:
      return false;

    case EditAction::eUpdateComposition:
    case EditAction::eCommitComposition:
    case EditAction::eCancelComposition:
    case EditAction::eDeleteByComposition:
      return true;

    case EditAction::eUndo:
    case EditAction::eRedo:
    case EditAction::eSetTextDirection:
      return false;

    case EditAction::eCut:
      return true;

    case EditAction::eCopy:
      return false;

    case EditAction::ePaste:
    case EditAction::ePasteAsQuotation:
      return true;

    case EditAction::eDrop:
      return false;  // Not deleting selection at drop.

    // EditActions changing format around selection.
    case EditAction::eIndent:
    case EditAction::eOutdent:
      return false;

    // EditActions inserting or deleting something at specified position.
    case EditAction::eInsertTableRowElement:
    case EditAction::eRemoveTableRowElement:
    case EditAction::eInsertTableColumn:
    case EditAction::eRemoveTableColumn:
    case EditAction::eResizingElement:
    case EditAction::eResizeElement:
    case EditAction::eMovingElement:
    case EditAction::eMoveElement:
    case EditAction::eUnknown:
    case EditAction::eSetAttribute:
    case EditAction::eRemoveAttribute:
    case EditAction::eRemoveNode:
    case EditAction::eInsertBlockElement:
      return false;

    // EditActions inserting someting around selection or replacing selection
    // with something.
    case EditAction::eReplaceText:
    case EditAction::eInsertNode:
    case EditAction::eInsertHorizontalRuleElement:
      return true;

    // EditActions changing format around selection or inserting or deleting
    // something at specific position.
    case EditAction::eInsertLinkElement:
    case EditAction::eInsertUnorderedListElement:
    case EditAction::eInsertOrderedListElement:
    case EditAction::eRemoveUnorderedListElement:
    case EditAction::eRemoveOrderedListElement:
    case EditAction::eRemoveListElement:
    case EditAction::eInsertBlockquoteElement:
    case EditAction::eNormalizeTable:
    case EditAction::eRemoveTableElement:
    case EditAction::eRemoveTableCellElement:
    case EditAction::eDeleteTableCellContents:
    case EditAction::eInsertTableCellElement:
    case EditAction::eJoinTableCellElements:
    case EditAction::eSplitTableCellElement:
    case EditAction::eSetTableCellElementType:
    case EditAction::eSetInlineStyleProperty:
    case EditAction::eRemoveInlineStyleProperty:
    case EditAction::eSetFontWeightProperty:
    case EditAction::eRemoveFontWeightProperty:
    case EditAction::eSetTextStyleProperty:
    case EditAction::eRemoveTextStyleProperty:
    case EditAction::eSetTextDecorationPropertyUnderline:
    case EditAction::eRemoveTextDecorationPropertyUnderline:
    case EditAction::eSetTextDecorationPropertyLineThrough:
    case EditAction::eRemoveTextDecorationPropertyLineThrough:
    case EditAction::eSetVerticalAlignPropertySuper:
    case EditAction::eRemoveVerticalAlignPropertySuper:
    case EditAction::eSetVerticalAlignPropertySub:
    case EditAction::eRemoveVerticalAlignPropertySub:
    case EditAction::eSetFontFamilyProperty:
    case EditAction::eRemoveFontFamilyProperty:
    case EditAction::eSetColorProperty:
    case EditAction::eRemoveColorProperty:
    case EditAction::eSetBackgroundColorPropertyInline:
    case EditAction::eRemoveBackgroundColorPropertyInline:
    case EditAction::eRemoveAllInlineStyleProperties:
    case EditAction::eIncrementFontSize:
    case EditAction::eDecrementFontSize:
    case EditAction::eSetAlignment:
    case EditAction::eAlignLeft:
    case EditAction::eAlignRight:
    case EditAction::eAlignCenter:
    case EditAction::eJustify:
    case EditAction::eSetBackgroundColor:
    case EditAction::eSetPositionToAbsoluteOrStatic:
    case EditAction::eIncreaseOrDecreaseZIndex:
      return false;

    // EditActions controlling editor feature or state.
    case EditAction::eEnableOrDisableCSS:
    case EditAction::eEnableOrDisableAbsolutePositionEditor:
    case EditAction::eEnableOrDisableResizer:
    case EditAction::eEnableOrDisableInlineTableEditingUI:
    case EditAction::eSetCharacterSet:
    case EditAction::eSetWrapWidth:
      return false;

    case EditAction::eRewrap:
    case EditAction::eSetText:
    case EditAction::eSetHTML:
    case EditAction::eInsertHTML:
      return true;

    case EditAction::eHidePassword:
    case EditAction::eCreatePaddingBRElementForEmptyEditor:
      return false;
  }
  return false;
}

inline bool MayEditActionRequireLayout(const EditAction aEditAction) {
  switch (aEditAction) {
    // Table editing require layout information for referring table cell data
    // such as row/column number and rowspan/colspan.
    case EditAction::eInsertTableRowElement:
    case EditAction::eRemoveTableRowElement:
    case EditAction::eInsertTableColumn:
    case EditAction::eRemoveTableColumn:
    case EditAction::eRemoveTableElement:
    case EditAction::eRemoveTableCellElement:
    case EditAction::eDeleteTableCellContents:
    case EditAction::eInsertTableCellElement:
    case EditAction::eJoinTableCellElements:
    case EditAction::eSplitTableCellElement:
    case EditAction::eSetTableCellElementType:
    case NS_EDIT_ACTION_CASES_ACCESSING_TABLE_DATA_WITHOUT_EDITING:
      return true;
    default:
      return false;
  }
}

}  // namespace mozilla

inline bool operator!(const mozilla::EditSubAction& aEditSubAction) {
  return aEditSubAction == mozilla::EditSubAction::eNone;
}

#endif  // #ifdef mozilla_EditAction_h
