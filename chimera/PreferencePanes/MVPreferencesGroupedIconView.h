#import <Foundation/NSBundle.h>
#import <Foundation/NSArray.h>
#import <AppKit/NSView.h>
#import <AppKit/NSNibDeclarations.h>

@class MVPreferencesController;

extern const unsigned int groupViewHeight, multiIconViewYOffset;

@interface MVPreferencesGroupedIconView : NSView {
	IBOutlet MVPreferencesController *preferencesController;
	NSBundle *selectedPane;
	NSArray *preferencePanes, *preferencePaneGroups;
	NSMutableArray *groupMultiIconViews;
}
- (void) setPreferencesController:(MVPreferencesController *) newPreferencesController;

- (void) setPreferencePanes:(NSArray *) newPreferencePanes;
- (NSArray *) preferencePanes;

- (void) setPreferencePaneGroups:(NSArray *) newPreferencePaneGroups;
- (NSArray *) preferencePaneGroups;
@end
