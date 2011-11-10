/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: Håkan Waara <hwaara@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
#import "mozAccessible.h"

// to get the mozView formal protocol, that all gecko's ChildViews implement.
#import "mozView.h"
#import "nsRoleMap.h"

#include "nsRect.h"
#include "nsCoord.h"
#include "nsObjCExceptions.h"

#include "nsIAccessible.h"
#include "nsIAccessibleText.h"
#include "nsIAccessibleEditableText.h"

#include "nsRootAccessible.h"

using namespace mozilla::a11y;

// These constants are only defined in OS X SDK 10.4, so we define them in order
// to be able to use for earlier OS versions.
const NSString *kInstanceDescriptionAttribute = @"AXDescription";       // NSAccessibilityDescriptionAttribute
const NSString *kTopLevelUIElementAttribute   = @"AXTopLevelUIElement"; // NSAccessibilityTopLevelUIElementAttribute
const NSString *kTextLinkSubrole              = @"AXTextLink";          // NSAccessibilitySecureTextFieldSubrole
const NSString *kURLAttribute                 = @"AXURL";

// converts a screen-global point in the cocoa coordinate system (with origo in the bottom-left corner
// of the screen), into a top-left screen point, that gecko can use.
static inline void
ConvertCocoaToGeckoPoint(NSPoint &aInPoint, nsPoint &aOutPoint)
{
  float mainScreenHeight = [(NSView*)[[NSScreen screens] objectAtIndex:0] frame].size.height;
  aOutPoint.MoveTo ((nscoord)aInPoint.x, (nscoord)(mainScreenHeight - aInPoint.y));
}

// all mozAccessibles are either abstract objects (that correspond to XUL widgets, HTML frames, etc) or are
// attached to a certain view; for example a document view. when we hand an object off to an AT, we always want
// to give it the represented view, in the latter case.
static inline id <mozAccessible>
GetObjectOrRepresentedView(id <mozAccessible> anObject)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ([anObject hasRepresentedView])
    return [anObject representedView];
  return anObject;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

// returns the passed in object if it is not ignored. if it's ignored, will return
// the first unignored ancestor.
static inline id
GetClosestInterestingAccessible(id anObject)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  // this object is not ignored, so let's return it.
  if (![anObject accessibilityIsIgnored])
    return GetObjectOrRepresentedView(anObject);
  
  // find the closest ancestor that is not ignored.
  id unignoredObject = anObject;
  while ((unignoredObject = [unignoredObject accessibilityAttributeValue:NSAccessibilityParentAttribute])) {
    if (![unignoredObject accessibilityIsIgnored])
      // object is not ignored, so let's stop the search.
      break;
  }
  
  // if it's a mozAccessible, we need to take care to maybe return the view we
  // represent, to the AT.
  if ([unignoredObject respondsToSelector:@selector(hasRepresentedView)])
    return GetObjectOrRepresentedView(unignoredObject);
  
  return unignoredObject;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

static inline mozAccessible* 
GetNativeFromGeckoAccessible(nsIAccessible *anAccessible)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSNULL;

  mozAccessible *native = nil;
  anAccessible->GetNativeInterface ((void**)&native);
  return native;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSNULL;
}

#pragma mark -
 
@implementation mozAccessible
 
- (id)initWithAccessible:(nsAccessibleWrap*)geckoAccessible
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ((self = [super init])) {
    mGeckoAccessible = geckoAccessible;
    mIsExpired = NO;
    mRole = geckoAccessible->Role();
    
    // Check for OS X "role skew"; the role constants in nsIAccessible.idl need to match the ones
    // in nsRoleMap.h.
    NS_ASSERTION([AXRoles[nsIAccessibleRole::ROLE_LAST_ENTRY] isEqualToString:@"ROLE_LAST_ENTRY"], "Role skew in the role map!");
  }
   
  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)dealloc
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mChildren release];
  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}
 
#pragma mark -

