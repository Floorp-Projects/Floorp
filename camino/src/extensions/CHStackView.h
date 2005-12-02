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
 * The Original Code is Chimera code.
 *
 * The Initial Developer of the Original Code is
 * Calum Robinson.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Calum Robinson <calumr@mac.com>
 *   Simon Fraser <smfr@smfr.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#import <AppKit/AppKit.h>

// views contained in the stack should send this notification when they change size
extern NSString* const kStackViewReloadNotificationName;

// the stack view sends this notification after it has adjusted to subview sizes
extern NSString* const kStackViewResizedNotificationName;


// 
// CHShrinkWrapView
// 
// A view that resizes to contain its subviews.
// 
// Early on, it will look at its subview positions, and calculate
// the "intrinsic padding" (i.e. the space around the subviews),
// and use this when sizing itself. It will then resize itself to
// contain its subviews, plus the padding, whenever one if its
// direct subviews resizes.
// 
// Can be used in Interface Builder, if you have built and installed
// the CaminoView.palette IB Palette.
// 

@interface CHShrinkWrapView : NSView
{
  float       mIntrinsicPadding[4];  // NSMinXEdge, NSMinYEdge, NSMaxXEdge, NSMaxYEdge
  BOOL        mFetchedIntrinsicPadding;
  BOOL        mPaddingSetManually;
  BOOL        mSelfResizing;
}

// padding will normally be calculated automatically from the subview positions,
// but you can set it explicilty if you wish.
- (void)setIntrinsicPadding:(float)inPadding forEdge:(NSRectEdge)inEdge;
- (float)paddingForEdge:(NSRectEdge)inEdge;

- (void)adaptToSubviews;

@end



// 
// CHFlippedShrinkWrapView
// 
// A subview of CHShrinkWrapView that is flipped.
// 
// This is used when nesting a CHStackView inside an NSScrollView,
// because NSScrollView messes up is its containerView isn't flipped
// (really).
// 
// If you have one of these in IB, editing contained views is broken
// (view outlines draw flipped, mouse interaction is busted).
// 

@interface CHFlippedShrinkWrapView : CHShrinkWrapView
{
}

@end

// 
// CHStackView
// 
// A view that lays out its subviews top to bottom.
// 
// It can either manage a static set of subviws as specified in
// the nib, or the subviews can be supplied by a "data source".
// If using a data source, the stack view can optionally insert
// a separator view after each supplied view.
// 
// The data source, if used, must retain the views.
// 
// Can be used in Interface Builder, if you have built and installed
// the CaminoView.palette IB Palette.
// 

@interface CHStackView : CHShrinkWrapView
{
  IBOutlet id mDataSource;
  BOOL        mShowsSeparators;
//  BOOL        mIsResizingSubviews;
}

- (void)setDataSource:(id)aDataSource;

// default is NO
- (BOOL)showsSeparators;
- (void)setShowsSeparators:(BOOL)inShowSeparators;

@end

@protocol CHStackViewDataSource

- (NSArray*)subviewsForStackView:(CHStackView *)stackView;

@end
