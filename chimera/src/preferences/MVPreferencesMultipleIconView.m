#import <Cocoa/Cocoa.h>
#import "MVPreferencesMultipleIconView.h"
#import "MVPreferencesController.h"
#import "ImageAdditions.h"

@interface MVPreferencesMultipleIconView (MVPreferencesMultipleIconViewPrivate)
- (void) _focusFirst;
- (void) _focusLast;
- (unsigned int) _iconsWide;
- (unsigned int) _numberOfIcons;
- (unsigned int) _numberOfRows;
- (BOOL) _isIconSelectedAtIndex:(unsigned int) index;
- (BOOL) _column:(unsigned int *) column andRow:(unsigned int *) row forIndex:(unsigned int) index;
- (NSRect) _boundsForIndex:(unsigned int) index;
- (BOOL) _iconImage:(NSImage **) image andName:(NSString **) name forIndex:(unsigned int) index;
- (BOOL) _iconImage:(NSImage **) image andName:(NSString **) name andIdentifier:(NSString **) identifier forIndex:(unsigned int) index;
- (void) _drawIconAtIndex:(unsigned int) index drawRect:(NSRect) drawRect;
- (void) _sizeToFit;
- (BOOL) _dragIconIndex:(unsigned int) index event:(NSEvent *) event;
- (BOOL) _dragIconImage:(NSImage *) iconImage andName:(NSString *) name event:(NSEvent *) event;
- (BOOL) _dragIconImage:(NSImage *) iconImage andName:(NSString *) name andIdentifier:(NSString *) identifier event:(NSEvent *) event;
@end

@interface MVPreferencesController (MVPreferencesControllerPrivate)
- (IBAction) _selectPreferencePane:(id) sender;
- (void) _resizeWindowForContentView:(NSView *) view;
- (NSImage *) _imageForPaneBundle:(NSBundle *) bundle;
- (NSString *) _paletteLabelForPaneBundle:(NSBundle *) bundle;
- (NSString *) _labelForPaneBundle:(NSBundle *) bundle;
@end

@implementation MVPreferencesMultipleIconView
const NSSize buttonSize = { 81., 75. };
const NSSize iconSize = { 32., 32. };
const unsigned int titleBaseline = 15;
const unsigned int iconBaseline = 30;
const unsigned int bottomBorder = 15;

- (id) initWithFrame:(NSRect) rect {
	if( ( self = [super initWithFrame:rect] ) ) {
		pressedIconIndex = NSNotFound;
		focusedIndex = NSNotFound;
		selectedPane = nil;
	}
	return self;
}

- (void) setPreferencesController:(MVPreferencesController *) newPreferencesController {
	[preferencesController autorelease];
	preferencesController = [newPreferencesController retain];
	[self setNeedsDisplay:YES];
}

- (void) setPreferencePanes:(NSArray *) newPreferencePanes {
	[preferencePanes autorelease];
	preferencePanes = [newPreferencePanes retain];
	[self _sizeToFit];
	[self setNeedsDisplay:YES];
}

- (NSArray *) preferencePanes {
	return preferencePanes;
}

- (void) setSelectedPane:(NSBundle *) newSelectedPane {
	selectedPane = newSelectedPane;
	[self setNeedsDisplay:YES];
}

- (void) mouseDown:(NSEvent *) event {
	NSPoint eventLocation;
	NSRect slopRect;
	const float dragSlop = 4.;
	unsigned int index;
	NSRect buttonRect;
	BOOL mouseInBounds = NO;

	eventLocation = [self convertPoint:[event locationInWindow] fromView:nil];
	slopRect = NSInsetRect( NSMakeRect( eventLocation.x, eventLocation.y, 1., 1. ), -dragSlop, -dragSlop );

	index = floor( eventLocation.x / buttonSize.width ) + ( floor( eventLocation.y / buttonSize.height ) * [self _iconsWide] );
	buttonRect = [self _boundsForIndex:index];
	if( index >= ( [self _iconsWide] * ( floor( eventLocation.y / buttonSize.height ) + 1 ) ) ) return;
	if( ! NSWidth( buttonRect ) ) return;

	pressedIconIndex = index;
	[self setNeedsDisplayInRect:[self _boundsForIndex:index]];

	while( 1 ) {
		NSEvent *nextEvent;
		NSPoint nextEventLocation;
		unsigned int newPressedIconIndex;

		nextEvent = [NSApp nextEventMatchingMask:NSLeftMouseDraggedMask|NSLeftMouseUpMask untilDate:[NSDate distantFuture] inMode:NSEventTrackingRunLoopMode dequeue:YES];

		nextEventLocation = [self convertPoint:[nextEvent locationInWindow] fromView:nil];
		mouseInBounds = NSMouseInRect(nextEventLocation, buttonRect, [self isFlipped]);
		newPressedIconIndex = ( mouseInBounds ? index : NSNotFound );
		if( newPressedIconIndex != pressedIconIndex ) {
			pressedIconIndex = newPressedIconIndex;
			[self setNeedsDisplayInRect:[self _boundsForIndex:pressedIconIndex]];
		}

		if( [nextEvent type] == NSLeftMouseUp ) break;
		else if( ! NSMouseInRect( nextEventLocation, slopRect, NO ) ) {
			if( [self _dragIconIndex:index event:event] ) {
				mouseInBounds = NO;
				break;
			}
		}
	}

	pressedIconIndex = NSNotFound;
	[self setNeedsDisplayInRect:[self _boundsForIndex:index]];

	if( mouseInBounds )
		[preferencesController selectPreferencePaneByIdentifier:[[preferencePanes objectAtIndex:index] bundleIdentifier]];
}

