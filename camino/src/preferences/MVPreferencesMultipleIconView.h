#import <Foundation/NSBundle.h>
#import <AppKit/NSView.h>
#import <AppKit/NSNibDeclarations.h>

@class MVPreferencesController;

extern const NSSize buttonSize, iconSize;
extern const unsigned int titleBaseline, iconBaseline, bottomBorder;

@interface MVPreferencesMultipleIconView : NSView
{
	IBOutlet MVPreferencesController *preferencesController;
	unsigned int pressedIconIndex, focusedIndex;
	int tag;
	NSBundle *selectedPane;
	NSArray *preferencePanes;
}
- (void) setPreferencesController:(MVPreferencesController *) newPreferencesController;

- (void) setPreferencePanes:(NSArray *) newPreferencePanes;
- (NSArray *) preferencePanes;

- (void) setSelectedPane:(NSBundle *) newSelectedClientRecord;

- (int) tag;
- (void) setTag:(int) newTag;
@end