- (BOOL)accessibilityIsIgnored
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  // unknown (either unimplemented, or irrelevant) elements are marked as ignored
  // as well as expired elements.
  return mIsExpired || [[self role] isEqualToString:NSAccessibilityUnknownRole];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (NSArray*)accessibilityAttributeNames
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  // if we're expired, we don't support any attributes.
  if (mIsExpired)
    return [NSArray array];
  
  static NSArray *generalAttributes = nil;
  
  if (!generalAttributes) {
    // standard attributes that are shared and supported by all generic elements.
    generalAttributes = [[NSArray alloc] initWithObjects:  NSAccessibilityChildrenAttribute, 
                                                           NSAccessibilityParentAttribute,
                                                           NSAccessibilityRoleAttribute,
                                                           NSAccessibilityTitleAttribute,
                                                           NSAccessibilityValueAttribute,
                                                           NSAccessibilitySubroleAttribute,
                                                           NSAccessibilityRoleDescriptionAttribute,
                                                           NSAccessibilityPositionAttribute,
                                                           NSAccessibilityEnabledAttribute,
                                                           NSAccessibilitySizeAttribute,
                                                           NSAccessibilityWindowAttribute,
                                                           NSAccessibilityFocusedAttribute,
                                                           NSAccessibilityHelpAttribute,
                                                           NSAccessibilityTitleUIElementAttribute,
                                                           kTopLevelUIElementAttribute,
                                                           kInstanceDescriptionAttribute,
                                                           nil];
  }

  return generalAttributes;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString*)attribute
{  
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (mIsExpired)
    return nil;
  
  if ([attribute isEqualToString:NSAccessibilityChildrenAttribute])
    return [self children];
  if ([attribute isEqualToString:NSAccessibilityParentAttribute]) 
    return [self parent];
  
#ifdef DEBUG_hakan
  NSLog (@"(%@ responding to attr %@)", self, attribute);
#endif

  if ([attribute isEqualToString:NSAccessibilityRoleAttribute])
    return [self role];
  if ([attribute isEqualToString:NSAccessibilityPositionAttribute]) 
    return [self position];
  if ([attribute isEqualToString:NSAccessibilitySubroleAttribute])
    return [self subrole];
  if ([attribute isEqualToString:NSAccessibilityEnabledAttribute])
    return [NSNumber numberWithBool:[self isEnabled]];
  if ([attribute isEqualToString:NSAccessibilityValueAttribute])
    return [self value];
  if ([attribute isEqualToString:NSAccessibilityRoleDescriptionAttribute])
    return NSAccessibilityRoleDescription([self role], nil);
  if ([attribute isEqualToString: (NSString*) kInstanceDescriptionAttribute])
    return [self customDescription];
  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute])
    return [NSNumber numberWithBool:[self isFocused]];
  if ([attribute isEqualToString:NSAccessibilitySizeAttribute])
    return [self size];
  if ([attribute isEqualToString:NSAccessibilityWindowAttribute])
    return [self window];
  if ([attribute isEqualToString: (NSString*) kTopLevelUIElementAttribute])
    return [self window];
  if ([attribute isEqualToString:NSAccessibilityTitleAttribute] || 
      [attribute isEqualToString:NSAccessibilityTitleUIElementAttribute])
    return [self title];
  if ([attribute isEqualToString:NSAccessibilityHelpAttribute])
    return [self help];
    
#ifdef DEBUG
 NSLog (@"!!! %@ can't respond to attribute %@", self, attribute);
#endif
  return nil; // be nice and return empty string instead?

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute])
    return [self canBeFocused];
  
  return NO;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#ifdef DEBUG_hakan
  NSLog (@"[%@] %@='%@'", self, attribute, value);
#endif
  
  // we only support focusing elements so far.
  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute] && [value boolValue])
    [self focus];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (id)accessibilityHitTest:(NSPoint)point
{
  if (mIsExpired)
    return nil;

  // Convert from cocoa's coordinate system to gecko's. According to the docs
  // the point we're given is guaranteed to be bottom-left screen coordinates.
  nsPoint geckoPoint;
  ConvertCocoaToGeckoPoint (point, geckoPoint);

  nsCOMPtr<nsIAccessible> deepestFoundChild;
  mGeckoAccessible->GetDeepestChildAtPoint((PRInt32)geckoPoint.x,
                                           (PRInt32)geckoPoint.y,
                                           getter_AddRefs(deepestFoundChild));
  
  // if we found something, return its native accessible.
  if (deepestFoundChild) {
    mozAccessible *nativeChild = GetNativeFromGeckoAccessible(deepestFoundChild);
    if (nativeChild)
      return GetClosestInterestingAccessible(nativeChild);
  }
  
  // if we didn't find anything, return ourself (or the first unignored ancestor).
  return GetClosestInterestingAccessible(self); 
}

- (NSArray*)accessibilityActionNames 
{
  return nil;
}

- (NSString*)accessibilityActionDescription:(NSString*)action 
{
  return nil;
}

- (void)accessibilityPerformAction:(NSString*)action 
{
}

