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

#import "MVPreferencesGroupedIconView.h"
#import "MVPreferencesMultipleIconView.h"
#import "MVPreferencesController.h"

@interface MVPreferencesMultipleIconView (MVPreferencesMultipleIconViewPrivate)
- (unsigned int) _numberOfRows;
@end

@interface MVPreferencesGroupedIconView (MVPreferencesGroupedIconViewPrivate)

- (unsigned int) _numberOfGroups;
- (void) _sizeToFit;
- (NSRect) _rectForGroupAtIndex:(unsigned int) index;
- (NSRect) _rectForGroupSeparatorAtIndex:(unsigned int) index;
- (void) _generateGroupViews;
- (void) _generateGroupViewAtIndex:(unsigned int) index;

@end

@implementation MVPreferencesGroupedIconView

const unsigned int groupViewHeight = 95;
const unsigned int multiIconViewYOffset = 18;
const unsigned int sideGutterWidths = 15;
const unsigned int extraRowHeight = 69;

- (id) initWithFrame:(NSRect) frame
{
  if( ( self = [super initWithFrame:frame] ) ) {
		groupMultiIconViews = [[NSMutableArray array] retain];
  }
  return self;
}

- (void) drawRect:(NSRect) rect
{
	[self _generateGroupViews];
}

- (void) setPreferencesController:(MVPreferencesController *) newPreferencesController
{
	[preferencesController autorelease];
	preferencesController = [newPreferencesController retain];
	[self _sizeToFit];
	[self setNeedsDisplay:YES];
}

- (void) setPreferencePanes:(NSArray *) newPreferencePanes
{
	[preferencePanes autorelease];
	preferencePanes = [newPreferencePanes retain];
	[self _sizeToFit];
	[self setNeedsDisplay:YES];
}

- (NSArray *) preferencePanes 
{
	return preferencePanes;
}

- (void) setPreferencePaneGroups:(NSArray *) newPreferencePaneGroups
{
	[preferencePaneGroups autorelease];
	preferencePaneGroups = [newPreferencePaneGroups retain];
	[self _sizeToFit];
	[self setNeedsDisplay:YES];
}

- (NSArray *) preferencePaneGroups
{
	return preferencePaneGroups;
}

- (BOOL) acceptsFirstResponder
{
	return YES;
}

- (BOOL) becomeFirstResponder
{
	[[self window] makeFirstResponder:[self viewWithTag:0]];
	return YES;
}

- (BOOL) resignFirstResponder
{
	return YES;
}

- (BOOL) isFlipped
{
	return YES;
}

- (BOOL) isOpaque
{
	return NO;
}
@end

@implementation MVPreferencesGroupedIconView (MVPreferencesGroupedIconViewPrivate)
- (unsigned int) _numberOfGroups {
	return [[self preferencePaneGroups] count];
}

- (void) _sizeToFit {
	if( ! [self _numberOfGroups] ) return;
	[self _generateGroupViews];
	[self setFrameSize:NSMakeSize( NSWidth( _bounds ), NSMaxY( [self _rectForGroupAtIndex:[self _numberOfGroups] - 1] ) ) ];
}

- (NSRect) _rectForGroupAtIndex:(unsigned int) index {
	unsigned int extra = 0, groupIndex;
	for( groupIndex = 0; groupIndex < index; groupIndex++ )
		extra += [[self viewWithTag:groupIndex] _numberOfRows] * extraRowHeight;
	return NSMakeRect( 0., (index * groupViewHeight ) + multiIconViewYOffset + index + extra, NSWidth( [self frame] ), groupViewHeight );
}

- (NSRect) _rectForGroupSeparatorAtIndex:(unsigned int) index {
	unsigned int extra = 0, groupIndex;
	if( ! index ) return NSZeroRect;
	for( groupIndex = 0; groupIndex < index; groupIndex++ )
		extra += [[self viewWithTag:groupIndex] _numberOfRows] * extraRowHeight;
	return NSMakeRect( sideGutterWidths, ((index + 1) * groupViewHeight) - groupViewHeight + index + extra, NSWidth( [self frame] ) - (sideGutterWidths * 2), 1 );
}

- (void) _generateGroupViews {
	unsigned int groupIndex, groupCount;

	groupCount = [self _numberOfGroups];
	for( groupIndex = 0; groupIndex < groupCount; groupIndex++ )
		[self _generateGroupViewAtIndex:groupIndex];
}

- (void) _generateGroupViewAtIndex:(unsigned int) index {
	MVPreferencesMultipleIconView *multiIconView = nil;
	NSString *identifier = [[preferencePaneGroups objectAtIndex:index] objectForKey:@"identifier"];
	NSString *name = NSLocalizedStringFromTable( identifier, @"MVPreferencePaneGroups", nil );
	NSDictionary *attributesDictionary;
	unsigned nameHeight = 0;

	if( ! ( multiIconView = [self viewWithTag:index] ) ) {
		NSMutableArray *panes = [NSMutableArray array];
		NSBox *sep = [[[NSBox alloc] initWithFrame:[self _rectForGroupSeparatorAtIndex:index]] autorelease];
		NSEnumerator *enumerator = [[[preferencePaneGroups objectAtIndex:index] objectForKey:@"panes"] objectEnumerator];
		id pane = nil, bundle = 0;

		multiIconView = [[[MVPreferencesMultipleIconView alloc] initWithFrame:[self _rectForGroupAtIndex:index]] autorelease];
		while( ( pane = [enumerator nextObject] ) ) {
			bundle = [NSBundle bundleWithIdentifier:pane];
			if( bundle ) [panes addObject:bundle];
		}

		[multiIconView setPreferencesController:preferencesController];
		[multiIconView setPreferencePanes:panes];
		[multiIconView setTag:index];

		[sep setBoxType:NSBoxSeparator];
		[sep setBorderType:NSBezelBorder];
	
		[self addSubview:multiIconView];
		[self addSubview:sep];
//		if( ! index ) [[self window] makeFirstResponder:multiIconView];
		if( index - 1 >= 0 ) [[self viewWithTag:index - 1] setNextKeyView:multiIconView];
		if( index == [self _numberOfGroups] - 1 ) [multiIconView setNextKeyView:[self viewWithTag:0]];
	}

	attributesDictionary = [NSDictionary dictionaryWithObjectsAndKeys:[NSFont boldSystemFontOfSize:13.], NSFontAttributeName, nil];
	nameHeight = [[NSFont boldSystemFontOfSize:13.] ascender];
	[name drawAtPoint:NSMakePoint( sideGutterWidths - 1, NSMinY( [multiIconView frame] ) - nameHeight - 1 ) withAttributes:attributesDictionary];
}
@end
