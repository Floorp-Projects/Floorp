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
* The Original Code is mozilla.org code.
*
* The Initial Developer of the Original Code is 
* Netscape Communications Corporation.
* Portions created by the Initial Developer are Copyright (C) 2002
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
*  Geoff Beier <me@mollyandgeoff.com>
*  Based on BrowserTabViewItem.mm by Simon Fraser <sfraser@netscape.com>
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

#import "TruncatingTextAndImageCell.h"
#import "NSString+Utils.h"

// this was in BrowserTabViewItem, but the comment stated that it needed to be moved
// since I needed it outside BrowserTabViewItem, I moved it. I also renamed it since it was
// easy and I'm confused by non-Apple names that start with NS :-)

@implementation TruncatingTextAndImageCell

-(id)initTextCell:(NSString*)aString
{
  if ((self = [super initTextCell:aString])) {
    mLabelStringWidth = -1;
    mImagePadding = 0;
    mImageSpace = 2;
    mMaxImageHeight = 16;
	  mRightGutter = 0.0;
    mImageIsVisible = NO;
  }
  return self;
}

-(void)dealloc
{
  [mProgressIndicator removeFromSuperview];
  [mProgressIndicator release];
  [mImage release];
  [mTruncLabelString release];
  [super dealloc];
}

-copyWithZone:(NSZone *)zone
{
  TruncatingTextAndImageCell *cell = (TruncatingTextAndImageCell *)[super copyWithZone:zone];
  cell->mImage = [mImage retain];
  cell->mTruncLabelString = nil;
  cell->mLabelStringWidth = -1;
  cell->mRightGutter = mRightGutter;
  cell->mImageIsVisible = mImageIsVisible;
  return cell;
}

-(NSRect )imageFrame
{
  return mImageFrame;
}

// currently draws progress indicator or favicon followed by label
-(void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView
{
  NSRect textRect = cellFrame;
  NSRect imageRect;
  
  // we always reserve space for the image, even if there isn't one
  // assume the image rect is always square
  float imageWidth = NSHeight(cellFrame) - 2 * mImagePadding;
  // draw the progress indicator if we have a reference to it, otherwise draw the favicon
  if (mProgressIndicator) {
    NSDivideRect(cellFrame, &textRect, &imageRect, NSWidth(cellFrame) - imageWidth, NSMinXEdge);
    if (controlView != [mProgressIndicator superview]) {
      [controlView addSubview:mProgressIndicator];
    }
    [mProgressIndicator setFrame:imageRect];
    [mProgressIndicator setNeedsDisplay:YES];
  }
  else if (mImage && mImageIsVisible) {
    //Put the favicon to the left of the text
    NSDivideRect(cellFrame,&imageRect,&textRect,imageWidth,NSMinXEdge);
    NSRect imageSrcRect = NSZeroRect;
    imageSrcRect.size = [mImage size];
    float imagePadding = mImagePadding;
    // I don't think this will be needed in practice, but if the favicon is smaller than
    // our planned size, it should be padded rather than left on the bottom of the cell IMO
    if (imageRect.size.height > mMaxImageHeight)
      imagePadding += (imageRect.size.height - mMaxImageHeight);
    [mImage drawInRect:NSInsetRect(imageRect, mImagePadding, mImagePadding)
            fromRect:imageSrcRect operation:NSCompositeSourceOver fraction:mImageAlpha];
  }
  else {
    NSDivideRect(cellFrame,&imageRect,&textRect,imageWidth,NSMinXEdge);
  }
  mImageFrame = [controlView convertRect:imageRect toView:nil];
  // remove image space
  NSDivideRect(textRect, &imageRect, &textRect, mImageSpace, NSMinXEdge);
  
  int cellWidth = (int)NSWidth(textRect) - (int)mRightGutter;
  NSDictionary *cellAttributes = [[self attributedStringValue] attributesAtIndex:0 effectiveRange:nil];
  
  if (mLabelStringWidth != cellWidth || !mTruncLabelString) {
    [mTruncLabelString release];
    mTruncLabelString = [[NSMutableString alloc] initWithString:[self stringValue]];
    [mTruncLabelString truncateToWidth:cellWidth at:kTruncateAtEnd withAttributes:cellAttributes];
    mLabelStringWidth = cellWidth;
  }
  
  [mTruncLabelString drawInRect:textRect withAttributes:cellAttributes];
}

-(void)setStringValue:(NSString *)aString
{
  if (![aString isEqualToString:[self stringValue]]) {
    [mTruncLabelString release];
    mTruncLabelString = nil;
  }
  [super setStringValue:aString];
}

-(void)setAttributedStringValue:(NSAttributedString *)attribStr
{
  if (![attribStr isEqualToAttributedString:[self attributedStringValue]]) {
    [mTruncLabelString release];
    mTruncLabelString = nil;
  }
  [super setAttributedStringValue:attribStr];
}

-(void)setImage:(NSImage *)anImage 
{
  if (anImage != mImage) {
    [mImage release];
    mImage = [anImage retain];
  }
}

-(void)setImageVisible:(BOOL)visible
{
  mImageIsVisible = visible;
}

-(NSImage *)image
{
  return mImage;
}

-(void)setImagePadding:(float)padding
{
  mImagePadding = padding;
}

-(void)setRightGutter:(float)rightPadding
{
  mRightGutter = rightPadding;
}

-(void)setImageSpace:(float)space
{
  mImageSpace = space;
}

-(void)setImageAlpha:(float)alpha
{
  mImageAlpha = alpha;
}

// called by BrowserTabViewItem when progress display should start
- (void)addProgressIndicator:(NSProgressIndicator*)indicator
{
  mProgressIndicator = [indicator retain];
}

// called by BrowserTabviewItem when progress display is finished
// and the progress indicator should be replaced with the favicon
- (void)removeProgressIndicator
{
  [mProgressIndicator removeFromSuperview];
  [mProgressIndicator release];
  mProgressIndicator = nil;
}

// called by TabButtonCell to constrain the height of the favicon to the height of the close button
// for best results, both should be 16x16
- (void)setMaxImageHeight:(float)height
{
  mMaxImageHeight = height;
}

@end
