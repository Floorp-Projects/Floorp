/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsILineIterator_h___
#define nsILineIterator_h___

#include "nsISupports.h"

/* a6cf90ff-15b3-11d2-932e-00805f8add32 */
#define NS_ILINE_ITERATOR_IID \
 { 0xa6cf90ff, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

/* {80AA3D7A-E0BF-4e18-8A82-2110397D7BC4}*/
#define NS_ILINE_ITERATOR_NAV_IID \
 { 0x80aa3d7a, 0xe0bf, 0x4e18,{0x8a, 0x82, 0x21, 0x10, 0x39, 0x7d, 0x7b, 0xc4}}

// Line iterator API.
//
// Lines are numbered from 0 to N, where 0 is the top line and N is
// the bottom line.
//
// NOTE: while you can get this interface by doing a slezy hacky
// QueryInterface on block frames, it isn't like a normal com
// interface: its not reflexive (you can't query back to the block
// frame) and unlike other frames, it *IS* reference counted so don't
// forget to NS_RELEASE it when you are done with it!

// Line Flags (see GetLine below)

// This bit is set when the line is wrapping up a block frame. When
// clear, it means that the line contains inline elements.
#define NS_LINE_FLAG_IS_BLOCK           0x1

// This bit is set when the line ends in some sort of break.
#define NS_LINE_FLAG_ENDS_IN_BREAK      0x4

class nsILineIterator : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ILINE_ITERATOR_IID)

  // Return the number of lines in the block.
  NS_IMETHOD GetNumLines(PRInt32* aResult) = 0;

  // Return the prevailing direction for the line. aIsRightToLeft will
  // be set to PR_TRUE if the CSS direction property for the block is
  // "rtl", otherwise aIsRightToLeft will be set to PR_FALSE.
  NS_IMETHOD GetDirection(PRBool* aIsRightToLeft) = 0;

  // Return structural information about a line. aFirstFrameOnLine is
  // the first frame on the line and aNumFramesOnLine is the number of
  // frames that are on the line. If the line-number is invalid then
  // aFirstFrameOnLine will be nsnull and aNumFramesOnLine will be
  // zero.
  //
  // For valid line numbers, aLineBounds is set to the bounding box of
  // the line (which is based on the in-flow position of the frames on
  // the line; if a frame was moved because of relative positioning
  // then its coordinates may be outside the line bounds).
  //
  // In addition, aLineFlags will contain flag information about the
  // line.
  NS_IMETHOD GetLine(PRInt32 aLineNumber,
                     nsIFrame** aFirstFrameOnLine,
                     PRInt32* aNumFramesOnLine,
                     nsRect& aLineBounds,
                     PRUint32* aLineFlags) = 0;

  // Given a frame thats a child of the block, find which line its on
  // and return that line index into aIndexResult. aIndexResult will
  // be set to -1 if the frame cannot be found.
  NS_IMETHOD FindLineContaining(nsIFrame* aFrame,
                                PRInt32* aLineNumberResult) = 0;

  // Given a Y coordinate relative to the block that provided this
  // line iterator, find the line that contains the Y
  // coordinate. Returns -1 in aLineNumberResult if the Y coordinate
  // is above the first line. Returns N (where N is the number of
  // lines) if the Y coordinate is below the last line.
  NS_IMETHOD FindLineAt(nscoord aY,
                        PRInt32* aLineNumberResult) = 0;

  // Given a line number and an X coordinate, find the frame on the
  // line that is nearest to the X coordinate. The
  // aXIsBeforeFirstFrame and aXIsAfterLastFrame flags are updated
  // appropriately.
  NS_IMETHOD FindFrameAt(PRInt32 aLineNumber,
                         nscoord aX,
#ifdef IBMBIDI
                         //This parameter determines whether to traverse the frames in logical or visual order
                         PRBool aCouldBeReordered,
#endif
                         nsIFrame** aFrameFound,
                         PRBool* aXIsBeforeFirstFrame,
                         PRBool* aXIsAfterLastFrame) = 0;

  // Give the line iterator implementor a chance todo something more complicated than
  // nsIFrame::GetNextSibling()
  NS_IMETHOD GetNextSiblingOnLine(nsIFrame*& aFrame, PRInt32 aLineNumber) = 0;

#ifdef IBMBIDI
  // Check whether visual and logical order of frames within a line are identical.
  //  If not, return the first and last visual frames
  NS_IMETHOD CheckLineOrder(PRInt32                  aLine,
                            PRBool                   *aIsReordered,
                            nsIFrame                 **aFirstVisual,
                            nsIFrame                 **aLastVisual) = 0;
#endif
};

//special line iterator for keyboard navigation
class nsILineIteratorNavigator : public nsILineIterator {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ILINE_ITERATOR_NAV_IID)

};
#endif /* nsILineIterator_h___ */
