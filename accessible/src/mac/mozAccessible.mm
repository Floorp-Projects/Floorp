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

#include "nsIAccessible.h"

// These constants are only defined in OS X SDK 10.4, so we define them in order
// to be able to use for earlier OS versions.
const NSString *kInstanceDescriptionAttribute = @"AXDescription";     // NSAccessibilityDescriptionAttribute
const NSString *kTopLevelUIElementAttribute = @"AXTopLevelUIElement"; // NSAccessibilityTopLevelUIElementAttribute

// converts a screen-global point in the cocoa coordinate system (with origo in the bottom-left corner
// of the screen), into a top-left screen point, that gecko can use.
static inline void
ConvertCocoaToGeckoPoint(NSPoint &aInPoint, nsPoint &aOutPoint)
{
  float mainScreenHeight = [(NSView*)[[NSScreen screens] objectAtIndex:0] frame].size.height;
  aOutPoint.MoveTo ((nscoord)aInPoint.x, (nscoord)(mainScreenHeight - aInPoint.y));
}

// returns an object if it is not ignored. if it's ignored, will return
// the first unignored ancestor.
static inline id
ObjectOrUnignoredAncestor(id anObject)
{
  if ([anObject accessibilityIsIgnored])
    return NSAccessibilityUnignoredAncestor(anObject);
  return anObject;
}


@interface mozAccessible (Private)
- (mozAccessible*)getNativeFromGeckoAccessible:(nsIAccessible*)accessible;
#ifdef DEBUG
- (void)sanityCheckChildren:(NSArray*)theChildren;
#endif
@end

#pragma mark -
 
@implementation mozAccessible
 
- (id)initWithAccessible:(nsAccessible*)accessible
{
  if ((self = [super init])) {
    mGeckoAccessible = accessible;
    mIsExpired = NO;
    
    // Check for OS X "role skew"; the role constants in nsIAccessible.idl need to match the ones
    // in nsRoleMap.h.
    NS_ASSERTION([AXRoles[nsIAccessible::ROLE_LAST_ENTRY] isEqualToString:@"ROLE_LAST_ENTRY"], "Role skew in the role map!");
  }
   
  return self;
}

- (void)dealloc
{
  // post a notification that we've gone away, if we're not ignored.
  // XXX: is this expensive perf-wise?
  if (![self accessibilityIsIgnored]) {
    NSAccessibilityPostNotification([self ourself],
                                    NSAccessibilityUIElementDestroyedNotification);
  }
  
  [mChildren release];
  [super dealloc];
}
 
#pragma mark -

- (BOOL)accessibilityIsIgnored
{
  // unknown (either unimplemented, or irrelevant) elements are marked as ignored
  // as well as expired elements.
  return mIsExpired || [[self role] isEqualToString:NSAccessibilityUnknownRole];
}

- (NSArray*)accessibilityAttributeNames
{
  // if we're expired, we don't support any attributes.
  if (mIsExpired)
    return [NSArray array];
  
  static NSArray *supportedAttributes = nil;
  if (!supportedAttributes) {
    // standard attributes that are shared and supported by all generic elements.
    supportedAttributes = [[NSArray alloc] initWithObjects:NSAccessibilityChildrenAttribute, 
                                                           NSAccessibilityParentAttribute,
                                                           NSAccessibilityRoleAttribute,
                                                           NSAccessibilityTitleAttribute,
                                                           NSAccessibilityValueAttribute,
                                                           NSAccessibilitySubroleAttribute,
                                                           NSAccessibilityRoleDescriptionAttribute,
                                                           NSAccessibilityPositionAttribute,
                                                           NSAccessibilitySizeAttribute,
                                                           NSAccessibilityWindowAttribute,
                                                           NSAccessibilityFocusedAttribute,
                                                           kTopLevelUIElementAttribute,
                                                           // kInstanceDescriptionAttribute, // XXX: not implemented yet
                                                           nil];
  }

  return supportedAttributes;
}

- (id)accessibilityAttributeValue:(NSString*)attribute
{  
  if (mIsExpired)
    return nil;
  
  if ([attribute isEqualToString:NSAccessibilityChildrenAttribute])
    return NSAccessibilityUnignoredChildren ([self children]);
  if ([attribute isEqualToString:NSAccessibilityParentAttribute]) 
    return ObjectOrUnignoredAncestor ([self parent]);
  
#ifdef DEBUG_hakan
  NSLog (@"(%@ responding to attr %@)", self, attribute);
#endif
  
  if ([attribute isEqualToString:NSAccessibilityPositionAttribute]) 
    return [self position];
  if ([attribute isEqualToString:NSAccessibilityRoleAttribute])
    return [self role];
  if ([attribute isEqualToString:NSAccessibilitySubroleAttribute])
    return nil; // default to no subrole
  if ([attribute isEqualToString:NSAccessibilityValueAttribute])
    return [self value];
  if ([attribute isEqualToString:NSAccessibilityRoleDescriptionAttribute])
    return NSAccessibilityRoleDescription([self role], nil);
  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute])
    return [NSNumber numberWithBool:[self isFocused]];
  if ([attribute isEqualToString:NSAccessibilitySizeAttribute])
    return [self size];
  if ([attribute isEqualToString:NSAccessibilityWindowAttribute])
    return [self window];
  if ([attribute isEqualToString:kTopLevelUIElementAttribute])
    // most apps seem to return the window as the top level ui element attr.
    return [self window];
  if ([attribute isEqualToString:NSAccessibilityTitleAttribute])
    return [self title];

