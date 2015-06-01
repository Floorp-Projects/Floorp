/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
#import "mozAccessible.h"

#import "MacUtils.h"
#import "mozView.h"

#include "Accessible-inl.h"
#include "nsAccUtils.h"
#include "nsIAccessibleRelation.h"
#include "nsIAccessibleEditableText.h"
#include "nsIPersistentProperties2.h"
#include "Relation.h"
#include "Role.h"
#include "RootAccessible.h"

#include "mozilla/Services.h"
#include "nsRect.h"
#include "nsCocoaUtils.h"
#include "nsCoord.h"
#include "nsObjCExceptions.h"
#include "nsWhitespaceTokenizer.h"

using namespace mozilla;
using namespace mozilla::a11y;

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

#pragma mark -

@implementation mozAccessible
 
- (id)initWithAccessible:(uintptr_t)aGeckoAccessible
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ((self = [super init])) {
    mGeckoAccessible = aGeckoAccessible;
    if (aGeckoAccessible & IS_PROXY)
      mRole = [self getProxyAccessible]->Role();
    else
      mRole = [self getGeckoAccessible]->Role();
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

- (mozilla::a11y::AccessibleWrap*)getGeckoAccessible
{
  // Check if mGeckoAccessible points at a proxy
  if (mGeckoAccessible & IS_PROXY)
    return nil;

  return reinterpret_cast<AccessibleWrap*>(mGeckoAccessible);
}

- (mozilla::a11y::ProxyAccessible*)getProxyAccessible
{
  // Check if mGeckoAccessible points at a proxy
  if (!(mGeckoAccessible & IS_PROXY))
    return nil;

  return reinterpret_cast<ProxyAccessible*>(mGeckoAccessible & ~IS_PROXY);
}
 
#pragma mark -

- (BOOL)accessibilityIsIgnored
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  // unknown (either unimplemented, or irrelevant) elements are marked as ignored
  // as well as expired elements.

  AccessibleWrap* accWrap = [self getGeckoAccessible];
  return !accWrap || ([[self role] isEqualToString:NSAccessibilityUnknownRole] &&
                               !(accWrap->InteractiveState() & states::FOCUSABLE));

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (NSArray*)accessibilityAttributeNames
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  // if we're expired, we don't support any attributes.
  if (![self getGeckoAccessible])
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
                                                           NSAccessibilityTopLevelUIElementAttribute,
                                                           NSAccessibilityDescriptionAttribute,
#if DEBUG
                                                           @"AXMozDescription",
#endif
                                                           nil];
  }

  return generalAttributes;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString*)attribute
{  
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (![self getGeckoAccessible])
    return nil;

#if DEBUG
  if ([attribute isEqualToString:@"AXMozDescription"])
    return [NSString stringWithFormat:@"role = %u native = %@", mRole, [self class]];
#endif
  
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
    return [self roleDescription];  
  if ([attribute isEqualToString:NSAccessibilityDescriptionAttribute])
    return [self customDescription];
  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute])
    return [NSNumber numberWithBool:[self isFocused]];
  if ([attribute isEqualToString:NSAccessibilitySizeAttribute])
    return [self size];
  if ([attribute isEqualToString:NSAccessibilityWindowAttribute])
    return [self window];
  if ([attribute isEqualToString:NSAccessibilityTopLevelUIElementAttribute])
    return [self window];
  if ([attribute isEqualToString:NSAccessibilityTitleAttribute])
    return [self title];
  if ([attribute isEqualToString:NSAccessibilityTitleUIElementAttribute]) {
    Relation rel =
      [self getGeckoAccessible]->RelationByType(RelationType::LABELLED_BY);
    Accessible* tempAcc = rel.Next();
    return tempAcc ? GetNativeFromGeckoAccessible(tempAcc) : nil;
  }
  if ([attribute isEqualToString:NSAccessibilityHelpAttribute])
    return [self help];
    
#ifdef DEBUG
 NSLog (@"!!! %@ can't respond to attribute %@", self, attribute);
#endif
  return nil;

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
  AccessibleWrap* accWrap = [self getGeckoAccessible];
  if (!accWrap)
    return nil;

  // Convert the given screen-global point in the cocoa coordinate system (with
  // origin in the bottom-left corner of the screen) into point in the Gecko
  // coordinate system (with origin in a top-left screen point).
  NSScreen* mainView = [[NSScreen screens] objectAtIndex:0];
  NSPoint tmpPoint = NSMakePoint(point.x,
                                 [mainView frame].size.height - point.y);
  nsIntPoint geckoPoint = nsCocoaUtils::
    CocoaPointsToDevPixels(tmpPoint, nsCocoaUtils::GetBackingScaleFactor(mainView));

  Accessible* child =
    accWrap->ChildAtPoint(geckoPoint.x, geckoPoint.y,
                          Accessible::eDeepestChild);

  if (child) {
    mozAccessible* nativeChild = GetNativeFromGeckoAccessible(child);
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
  // by default we return whatever the MacOS API know about.
  // if you have custom actions, override.
  return NSAccessibilityActionDescription(action);
}

