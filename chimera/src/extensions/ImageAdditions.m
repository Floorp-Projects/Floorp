#import "ImageAdditions.h"
#import <Cocoa/Cocoa.h>

@implementation NSImage (ImageAdditions)
- (void) drawFlippedInRect:(NSRect) rect operation:(NSCompositingOperation) op fraction:(float) delta {
	CGContextRef context;

	context = [[NSGraphicsContext currentContext] graphicsPort];
	CGContextSaveGState( context ); {
		CGContextTranslateCTM( context, 0, NSMaxY( rect ) );
		CGContextScaleCTM( context, 1, -1 );

		rect.origin.y = 0;
		[self drawInRect:rect fromRect:NSZeroRect operation:op fraction:delta];
	} CGContextRestoreGState( context );
}

- (void) drawFlippedInRect:(NSRect) rect operation:(NSCompositingOperation) op {
    [self drawFlippedInRect:rect operation:op fraction:1.0];
}

- (void)applyBadge:(NSImage*)badge withAlpha:(float)alpha scale:(float)scale
{
    if (!badge)
        return;
    
    [badge setScalesWhenResized:YES];
    [badge setSize:NSMakeSize([self size].width * scale,[self size].height * scale)];
    
    [self lockFocus];
    [[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationHigh];
    [badge dissolveToPoint:NSMakePoint([self size].width - [badge size].width,0) fraction:alpha];
    [self unlockFocus];
}

@end
