/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/******

  This file contains the list of all layout nsIAtoms and their values
  
  It is designed to be used as inline input to nsLayoutAtoms.cpp *only*
  through the magic of C preprocessing.

  All entires must be enclosed in the macro LAYOUT_ATOM which will have cruel
  and unusual things done to it

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order

  The first argument to LAYOUT_ATOM is the C++ identifier of the atom
  The second argument is the string value of the atom

 ******/

// OUTPUT_CLASS=nsLayoutAtoms
// MACRO_NAME=LAYOUT_ATOM

  // Alphabetical list of media type atoms
LAYOUT_ATOM(all, "all")  // Media atoms must be lower case
LAYOUT_ATOM(aural, "aural")
LAYOUT_ATOM(braille, "braille")
LAYOUT_ATOM(embossed, "embossed")
LAYOUT_ATOM(handheld, "handheld")
LAYOUT_ATOM(print, "print")
LAYOUT_ATOM(projection, "projection")
LAYOUT_ATOM(screen, "screen")
LAYOUT_ATOM(tty, "tty")
LAYOUT_ATOM(tv, "tv")

  // Alphabetical list of standard name space prefixes
LAYOUT_ATOM(htmlNameSpace, "html")
LAYOUT_ATOM(xmlNameSpace, "xml")
LAYOUT_ATOM(xmlnsNameSpace, "xmlns")

  // Alphabetical list of frame additional child list names
LAYOUT_ATOM(absoluteList, "Absolute-list")
LAYOUT_ATOM(bulletList, "Bullet-list")
LAYOUT_ATOM(captionList, "Caption-list")
LAYOUT_ATOM(colGroupList, "ColGroup-list")
LAYOUT_ATOM(editorDisplayList, "EditorDisplay-List")
LAYOUT_ATOM(fixedList, "Fixed-list")
LAYOUT_ATOM(floatList, "Float-list")
LAYOUT_ATOM(overflowList, "Overflow-list")
LAYOUT_ATOM(overflowOutOfFlowList, "OverflowOutOfFlow-list")
LAYOUT_ATOM(popupList, "Popup-list")

LAYOUT_ATOM(commentTagName, "__moz_comment")
LAYOUT_ATOM(textTagName, "__moz_text")
LAYOUT_ATOM(processingInstructionTagName, "__moz_pi")
LAYOUT_ATOM(documentFragmentNodeName, "#document-fragment")

  // Alphabetical list of frame types
LAYOUT_ATOM(areaFrame, "AreaFrame")
LAYOUT_ATOM(bcTableCellFrame, "BCTableCellFrame") // table cell in border collapsing model
LAYOUT_ATOM(blockFrame, "BlockFrame")
LAYOUT_ATOM(boxFrame, "BoxFrame")
LAYOUT_ATOM(brFrame, "BRFrame")
LAYOUT_ATOM(bulletFrame, "BulletFrame")
LAYOUT_ATOM(fieldSetFrame, "FieldSetFrame")
LAYOUT_ATOM(gfxButtonControlFrame, "gfxButtonControlFrame")
LAYOUT_ATOM(subDocumentFrame, "subDocumentFrame")
LAYOUT_ATOM(imageFrame, "ImageFrame")
LAYOUT_ATOM(imageControlFrame, "ImageControlFrame")
LAYOUT_ATOM(inlineFrame, "InlineFrame")
LAYOUT_ATOM(legendFrame, "LegendFrame")
LAYOUT_ATOM(letterFrame, "LetterFrame")
LAYOUT_ATOM(lineFrame, "LineFrame")
LAYOUT_ATOM(listControlFrame,"ListControlFrame")
LAYOUT_ATOM(objectFrame, "ObjectFrame")
LAYOUT_ATOM(pageFrame, "PageFrame")
LAYOUT_ATOM(pageBreakFrame, "PageBreakFrame")
LAYOUT_ATOM(pageContentFrame, "PageContentFrame")
LAYOUT_ATOM(placeholderFrame, "PlaceholderFrame")
LAYOUT_ATOM(positionedInlineFrame, "PositionedInlineFrame")
LAYOUT_ATOM(canvasFrame, "CanvasFrame")
LAYOUT_ATOM(rootFrame, "RootFrame")
LAYOUT_ATOM(scrollFrame, "ScrollFrame")
LAYOUT_ATOM(sequenceFrame, "SequenceFrame")
LAYOUT_ATOM(tableCaptionFrame, "TableCaptionFrame")
LAYOUT_ATOM(tableCellFrame, "TableCellFrame")
LAYOUT_ATOM(tableColFrame, "TableColFrame")
LAYOUT_ATOM(tableColGroupFrame, "TableColGroupFrame")
LAYOUT_ATOM(tableFrame, "TableFrame")
LAYOUT_ATOM(tableOuterFrame, "TableOuterFrame")
LAYOUT_ATOM(tableRowGroupFrame, "TableRowGroupFrame")
LAYOUT_ATOM(tableRowFrame, "TableRowFrame")
LAYOUT_ATOM(textInputFrame,"TextInputFrame")
LAYOUT_ATOM(textFrame, "TextFrame")
LAYOUT_ATOM(viewportFrame, "ViewportFrame")

  // Alphabetical list of frame property names