#ifdef DEBUG
 NSLog (@"!!! %@ can't respond to attribute %@", self, attribute);
#endif
 return nil; // be nice and return empty string instead?
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute
{
  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute])
    return [self canBeFocused];
  
  return NO;
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute
{
  // we only support focusing elements so far.
  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute] && [value boolValue])
    [self focus];
}

- (id)accessibilityHitTest:(NSPoint)point
{
  if (mIsExpired)
    return nil;
  
  // Convert from cocoa's coordinate system to gecko's. According to the docs
  // the point we're given is guaranteed to be bottom-left screen coordinates.
  nsPoint geckoPoint;
  ConvertCocoaToGeckoPoint (point, geckoPoint);
  
  // see if we can find an accessible at that point.
  nsCOMPtr<nsIAccessible> foundChild;
  mGeckoAccessible->GetChildAtPoint ((PRInt32)geckoPoint.x, (PRInt32)geckoPoint.y, getter_AddRefs (foundChild));
  
  if (foundChild) {
    // if we found something, return its native accessible.
    mozAccessible *nativeChild = [self getNativeFromGeckoAccessible:foundChild];
    if (nativeChild)
      return ObjectOrUnignoredAncestor (nativeChild);
  }
  
  // if we didn't find anything, return ourself (or the first unignored ancestor).
  return ObjectOrUnignoredAncestor ([self ourself]); 
}

- (NSArray*)accessibilityActionNames 
{
  return [NSArray array];
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
  
  nsCOMPtr<nsIAccessible> focusedGeckoChild;
  mGeckoAccessible->GetFocusedChild (getter_AddRefs (focusedGeckoChild));
  
  if (focusedGeckoChild) {
    mozAccessible *focusedChild = [self getNativeFromGeckoAccessible:focusedGeckoChild];
    if (focusedChild)
      return ObjectOrUnignoredAncestor(focusedChild);
  }
  
  // return ourself if we can't get a native focused child.
  return ObjectOrUnignoredAncestor([self ourself]);
}

#pragma mark -

- (id <mozAccessible>)parent
{
  nsCOMPtr<nsIAccessible> accessibleParent;
  nsresult rv = mGeckoAccessible->GetParent (getter_AddRefs (accessibleParent));
  if (accessibleParent && NS_SUCCEEDED (rv)) {
    mozAccessible *nativeParent = [self getNativeFromGeckoAccessible:accessibleParent];
    if (nativeParent)
      return nativeParent;
  }
  
#ifdef DEBUG
  NSLog (@"!!! we can't find a parent for %@", self);
#endif
  // we didn't find a parent
  return nil;
}

- (id <mozAccessible>)ourself
{
  return self;
}

- (BOOL)isRoot
{
  return NO;
}

// gets our native children lazily.
// returns nil when there are no children.
- (NSArray*)children
{
  // do we want to expire all children, when something is inserted
  // into a children array instead? then we could avoid this (quite expensive)
  // check.
  PRInt32 count=0;
  mGeckoAccessible->GetChildCount (&count);
  
  // short-circuit here. if we already have a children
  // array, and the count hasn't changed, just return our
  // cached version.
  if (mChildren && ([mChildren count] == (unsigned)count))
    return mChildren;
  
  if (!mChildren)
    mChildren = [[NSMutableArray alloc] initWithCapacity:count];
  
  // get the array of children.
  nsCOMPtr<nsIArray> childrenArray;
  mGeckoAccessible->GetChildren(getter_AddRefs(childrenArray));
  
  if (childrenArray) {
    // get an enumerator for this array.
    nsCOMPtr<nsISimpleEnumerator> enumerator;
    childrenArray->Enumerate(getter_AddRefs(enumerator));
    
    nsCOMPtr<nsISupports> curChild;
    nsCOMPtr<nsIAccessible> accessible;
    mozAccessible *curNative = nil;
    PRBool hasMore = PR_FALSE;
    
    // now iterate through the children array, and get each native accessible.
    while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMore)) && hasMore && 
           NS_SUCCEEDED(enumerator->GetNext(getter_AddRefs(curChild))) && curChild) {
      accessible = do_QueryInterface (curChild);
      if (accessible) {
        curNative = [self getNativeFromGeckoAccessible:accessible];
        if (curNative)
          [mChildren addObject:curNative];
      }
    }
  }
  
  return mChildren;
}