- (id)accessibilityFocusedUIElement
{
  if (mIsExpired)
    return nil;
  
  nsAccessible* focusedGeckoChild = mGeckoAccessible->FocusedChild();
  if (focusedGeckoChild) {
    mozAccessible *focusedChild = GetNativeFromGeckoAccessible(focusedGeckoChild);
    if (focusedChild)
      return GetClosestInterestingAccessible(focusedChild);
  }
  
  // return ourself if we can't get a native focused child.
  return GetClosestInterestingAccessible(self);
}

#pragma mark -

- (id <mozAccessible>)parent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsCOMPtr<nsIAccessible> accessibleParent(mGeckoAccessible->GetUnignoredParent());
  if (accessibleParent) {
    id nativeParent = GetNativeFromGeckoAccessible(accessibleParent);
    if (nativeParent)
      return GetClosestInterestingAccessible(nativeParent);
  }
  
  // GetUnignoredParent() returns null when there is no unignored accessible all the way up to
  // the root accessible. so we'll have to return whatever native accessible is above our root accessible 
  // (which might be the owning NSWindow in the application, for example).
  //
  // get the native root accessible, and tell it to return its first parent unignored accessible.
  nsRootAccessible* root = mGeckoAccessible->RootAccessible();
  id nativeParent = GetNativeFromGeckoAccessible(static_cast<nsIAccessible*>(root));
  NSAssert1 (nativeParent, @"!!! we can't find a parent for %@", self);
  
  return GetClosestInterestingAccessible(nativeParent);

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)hasRepresentedView
{
  return NO;
}

- (id)representedView
{
  return nil;
}

- (BOOL)isRoot
{
  return NO;
}

// gets our native children lazily.
// returns nil when there are no children.
- (NSArray*)children
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (mChildren)
    return mChildren;
    
  mChildren = [[NSMutableArray alloc] init];
  
  // get the array of children.
  nsTArray<nsRefPtr<nsAccessibleWrap> > childrenArray;
  mGeckoAccessible->GetUnignoredChildren(childrenArray);
  
  // now iterate through the children array, and get each native accessible.
  int totalCount = childrenArray.Length();
  int index = 0;
   
  for (; index < totalCount; index++) {
    nsAccessibleWrap *curAccessible = childrenArray.ElementAt(index);
    if (curAccessible) {
      mozAccessible *curNative = GetNativeFromGeckoAccessible(curAccessible);
      if (curNative)
        [mChildren addObject:GetObjectOrRepresentedView(curNative)];
    }
  }
  
#ifdef DEBUG_hakan
  // make sure we're not returning any ignored accessibles.
  NSEnumerator *e = [mChildren objectEnumerator];
  mozAccessible *m = nil;
  while ((m = [e nextObject])) {
    NSAssert1(![m accessibilityIsIgnored], @"we should never return an ignored accessible! (%@)", m);
  }
#endif
  
  return mChildren;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSValue*)position
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  PRInt32 x, y, width, height;
  mGeckoAccessible->GetBounds (&x, &y, &width, &height);
  NSPoint p = NSMakePoint (x, y);
  
  // The coords we get from Gecko are top-left screen coordinates.
  // Cocoa wants us to return bottom-left screen coordinates.
  // This involves two steps:
  // 1. Put the rect in the bottom-left coord space
  // 2. Subtract the height of the rect's Y-coordinate, to make the
  //    the rect's origin (0, 0) be in the bottom-left corner.
  
  float mainScreenHeight = [[[NSScreen screens] objectAtIndex:0] frame].size.height;
  p.y = mainScreenHeight - p.y - height;
  
  return [NSValue valueWithPoint:p];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSValue*)size
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  PRInt32 x, y, width, height;
  mGeckoAccessible->GetBounds (&x, &y, &width, &height);  
  return [NSValue valueWithSize:NSMakeSize (width, height)];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSString*)role
{
#ifdef DEBUG_A11Y
  NS_ASSERTION(nsAccUtils::IsTextInterfaceSupportCorrect(mGeckoAccessible),
               "Does not support nsIAccessibleText when it should");
#endif
  return (NSString*) AXRoles[mRole];
}

- (NSString*)subrole
{
  return nil;
}

- (NSString*)title
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsAutoString title;
  mGeckoAccessible->GetName (title);
  return title.IsEmpty() ? nil : [NSString stringWithCharacters:title.BeginReading() length:title.Length()];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)value
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsAutoString value;
  mGeckoAccessible->GetValue (value);
  return value.IsEmpty() ? nil : [NSString stringWithCharacters:value.BeginReading() length:value.Length()];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)valueDidChange
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#ifdef DEBUG_hakan
  NSLog(@"%@'s value changed!", self);
