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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: Aaron Leventhal (aaronl@netscape.com)
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

  This file contains the list of all accessibility nsIAtoms and their values
  
  It is designed to be used as inline input to nsAccessibilityAtoms.cpp *only*
  through the magic of C preprocessing.

  All entires must be enclosed in the macro ACCESSIBILITY_ATOM which will have cruel
  and unusual things done to it

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order

  The first argument to ACCESSIBILITY_ATOM is the C++ identifier of the atom
  The second argument is the string value of the atom

 ******/


  // Alphabetical list of frame types
ACCESSIBILITY_ATOM(blockFrame, "BlockFrame")
ACCESSIBILITY_ATOM(inlineFrame, "InlineFrame")
ACCESSIBILITY_ATOM(objectFrame, "ObjectFrame")
ACCESSIBILITY_ATOM(textFrame, "TextFrame")

  // Alphabetical list of tag names
ACCESSIBILITY_ATOM(a, "a")
ACCESSIBILITY_ATOM(ol, "ol")
ACCESSIBILITY_ATOM(optgroup, "optgroup")
ACCESSIBILITY_ATOM(option, "option")
ACCESSIBILITY_ATOM(ul, "ul")

  // Alphabetical list of attributes
ACCESSIBILITY_ATOM(id, "id")
ACCESSIBILITY_ATOM(tabindex, "tabindex")