LAYOUT_ATOM(changeListProperty, "ChangeListProperty") // void*
LAYOUT_ATOM(collapseOffsetProperty, "CollapseOffsetProperty")  // nsPoint*
LAYOUT_ATOM(computedOffsetProperty, "ComputedOffsetProperty")  // nsPoint*
LAYOUT_ATOM(IBSplitSpecialPrevSibling, "IBSplitSpecialPrevSibling")// nsIFrame*
LAYOUT_ATOM(IBSplitSpecialSibling, "IBSplitSpecialSibling")    // nsIFrame*
LAYOUT_ATOM(lineCursorProperty, "LineCursorProperty") // nsLineBox*
LAYOUT_ATOM(maxElementWidthProperty, "MaxElementWidthProperty")  // nscoord*
LAYOUT_ATOM(overflowAreaProperty, "OverflowArea")              // nsRect*
LAYOUT_ATOM(overflowProperty, "OverflowProperty")              // list of nsIFrame*
LAYOUT_ATOM(overflowLinesProperty, "OverflowLinesProperty")    // list of nsLineBox*
LAYOUT_ATOM(overflowOutOfFlowsProperty, "OverflowOutOfFlowsProperty")      // nsFrameList*
LAYOUT_ATOM(overflowPlaceholdersProperty, "OverflowPlaceholdersProperty")  // nsPlaceholder*
LAYOUT_ATOM(rowUnpaginatedHeightProperty, "RowUnpaginatedHeightProperty")  // nscoord*
LAYOUT_ATOM(spaceManagerProperty, "SpaceManagerProperty")      // the space manager for a block
LAYOUT_ATOM(tableBCProperty, "TableBCProperty")                // table border collapsing info (e.g. damage area, table border widths)
LAYOUT_ATOM(viewProperty, "ViewProperty")                      

  // Alphabetical list of event handler names
LAYOUT_ATOM(onabort, "onabort")
LAYOUT_ATOM(onbeforeunload, "onbeforeunload")
LAYOUT_ATOM(onblur, "onblur")
LAYOUT_ATOM(onbroadcast, "onbroadcast")
LAYOUT_ATOM(onchange, "onchange")
LAYOUT_ATOM(onclick, "onclick")
LAYOUT_ATOM(onclose, "onclose")
LAYOUT_ATOM(oncommand, "oncommand")
LAYOUT_ATOM(oncommandupdate, "oncommandupdate")
LAYOUT_ATOM(oncontextmenu, "oncontextmenu")
LAYOUT_ATOM(onDOMActivate, "onDOMActivate")
LAYOUT_ATOM(onDOMFocusIn, "onDOMFocusIn")
LAYOUT_ATOM(onDOMFocusOut, "onDOMFocusOut")
LAYOUT_ATOM(ondblclick, "ondblclick")
LAYOUT_ATOM(ondragdrop, "ondragdrop")
LAYOUT_ATOM(ondragenter, "ondragenter")
LAYOUT_ATOM(ondragexit, "ondragexit")
LAYOUT_ATOM(ondraggesture, "ondraggesture")
LAYOUT_ATOM(ondragover, "ondragover")
LAYOUT_ATOM(onerror, "onerror")
LAYOUT_ATOM(onfocus, "onfocus")
LAYOUT_ATOM(oninput, "oninput")
LAYOUT_ATOM(onkeydown, "onkeydown")
LAYOUT_ATOM(onkeypress, "onkeypress")
LAYOUT_ATOM(onkeyup, "onkeyup")
LAYOUT_ATOM(onload, "onload")
LAYOUT_ATOM(onmousedown, "onmousedown")
LAYOUT_ATOM(onmousemove, "onmousemove")
LAYOUT_ATOM(onmouseout, "onmouseout")
LAYOUT_ATOM(onmouseover, "onmouseover")
LAYOUT_ATOM(onmouseup, "onmouseup")
LAYOUT_ATOM(onpaint, "onpaint")
LAYOUT_ATOM(onpopuphidden, "onpopuphidden")
LAYOUT_ATOM(onpopuphiding, "onpopuphiding")
LAYOUT_ATOM(onpopupshowing, "onpopupshowing")
LAYOUT_ATOM(onpopupshown, "onpopupshown")
LAYOUT_ATOM(onreset, "onreset")
LAYOUT_ATOM(onresize, "onresize")
LAYOUT_ATOM(onscroll, "onscroll")
LAYOUT_ATOM(onselect, "onselect")
LAYOUT_ATOM(onsubmit, "onsubmit")
LAYOUT_ATOM(onunload, "onunload")