#define kTabCharCode 9
#define kShiftTabCharCode 25
#define kSpaceCharCode 32

- (void) keyDown:(NSEvent *) theEvent {
	NSView *nextView = nil;
	if( [[theEvent charactersIgnoringModifiers] characterAtIndex:0] == kTabCharCode ) {
		[self setKeyboardFocusRingNeedsDisplayInRect:[self _boundsForIndex:focusedIndex]];
		if( focusedIndex == NSNotFound && [self _numberOfIcons] ) focusedIndex = 0;
		else if( ! ( [theEvent modifierFlags] & NSShiftKeyMask ) ) focusedIndex++;
		if( focusedIndex >= [self _numberOfIcons] ) {
			if( ( nextView = [self nextValidKeyView] ) ) {
				[[self window] makeFirstResponder:nextView];
				focusedIndex = NSNotFound;
				if( [nextView isKindOfClass:[MVPreferencesMultipleIconView class]] )
					[(MVPreferencesMultipleIconView *)nextView _focusFirst];
			} else focusedIndex = 0;
		}
		if( ! [self _numberOfIcons] ) focusedIndex = NSNotFound;
		[self setNeedsDisplayInRect:[self _boundsForIndex:focusedIndex]];
	} else if( [[theEvent charactersIgnoringModifiers] characterAtIndex:0] == kShiftTabCharCode ) {
		[self setKeyboardFocusRingNeedsDisplayInRect:[self _boundsForIndex:focusedIndex]];
		if( focusedIndex == NSNotFound && [self _numberOfIcons] ) focusedIndex = [self _numberOfIcons] - 1;
		else if( [theEvent modifierFlags] & NSShiftKeyMask ) focusedIndex--;
		if( (signed) focusedIndex < 0 && [self _numberOfIcons] ) {
			if( ( nextView = [self previousValidKeyView] ) ) {
				[[self window] makeFirstResponder:nextView];
				focusedIndex = NSNotFound;
				if( [nextView isKindOfClass:[MVPreferencesMultipleIconView class]] )
					[(MVPreferencesMultipleIconView *)nextView _focusLast];
			} else focusedIndex = [self _numberOfIcons] - 1;
		}
		if( ! [self _numberOfIcons] ) focusedIndex = NSNotFound;
		[self setNeedsDisplayInRect:[self _boundsForIndex:focusedIndex]];
	} else if( [[theEvent charactersIgnoringModifiers] characterAtIndex:0] == kSpaceCharCode ) {
		if( focusedIndex != NSNotFound && focusedIndex < [self _numberOfIcons] && focusedIndex >= 0 ) {
			[preferencesController selectPreferencePaneByIdentifier:[[preferencePanes objectAtIndex:focusedIndex] bundleIdentifier]];
			focusedIndex = NSNotFound;
		}
	}
}

- (BOOL) acceptsFirstResponder {
	return YES;
}

- (BOOL) becomeFirstResponder {
	focusedIndex = NSNotFound;
	return YES;
}

- (BOOL) resignFirstResponder {
	[self setKeyboardFocusRingNeedsDisplayInRect:[self _boundsForIndex:focusedIndex]];
	focusedIndex = NSNotFound;
	return YES;
}

- (void) drawRect:(NSRect) rect {
	unsigned int paneIndex, paneCount;

	paneCount = [self _numberOfIcons];
	for( paneIndex = 0; paneIndex < paneCount; paneIndex++ )
		[self _drawIconAtIndex:paneIndex drawRect:rect];
}

- (BOOL) isFlipped {
	return YES;
}

- (BOOL) isOpaque {
	return NO;
}

- (int) tag {
	return tag;
}