- (void)accessibilityPerformAction:(NSString*)action 
{
}

- (id)accessibilityFocusedUIElement
{
  AccessibleWrap* accWrap = [self getGeckoAccessible];
  if (!accWrap)
    return nil;
  
  Accessible* focusedGeckoChild = accWrap->FocusedChild();
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

  AccessibleWrap* accWrap = [self getGeckoAccessible];
  Accessible* accessibleParent = accWrap->GetUnignoredParent();
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
  id nativeParent =
    GetNativeFromGeckoAccessible(accWrap->RootAccessible());
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

  AccessibleWrap* accWrap = [self getGeckoAccessible];
  if (mChildren || !accWrap->AreChildrenCached())
    return mChildren;

  mChildren = [[NSMutableArray alloc] init];

  // get the array of children.
  nsAutoTArray<Accessible*, 10> childrenArray;
  accWrap->GetUnignoredChildren(&childrenArray);

  // now iterate through the children array, and get each native accessible.
  uint32_t totalCount = childrenArray.Length();
  for (uint32_t idx = 0; idx < totalCount; idx++) {
    Accessible* curAccessible = childrenArray.ElementAt(idx);
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

  AccessibleWrap* accWrap = [self getGeckoAccessible];
  if (!accWrap)
    return nil;

  nsIntRect rect = accWrap->Bounds();

  NSScreen* mainView = [[NSScreen screens] objectAtIndex:0];
  CGFloat scaleFactor = nsCocoaUtils::GetBackingScaleFactor(mainView);
  NSPoint p = NSMakePoint(static_cast<CGFloat>(rect.x) / scaleFactor,
                         [mainView frame].size.height - static_cast<CGFloat>(rect.y + rect.height) / scaleFactor);

  return [NSValue valueWithPoint:p];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSValue*)size
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  AccessibleWrap* accWrap = [self getGeckoAccessible];
  if (!accWrap)
    return nil;

  nsIntRect rect = accWrap->Bounds();
  CGFloat scaleFactor =
    nsCocoaUtils::GetBackingScaleFactor([[NSScreen screens] objectAtIndex:0]);
  return [NSValue valueWithSize:NSMakeSize(static_cast<CGFloat>(rect.width) / scaleFactor,
                                           static_cast<CGFloat>(rect.height) / scaleFactor)];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSString*)role
{
  AccessibleWrap* accWrap = [self getGeckoAccessible];
  if (!accWrap)
    return nil;

#ifdef DEBUG_A11Y
  NS_ASSERTION(nsAccUtils::IsTextInterfaceSupportCorrect(accWrap),
               "Does not support Text when it should");
#endif

#define ROLE(geckoRole, stringRole, atkRole, macRole, msaaRole, ia2Role, nameRule) \
  case roles::geckoRole: \
    return macRole;

  switch (mRole) {
#include "RoleMap.h"
    default:
      NS_NOTREACHED("Unknown role.");
      return NSAccessibilityUnknownRole;
  }

#undef ROLE
}

- (NSString*)subrole
{
  AccessibleWrap* accWrap = [self getGeckoAccessible];
  if (!accWrap)
    return nil;

  nsIAtom* landmark = accWrap->LandmarkRole();
  if (landmark) {
    if (landmark == nsGkAtoms::application)
      return @"AXLandmarkApplication";
    if (landmark == nsGkAtoms::banner)
      return @"AXLandmarkBanner";
    if (landmark == nsGkAtoms::complementary)
      return @"AXLandmarkComplementary";
    if (landmark == nsGkAtoms::contentinfo)
      return @"AXLandmarkContentInfo";
    if (landmark == nsGkAtoms::form)
      return @"AXLandmarkForm";
    if (landmark == nsGkAtoms::main)
      return @"AXLandmarkMain";
    if (landmark == nsGkAtoms::navigation)
      return @"AXLandmarkNavigation";
    if (landmark == nsGkAtoms::search)
      return @"AXLandmarkSearch";
    if (landmark == nsGkAtoms::searchbox)
      return @"AXSearchField";
  }

  switch (mRole) {
    case roles::LIST:
      return @"AXContentList"; // 10.6+ NSAccessibilityContentListSubrole;

    case roles::ENTRY:
      if (accWrap->IsSearchbox())
        return @"AXSearchField";
      break;

    case roles::DEFINITION_LIST:
      return @"AXDefinitionList"; // 10.6+ NSAccessibilityDefinitionListSubrole;

    case roles::TERM:
      return @"AXTerm";

    case roles::DEFINITION:
      return @"AXDefinition";

    case roles::SWITCH:
      return @"AXSwitch";

    default:
      break;
  }

  return nil;
}

