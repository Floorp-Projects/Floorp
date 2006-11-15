#import <Cocoa/Cocoa.h>
#import "mozAccessible.h"

/* Simple subclasses for things like checkboxes, buttons, etc. */

@interface mozButtonAccessible : mozAccessible
- (void)click;
@end

@interface mozCheckboxAccessible : mozButtonAccessible
- (BOOL)isChecked;
@end