#endif
  // sending out a notification is expensive, so we don't do it other than for really important objects,
  // like mozTextAccessible.

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (NSString*)customDescription
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (mGeckoAccessible->IsDefunct())
    return nil;

  nsAutoString desc;
  mGeckoAccessible->Description(desc);
  return desc.IsEmpty() ? nil : [NSString stringWithCharacters:desc.BeginReading() length:desc.Length()];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSString*)help
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsAutoString helpText;
  mGeckoAccessible->GetHelp (helpText);
  return helpText.IsEmpty() ? nil : [NSString stringWithCharacters:helpText.BeginReading() length:helpText.Length()];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

// objc-style description (from NSObject); not to be confused with the accessible description above.
- (NSString*)description
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [NSString stringWithFormat:@"(%p) %@", self, [self role]];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)isFocused
{
  return (mGeckoAccessible->State() & states::FOCUSED) != 0;
}

- (BOOL)canBeFocused
{
  return mGeckoAccessible->State() & states::FOCUSABLE;
}

- (BOOL)focus
{
  nsresult rv = mGeckoAccessible->TakeFocus();
  return NS_SUCCEEDED(rv);
}

- (BOOL)isEnabled
{
  return (mGeckoAccessible->State() & states::UNAVAILABLE) == 0;
}

// The root accessible calls this when the focused node was
// changed to us.
- (void)didReceiveFocus
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#ifdef DEBUG_hakan
  NSLog (@"%@ received focus!", self);
#endif
  NSAssert1(![self accessibilityIsIgnored], @"trying to set focus to ignored element! (%@)", self);
  NSAccessibilityPostNotification(GetObjectOrRepresentedView(self),
                                  NSAccessibilityFocusedUIElementChangedNotification);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (NSWindow*)window
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsAccessibleWrap *accWrap = static_cast<nsAccessibleWrap*>(mGeckoAccessible);

  // Get a pointer to the native window (NSWindow) we reside in.
  NSWindow *nativeWindow = nil;
  nsDocAccessible* docAcc = accWrap->GetDocAccessible();
  if (docAcc)
    nativeWindow = static_cast<NSWindow*>(docAcc->GetNativeWindow());

  NSAssert1(nativeWindow, @"Could not get native window for %@", self);
  return nativeWindow;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)invalidateChildren
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // make room for new children
  [mChildren release];
  mChildren = nil;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)expire
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [self invalidateChildren];
  mIsExpired = YES;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (BOOL)isExpired
{
  return mIsExpired;
}

#pragma mark -
#pragma mark Debug methods
#pragma mark -

#ifdef DEBUG

// will check that our children actually reference us as their
// parent.
- (void)sanityCheckChildren:(NSArray *)children
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSAssert(![self accessibilityIsIgnored], @"can't sanity check children of an ignored accessible!");
  NSEnumerator *iter = [children objectEnumerator];
  mozAccessible *curObj = nil;
  
  NSLog(@"sanity checking %@", self);
  
  while ((curObj = [iter nextObject])) {
    id realSelf = GetObjectOrRepresentedView(self);
    NSLog(@"checking %@", realSelf);
    NSAssert2([curObj parent] == realSelf, 
              @"!!! %@ not returning %@ as AXParent, even though it is a AXChild of it!", curObj, realSelf);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)sanityCheckChildren
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [self sanityCheckChildren:[self children]];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)printHierarchy
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [self printHierarchyWithLevel:0];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)printHierarchyWithLevel:(unsigned)level
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSAssert(![self isExpired], @"!!! trying to print hierarchy of expired object!");
  
  // print this node
  NSMutableString *indent = [NSMutableString stringWithCapacity:level];
  unsigned i=0;
  for (;i<level;i++)
    [indent appendString:@" "];
  
  NSLog (@"%@(#%i) %@", indent, level, self);
  
  // use |children| method to make sure our children are lazily fetched first.
  NSArray *children = [self children];
  if (!children)
    return;
    
  if (![self accessibilityIsIgnored])
    [self sanityCheckChildren];
    
  NSEnumerator *iter = [children objectEnumerator];
  mozAccessible *object = nil;
  
  while (iter && (object = [iter nextObject]))
    // print every child node's subtree, increasing the indenting
    // by two for every level.
    [object printHierarchyWithLevel:(level+1)];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

#endif /* DEBUG */

@end