struct RoleDescrMap
{
  NSString* role;
  const nsString& description;
};

static const RoleDescrMap sRoleDescrMap[] = {
  { @"AXDefinition", NS_LITERAL_STRING("definition") },
  { @"AXLandmarkBanner", NS_LITERAL_STRING("banner") },
  { @"AXLandmarkComplementary", NS_LITERAL_STRING("complementary") },
  { @"AXLandmarkContentInfo", NS_LITERAL_STRING("content") },
  { @"AXLandmarkMain", NS_LITERAL_STRING("main") },
  { @"AXLandmarkNavigation", NS_LITERAL_STRING("navigation") },
  { @"AXLandmarkSearch", NS_LITERAL_STRING("search") },
  { @"AXSearchField", NS_LITERAL_STRING("searchTextField") },
  { @"AXTerm", NS_LITERAL_STRING("term") }
};

struct RoleDescrComparator
{
  const NSString* mRole;
  explicit RoleDescrComparator(const NSString* aRole) : mRole(aRole) {}
  int operator()(const RoleDescrMap& aEntry) const {
    return [mRole compare:aEntry.role];
  }
};

- (NSString*)roleDescription
{
  if (mRole == roles::DOCUMENT)
    return utils::LocalizedString(NS_LITERAL_STRING("htmlContent"));

  NSString* subrole = [self subrole];

  size_t idx = 0;
  if (BinarySearchIf(sRoleDescrMap, 0, ArrayLength(sRoleDescrMap),
                     RoleDescrComparator(subrole), &idx)) {
    return utils::LocalizedString(sRoleDescrMap[idx].description);
  }

  return NSAccessibilityRoleDescription([self role], subrole);
}

- (NSString*)title
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsAutoString title;
  [self getGeckoAccessible]->Name(title);
  return nsCocoaUtils::ToNSString(title);

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)value
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsAutoString value;
  [self getGeckoAccessible]->Value(value);
  return nsCocoaUtils::ToNSString(value);

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

- (void)selectedTextDidChange
{
  // Do nothing. mozTextAccessible will.
}

- (NSString*)customDescription
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  AccessibleWrap* accWrap = [self getGeckoAccessible];
  if (accWrap->IsDefunct())
    return nil;

  nsAutoString desc;
  accWrap->Description(desc);

  return nsCocoaUtils::ToNSString(desc);

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSString*)help
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsAutoString helpText;
  [self getGeckoAccessible]->Help(helpText);
  return nsCocoaUtils::ToNSString(helpText);

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
  return FocusMgr()->IsFocused([self getGeckoAccessible]);
}

- (BOOL)canBeFocused
{
  AccessibleWrap* accWrap = [self getGeckoAccessible];
  return accWrap && (accWrap->InteractiveState() & states::FOCUSABLE);
}

- (BOOL)focus
{
  AccessibleWrap* accWrap = [self getGeckoAccessible];
  if (!accWrap)
    return NO;

  accWrap->TakeFocus();
  return YES;
}

- (BOOL)isEnabled
{
  AccessibleWrap* accWrap = [self getGeckoAccessible];
  return accWrap && ((accWrap->InteractiveState() & states::UNAVAILABLE) == 0);
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

  AccessibleWrap* accWrap = [self getGeckoAccessible];

  // Get a pointer to the native window (NSWindow) we reside in.
  NSWindow *nativeWindow = nil;
  DocAccessible* docAcc = accWrap->Document();
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

- (void)appendChild:(Accessible*)aAccessible
{
  // if mChildren is nil, then we don't even need to bother
  if (!mChildren)
    return;
    
  mozAccessible *curNative = GetNativeFromGeckoAccessible(aAccessible);
  if (curNative)
    [mChildren addObject:GetObjectOrRepresentedView(curNative)];
}

- (void)expire
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [self invalidateChildren];

  mGeckoAccessible = 0;
  
  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (BOOL)isExpired
{
  return ![self getGeckoAccessible];
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
