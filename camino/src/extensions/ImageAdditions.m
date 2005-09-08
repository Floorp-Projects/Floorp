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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
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

#import "ImageAdditions.h"
#import <Cocoa/Cocoa.h>

@implementation NSImage (ImageAdditions)

- (void) drawFlippedInRect:(NSRect) rect operation:(NSCompositingOperation) op fraction:(float) delta
{
	CGContextRef context;

	context = [[NSGraphicsContext currentContext] graphicsPort];
	CGContextSaveGState( context ); {
		CGContextTranslateCTM( context, 0, NSMaxY( rect ) );
		CGContextScaleCTM( context, 1, -1 );

		rect.origin.y = 0;
		[self drawInRect:rect fromRect:NSZeroRect operation:op fraction:delta];
	} CGContextRestoreGState( context );
}

- (void) drawFlippedInRect:(NSRect) rect operation:(NSCompositingOperation) op
{
    [self drawFlippedInRect:rect operation:op fraction:1.0];
}

- (NSImage*)imageByApplyingBadge:(NSImage*)badge withAlpha:(float)alpha scale:(float)scale;
{
  if (!badge)
    return self;

  // bad to actually change badge here  
  [badge setScalesWhenResized:YES];
  [badge setSize:NSMakeSize([self size].width * scale,[self size].height * scale)];

  // make a new image, copy over our best rep into it
  NSImage* newImage = [[[NSImage alloc] initWithSize:[self size]] autorelease];
  NSImageRep* imageRep = [[self bestRepresentationForDevice:nil] copy];
  [newImage addRepresentation:imageRep];
  [imageRep release];
  
  [newImage lockFocus];
  [[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationHigh];
  [badge dissolveToPoint:NSMakePoint([self size].width - [badge size].width, 0.0) fraction:alpha];
  [newImage unlockFocus];

  return newImage;
}

@end