- (void) setTag:(int) newTag {
	tag = newTag;
}

- (NSDragOperation) draggingSourceOperationMaskForLocal:(BOOL) flag {
	return NSDragOperationMove;
}

- (void) draggedImage:(NSImage *)image endedAt:(NSPoint) screenPoint operation:(NSDragOperation) operation {
}

- (BOOL) ignoreModifierKeysWhileDragging {
	return YES;
}
@end

@implementation MVPreferencesMultipleIconView (MVPreferencesMultipleIconViewPrivate)
- (void) _focusFirst {
	focusedIndex = 0;
	[self setNeedsDisplayInRect:[self _boundsForIndex:focusedIndex]];
}

- (void) _focusLast {
	focusedIndex = [self _numberOfIcons] - 1;
	[self setNeedsDisplayInRect:[self _boundsForIndex:focusedIndex]];
}

- (unsigned int) _iconsWide {
	return NSWidth( [self bounds] ) / buttonSize.width;
}

- (unsigned int) _numberOfIcons {
	return [[self preferencePanes] count];
}

- (unsigned int) _numberOfRows {
	unsigned int row = 0, column = 0;
	if( ! [self _column:&column andRow:&row forIndex:[self _numberOfIcons] - 1] ) return 0;
	return row;
}

- (BOOL) _isIconSelectedAtIndex:(unsigned int) index {
	return [[self preferencePanes] objectAtIndex:index] == selectedPane;
}

- (BOOL) _column:(unsigned int *) column andRow:(unsigned int *) row forIndex:(unsigned int) index {
	if( index >= [self _numberOfIcons] ) return NO;

	*row = index / [self _iconsWide];
	*column = index % [self _iconsWide];

	return YES;
}

- (NSRect) _boundsForIndex:(unsigned int) index {
	unsigned int row = 0, column = 0, leftEdge = 0;

	if( ! [self _column:&column andRow:&row forIndex:index] ) return NSZeroRect;

	leftEdge = ( NSWidth( _bounds ) - ( [self _iconsWide] * buttonSize.width ) ) / 2. ;

	return NSMakeRect( ( column * buttonSize.width ) + leftEdge, row * buttonSize.height - (row * 6), buttonSize.width, buttonSize.height );
}

- (BOOL) _iconImage:(NSImage **) image andName:(NSString **) name forIndex:(unsigned int) index {
	NSString *unused;
	return [self _iconImage:image andName:name andIdentifier:&unused forIndex:index];
}

- (BOOL) _iconImage:(NSImage **) image andName:(NSString **) name andIdentifier:(NSString **) identifier forIndex:(unsigned int) index {
	NSDictionary *info = nil;
	NSBundle *pane = nil;

	if( index >= [self _numberOfIcons] ) return NO;

	pane = [[self preferencePanes] objectAtIndex:index];
	info = [pane infoDictionary];
	*image = [preferencesController _imageForPaneBundle:pane];
	*name = [preferencesController _paletteLabelForPaneBundle:pane];
	*identifier = [pane bundleIdentifier];

	return YES;
}

- (void) _drawIconAtIndex:(unsigned int) index drawRect:(NSRect) drawRect {
	NSImage *image;
	NSString *name;
	unsigned int row, column;
	NSPoint drawPoint;
	float nameHeight;
	NSRect buttonRect, destinationRect;
	NSDictionary *attributesDictionary;
	NSMutableParagraphStyle *paraStyle = [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
	CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];

	buttonRect = [self _boundsForIndex:index];
	if( ! NSIntersectsRect( buttonRect, drawRect ) ) return;
	if( ! [self _iconImage:&image andName:&name forIndex:index] ) return;
	if( ! [self _column:&column andRow:&row forIndex:index] ) return;

	// Draw dark gray rectangle around currently selected icon (for MultipleIconView)
	if( [self _isIconSelectedAtIndex:index] ) {
		[[NSColor colorWithCalibratedWhite:0.8 alpha:0.75] set];
		NSRectFillUsingOperation(buttonRect, NSCompositeSourceOver);
	}

	if( focusedIndex == index ) {
		CGContextSaveGState( context );
		NSSetFocusRingStyle( NSFocusRingAbove );
	}

	// Draw icon, dark if it is currently being pressed
	destinationRect = NSIntegralRect( NSMakeRect( NSMidX( buttonRect) - iconSize.width / 2., NSMaxY( buttonRect ) - iconBaseline - iconSize.height, iconSize.width, iconSize.height ) );
	destinationRect.size = iconSize;
	if( index != pressedIconIndex && image ) [image drawFlippedInRect:destinationRect operation:NSCompositeSourceOver fraction:1.];
	else if( image ) {
		NSImage *darkImage;
		NSSize darkImageSize;

		darkImage = [image copy];
		darkImageSize = [darkImage size];
		[darkImage lockFocus]; {
			[[NSColor blackColor] set];
			NSRectFillUsingOperation( NSMakeRect( 0., 0., darkImageSize.width, darkImageSize.height ), NSCompositeSourceIn );
			[darkImage unlockFocus];
		}

		[darkImage drawFlippedInRect:destinationRect operation:NSCompositeSourceOver fraction:1.];
		[image drawFlippedInRect:destinationRect operation:NSCompositeSourceOver fraction:.6666667];
		[darkImage release];
	}
	if( focusedIndex == index ) CGContextRestoreGState( context );

	// Draw text
	[paraStyle setAlignment:NSCenterTextAlignment];
	attributesDictionary = [NSDictionary dictionaryWithObjectsAndKeys:[NSFont toolTipsFontOfSize:11.], NSFontAttributeName, paraStyle, NSParagraphStyleAttributeName, nil];
	nameHeight = [[NSFont toolTipsFontOfSize:11.] defaultLineHeightForFont];
	drawPoint = NSMakePoint( rint( buttonSize.width + 2 * NSMinX( buttonRect ) - NSWidth( _bounds ) - 1 ), rint( NSMaxY( buttonRect ) - titleBaseline - nameHeight ) );
	[name drawAtPoint:drawPoint withAttributes:attributesDictionary];
}

