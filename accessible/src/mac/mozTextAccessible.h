#import "mozAccessible.h"

#import "nsHyperTextAccessible.h"

@interface mozTextAccessible : mozAccessible
{
  // both of these are the same old mGeckoAccessible, but already
  // QI'd for us, to the right type, for convenience.
  nsHyperTextAccessible     *mGeckoTextAccessible;         // strong
  nsIAccessibleEditableText *mGeckoEditableTextAccessible; // strong
}
@end

/* A combobox (in the mac world) is a textfield with an associated menu, for example
   the location bar. */
@interface mozComboboxAccessible : mozTextAccessible
// equivalent to pressing return key in this textfield.
- (void)confirm;

// shows the menu for this combobox.
- (void)showMenu;
@end
