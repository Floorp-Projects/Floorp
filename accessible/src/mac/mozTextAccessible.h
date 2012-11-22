/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozAccessible.h"

#import "HyperTextAccessible.h"

@interface mozTextAccessible : mozAccessible
{
  // both of these are the same old mGeckoAccessible, but already
  // QI'd for us, to the right type, for convenience.
  mozilla::a11y::HyperTextAccessible* mGeckoTextAccessible; // strong
  nsIAccessibleEditableText *mGeckoEditableTextAccessible; // strong
}
@end

@interface mozTextLeafAccessible : mozAccessible
{
}
@end
