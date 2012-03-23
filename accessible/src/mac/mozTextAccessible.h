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