- (NSValue*)position
{
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
}

- (NSValue*)size
{
  PRInt32 x, y, width, height;
  mGeckoAccessible->GetBounds (&x, &y, &width, &height);  
  return [NSValue valueWithSize:NSMakeSize (width, height)];
}

- (NSString*)role
{
#ifdef DEBUG_A11Y
  NS_ASSERTION(nsAccessible::IsTextInterfaceSupportCorrect(mGeckoAccessible), "Does not support nsIAccessibleText when it should");
#endif
  PRUint32 roleInt = 0;
  mGeckoAccessible->GetFinalRole (&roleInt);
  return AXRoles[roleInt]; // see nsRoleMap.h
}

- (NSString*)title
{
  nsAutoString title;
  mGeckoAccessible->GetName (title);
  return [NSString stringWithCharacters:title.BeginReading() length:title.Length()];
}

- (NSString*)value
{
  nsAutoString value;
  mGeckoAccessible->GetValue (value);
  return [NSString stringWithCharacters:value.BeginReading() length:value.Length()];
}

- (NSString*)customDescription
{
  nsAutoString desc;
  mGeckoAccessible->GetDescription (desc);
  return [NSString stringWithCharacters:desc.BeginReading() length:desc.Length()];
}

// objc-style description (from NSObject); not to be confused with the accessible description above.
- (NSString*)description
{
  return [NSString stringWithFormat:@"(%p) %@", self, [self role], nil];
}

- (BOOL)isFocused
{
  PRUint32 state = 0;
  mGeckoAccessible->GetFinalState (&state);
  return (state & nsIAccessible::STATE_FOCUSED) != 0; 
}

- (BOOL)canBeFocused
{
  PRUint32 state = 0;
  mGeckoAccessible->GetFinalState (&state);
  return (state & nsIAccessible::STATE_FOCUSABLE) != 0;
}

- (BOOL)focus
{
  nsresult rv = mGeckoAccessible->TakeFocus();
  return NS_SUCCEEDED(rv);
}

// The root accessible calls this when the focused node was
// changed to us.
- (void)didReceiveFocus
{
  #ifdef DEBUG_hakan
    NSLog (@"%@ received focus!", self);
  #endif
  NSAccessibilityPostNotification([self ourself],
                                  NSAccessibilityFocusedUIElementChangedNotification);
}

- (NSWindow*)window
{
  nsAccessibleWrap *accWrap = NS_STATIC_CAST (nsAccessibleWrap*, mGeckoAccessible);
  NSWindow *nativeWindow = nil;
  accWrap->GetNativeWindow ((void**)&nativeWindow);
  
#ifdef DEBUG
  if (!nativeWindow)
    NSLog (@"Could not get native window for %@", self);
#endif
  
  return nativeWindow;
}

- (mozAccessible*)getNativeFromGeckoAccessible:(nsIAccessible*)accessible
{
  mozAccessible *native = nil;
  accessible->GetNativeInterface ((void**)&native);
  return native ? (mozAccessible*)[native ourself] : nil;
}

- (void)invalidateChildren
{
  [mChildren removeAllObjects];
}

- (void)expire
{
  [self invalidateChildren];
  mIsExpired = YES;
}

#pragma mark -
#pragma mark Debug methods
#pragma mark -

#ifdef DEBUG

// will check that our children actually reference us as their
// parent.
- (void)sanityCheckChildren:(NSArray*)theChildren
{
  id parentObject = nil;
  
  // if we're the root accessible, our children's AXParent
  // should reference the native view. see mozDocAccessible.h
  if ([self isRoot])
    parentObject = [self ourself];
  else
    parentObject = self;
  
  NSEnumerator *iter = [theChildren objectEnumerator];
  mozAccessible *curObj = nil;
  
  while ((curObj = [iter nextObject])) {
    if ([curObj parent] != parentObject)
      NSLog (@"!!! %@ not returning %@ as AXParent, even though it is a AXChild of it!", curObj, parentObject);
  }
}

- (void)printHierarchy
{
  [self printHierarchyWithLevel:0];
}

- (void)printHierarchyWithLevel:(unsigned)level
{
  // print this node
  NSMutableString *indent = [NSMutableString stringWithCapacity:level];
  unsigned i=0;
  for (;i<level;i++)
    [indent appendString:@" "];
  
  NSLog (@"%@(#%i) %@", indent, level, self);
  
  // use |children| method to make sure our children are lazily
  // fetched first.
  NSEnumerator *iter = [[self children] objectEnumerator];
  mozAccessible *object = nil;
  
  while (iter && (object = [iter nextObject]))
    // print every child node's subtree, increasing the indenting
    // by two for every level.
    [object printHierarchyWithLevel:(level+1)];
}

#endif /* DEBUG */

@end