- (void) _sizeToFit {
	if( ! [self preferencePanes] ) return;
	[self setFrameSize:NSMakeSize( NSWidth( _bounds ), NSMaxY( [self _boundsForIndex:[self _numberOfIcons] - 1] ) + bottomBorder ) ];
}

- (BOOL) _dragIconIndex:(unsigned int) index event:(NSEvent *) event {
	NSImage *iconImage;
	NSString *name;
	NSString *identifier;

	if( ! [self _iconImage:&iconImage andName:&name andIdentifier:&identifier forIndex:index] )
		return YES;

	return [self _dragIconImage:iconImage andName:name andIdentifier:identifier event:event];
}

- (BOOL) _dragIconImage:(NSImage *) iconImage andName:(NSString *) name event:(NSEvent *) event {
	return [self _dragIconImage:iconImage andName:name andIdentifier:name event:event];
}

- (BOOL) _dragIconImage:(NSImage *) iconImage andName:(NSString *) name andIdentifier:(NSString *) identifier event:(NSEvent *) event {
	NSImage *dragImage;
	NSPasteboard *pasteboard;
	NSPoint dragPoint, startPoint;
	NSMutableParagraphStyle *paraStyle = [[NSParagraphStyle defaultParagraphStyle] mutableCopy];

	dragImage = [[NSImage alloc] initWithSize:buttonSize];
	[dragImage lockFocus]; {
		float nameHeight;
		NSSize nameSize;
		NSDictionary *attributesDictionary;

		[iconImage drawInRect:NSMakeRect( buttonSize.width / 2. - iconSize.width / 2., iconBaseline, iconSize.width, iconSize.height ) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:.5];

		[paraStyle setAlignment:NSCenterTextAlignment];
		attributesDictionary = [NSDictionary dictionaryWithObjectsAndKeys:[NSFont toolTipsFontOfSize:11.], NSFontAttributeName, [[NSColor blackColor] colorWithAlphaComponent:.5], NSForegroundColorAttributeName, paraStyle, NSParagraphStyleAttributeName, nil];
		nameSize = [name sizeWithAttributes:attributesDictionary];
		nameHeight = [[NSFont toolTipsFontOfSize:11.] defaultLineHeightForFont];
		[name drawAtPoint:NSMakePoint( 0., nameHeight + titleBaseline - nameSize.height ) withAttributes:attributesDictionary];
		[dragImage unlockFocus];
	}

	pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
	[pasteboard declareTypes:[NSArray arrayWithObject:@"NSToolbarIndividualItemDragType"] owner:nil];
	[pasteboard setString:identifier forType:@"NSToolbarItemIdentiferPboardType"];

	dragPoint = [self convertPoint:[event locationInWindow] fromView:nil];
	startPoint = [self _boundsForIndex:[preferencePanes indexOfObject:[NSBundle bundleWithIdentifier:identifier]]].origin;
	startPoint.y += buttonSize.height;
	[self setNeedsDisplayInRect:[self _boundsForIndex:pressedIconIndex]];
	pressedIconIndex = NSNotFound;
	[self dragImage:dragImage at:startPoint offset:NSZeroSize event:event pasteboard:pasteboard source:self slideBack:YES];

	return YES;
}
@end