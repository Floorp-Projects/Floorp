#import "mozAccessible.h"

#import "nsIAccessibleText.h"
#import "nsIAccessibleEditableText.h"

@interface mozTextAccessible : mozAccessible
{
  // both of these are the same old mGeckoAccessible, but already
  // QI'd for us, to the right type, for convenience.
  nsIAccessibleText         *mGeckoTextAccessible;         // strong
  nsIAccessibleEditableText *mGeckoEditableTextAccessible; // strong
}
@end