// scrolling
LAYOUT_ATOM(onoverflow, "onoverflow")
LAYOUT_ATOM(onunderflow, "onunderflow")
LAYOUT_ATOM(onoverflowchanged, "onoverflowchanged")

// mutation events
LAYOUT_ATOM(onDOMSubtreeModified, "onDOMSubtreeModified")
LAYOUT_ATOM(onDOMNodeInserted, "onDOMNodeInserted")
LAYOUT_ATOM(onDOMNodeRemoved, "onDOMNodeRemoved")
LAYOUT_ATOM(onDOMNodeRemovedFromDocument, "onDOMNodeRemovedFromDocument")
LAYOUT_ATOM(onDOMNodeInsertedIntoDocument, "onDOMNodeInsertedIntoDocument")
LAYOUT_ATOM(onDOMAttrModified, "onDOMAttrModified")
LAYOUT_ATOM(onDOMCharacterDataModified, "onDOMCharacterDataModified")

  // Alphabetical list of languages for lang-specific transforms
LAYOUT_ATOM(Japanese, "ja")
LAYOUT_ATOM(Unicode, "x-unicode")

  // other
LAYOUT_ATOM(wildcard, "*")
LAYOUT_ATOM(mozdirty, "_moz_dirty")
LAYOUT_ATOM(stylesheet, "stylesheet")
LAYOUT_ATOM(transform, "transform")
LAYOUT_ATOM(show, "show")
LAYOUT_ATOM(actuate, "actuate")

LAYOUT_ATOM(autoAtom, "auto")
LAYOUT_ATOM(none, "none")
LAYOUT_ATOM(normal, "normal")
LAYOUT_ATOM(transparent, "transparent")

LAYOUT_ATOM(directionalFrame, "DirectionalFrame")
LAYOUT_ATOM(baseLevel, "BaseLevel")                            // PRUint8
LAYOUT_ATOM(embeddingLevel, "EmbeddingLevel")                  // PRUint8
LAYOUT_ATOM(endsInDiacritic, "EndsInDiacritic")                // PRUint32
LAYOUT_ATOM(nextBidi, "NextBidi")                              // nsIFrame*
LAYOUT_ATOM(charType, "charType")                              // PRUint8

#ifdef DEBUG
  // Alphabetical list of atoms used by debugging code
LAYOUT_ATOM(cellMap, "TableCellMap")
LAYOUT_ATOM(imageMap, "ImageMap")
LAYOUT_ATOM(lineBoxBig, "LineBox:inline,big")
LAYOUT_ATOM(lineBoxBlockBig, "LineBox:block,big")
LAYOUT_ATOM(lineBoxBlockSmall, "LineBox:block,small")
LAYOUT_ATOM(lineBoxFloats, "LineBoxFloats")
LAYOUT_ATOM(lineBoxSmall, "LineBox:inline,small")
LAYOUT_ATOM(spaceManager, "SpaceManager")
LAYOUT_ATOM(tableColCache, "TableColumnCache")
LAYOUT_ATOM(tableStrategy, "TableLayoutStrategy")
LAYOUT_ATOM(textRun, "TextRun")
LAYOUT_ATOM(xml_document_entities, "XMLDocumentEntities")
LAYOUT_ATOM(xml_document_notations, "XMLDocumentNotations")
#endif
