/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL") you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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
LAYOUT_ATOM(colGroupList, "ColGroup-list")
LAYOUT_ATOM(fixedList, "Fixed-list")
LAYOUT_ATOM(floaterList, "Floater-list")
LAYOUT_ATOM(popupList, "Popup-list")

  // Alphabetical list of pseudo tag names for non-element content
LAYOUT_ATOM(canvasPseudo, ":canvas")
LAYOUT_ATOM(commentTagName, "__moz_comment")
LAYOUT_ATOM(dummyOptionPseudo, ":-moz-dummy-option")
LAYOUT_ATOM(textTagName, "__moz_text")
LAYOUT_ATOM(pagePseudo, ":-moz-page")
LAYOUT_ATOM(pageSequencePseudo, ":-moz-page-sequence")
LAYOUT_ATOM(processingInstructionTagName, "__moz_pi")
LAYOUT_ATOM(scrolledContentPseudo, ":scrolled-content")
LAYOUT_ATOM(viewportPseudo, ":viewport")
LAYOUT_ATOM(viewportScrollPseudo, ":viewport-scroll")

  // Alphabetical list of frame types
LAYOUT_ATOM(areaFrame, "AreaFrame")
LAYOUT_ATOM(blockFrame, "BlockFrame")
LAYOUT_ATOM(bulletFrame, "BulletFrame")
LAYOUT_ATOM(hrFrame, "HRFrame")
LAYOUT_ATOM(htmlFrameInnerFrame, "htmlFrameInnerFrame")
LAYOUT_ATOM(htmlFrameOuterFrame, "htmlFrameOuterFrame")
LAYOUT_ATOM(imageFrame, "ImageFrame")
LAYOUT_ATOM(inlineFrame, "InlineFrame")
LAYOUT_ATOM(letterFrame, "LetterFrame")
LAYOUT_ATOM(lineFrame, "LineFrame")
LAYOUT_ATOM(objectFrame, "ObjectFrame")
LAYOUT_ATOM(pageFrame, "PageFrame")
LAYOUT_ATOM(placeholderFrame, "PlaceholderFrame")
LAYOUT_ATOM(positionedInlineFrame, "PositionedInlineFrame")
LAYOUT_ATOM(rootFrame, "RootFrame")
LAYOUT_ATOM(scrollFrame, "ScrollFrame")
LAYOUT_ATOM(tableCellFrame, "TableCellFrame")
LAYOUT_ATOM(tableColFrame, "TableColFrame")
LAYOUT_ATOM(tableColGroupFrame, "TableColGroupFrame")
LAYOUT_ATOM(tableFrame, "TableFrame")
LAYOUT_ATOM(tableOuterFrame, "TableOuterFrame")
LAYOUT_ATOM(tableRowGroupFrame, "TableRowGroupFrame")
LAYOUT_ATOM(tableRowFrame, "TableRowFrame")
LAYOUT_ATOM(textFrame, "TextFrame")
LAYOUT_ATOM(viewportFrame, "ViewportFrame")

  // Alphabetical list of frame property names
LAYOUT_ATOM(collapseOffsetProperty, "CollapseOffsetProperty")
LAYOUT_ATOM(maxElementSizeProperty, "MaxElementSizeProperty")
LAYOUT_ATOM(overflowProperty, "OverflowProperty")
LAYOUT_ATOM(viewProperty, "ViewProperty")

#ifdef DEBUG
  // Alphabetical list of atoms used by debugging code
LAYOUT_ATOM(cellMap, "TableCellMap")
LAYOUT_ATOM(imageMap, "ImageMap")
LAYOUT_ATOM(lineBoxBig, "LineBox:inline,big")
LAYOUT_ATOM(lineBoxBlockBig, "LineBox:block,big")
LAYOUT_ATOM(lineBoxBlockSmall, "LineBox:block,small")
LAYOUT_ATOM(lineBoxFloaters, "LineBoxFloaters")
LAYOUT_ATOM(lineBoxSmall, "LineBox:inline,small")
LAYOUT_ATOM(spaceManager, "SpaceManager")
LAYOUT_ATOM(tableColCache, "TableColumnCache")
LAYOUT_ATOM(tableStrategy, "TableLayoutStrategy")
LAYOUT_ATOM(textRun, "TextRun")
LAYOUT_ATOM(xml_document_entities, "XMLDocumentEntities")
LAYOUT_ATOM(xml_document_notations, "XMLDocumentNotations")
#endif
