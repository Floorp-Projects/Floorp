#import <AppKit/NSToolbar.h>

@interface NSToolbar (CHToolbarCustomizableAdditions)
- (BOOL) alwaysCustomizableByDrag;
- (void) setAlwaysCustomizableByDrag:(BOOL) flag;

- (BOOL) showsContextMenu;
- (void) setShowsContextMenu:(BOOL) flag;

- (unsigned int) indexOfFirstMovableItem;
- (void) setIndexOfFirstMovableItem:(unsigned int) anIndex;
@end
