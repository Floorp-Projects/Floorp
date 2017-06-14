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
#include "DocAccessibleParent.h"
#include "Relation.h"
#include "Role.h"
#include "RootAccessible.h"
#include "TableAccessible.h"
#include "TableCellAccessible.h"
#include "mozilla/a11y/PDocAccessible.h"
#include "OuterDocAccessible.h"

#include "mozilla/Services.h"
#include "nsRect.h"
#include "nsCocoaUtils.h"
#include "nsCoord.h"
#include "nsObjCExceptions.h"
#include "nsWhitespaceTokenizer.h"
#include <prdtoa.h>

using namespace mozilla;
using namespace mozilla::a11y;

#define NSAccessibilityMathRootRadicandAttribute @"AXMathRootRadicand"
#define NSAccessibilityMathRootIndexAttribute @"AXMathRootIndex"
#define NSAccessibilityMathFractionNumeratorAttribute @"AXMathFractionNumerator"
#define NSAccessibilityMathFractionDenominatorAttribute @"AXMathFractionDenominator"
#define NSAccessibilityMathBaseAttribute @"AXMathBase"
#define NSAccessibilityMathSubscriptAttribute @"AXMathSubscript"
#define NSAccessibilityMathSuperscriptAttribute @"AXMathSuperscript"
#define NSAccessibilityMathUnderAttribute @"AXMathUnder"
#define NSAccessibilityMathOverAttribute @"AXMathOver"
#define NSAccessibilityMathLineThicknessAttribute @"AXMathLineThickness"
// XXX WebKit also defines the following attributes.
// See bugs 1176970 and 1176983.
// - NSAccessibilityMathFencedOpenAttribute @"AXMathFencedOpen"
// - NSAccessibilityMathFencedCloseAttribute @"AXMathFencedClose"
// - NSAccessibilityMathPrescriptsAttribute @"AXMathPrescripts"
// - NSAccessibilityMathPostscriptsAttribute @"AXMathPostscripts"

// convert an array of Gecko accessibles to an NSArray of native accessibles
static inline NSMutableArray*
ConvertToNSArray(nsTArray<Accessible*>& aArray)
{
  NSMutableArray* nativeArray = [[NSMutableArray alloc] init];

  // iterate through the list, and get each native accessible.
  size_t totalCount = aArray.Length();
  for (size_t i = 0; i < totalCount; i++) {
    Accessible* curAccessible = aArray.ElementAt(i);
    mozAccessible* curNative = GetNativeFromGeckoAccessible(curAccessible);
    if (curNative)
      [nativeArray addObject:GetObjectOrRepresentedView(curNative)];
  }

  return nativeArray;
}

// convert an array of Gecko proxy accessibles to an NSArray of native accessibles
static inline NSMutableArray*
ConvertToNSArray(nsTArray<ProxyAccessible*>& aArray)
{
  NSMutableArray* nativeArray = [[NSMutableArray alloc] init];

  // iterate through the list, and get each native accessible.
  size_t totalCount = aArray.Length();
  for (size_t i = 0; i < totalCount; i++) {
    ProxyAccessible* curAccessible = aArray.ElementAt(i);
    mozAccessible* curNative = GetNativeFromProxy(curAccessible);
    if (curNative)
      [nativeArray addObject:GetObjectOrRepresentedView(curNative)];
  }

  return nativeArray;
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

  bool noRole = [[self role] isEqualToString:NSAccessibilityUnknownRole];
  if (AccessibleWrap* accWrap = [self getGeckoAccessible])
    return (noRole && !(accWrap->InteractiveState() & states::FOCUSABLE));

  if (ProxyAccessible* proxy = [self getProxyAccessible])
    return (noRole && !(proxy->State() & states::FOCUSABLE));

  return true;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (NSArray*)additionalAccessibilityAttributeNames
{
  NSMutableArray* additional = [NSMutableArray array];
  switch (mRole) {
    case roles::MATHML_ROOT:
      [additional addObject:NSAccessibilityMathRootIndexAttribute];
      [additional addObject:NSAccessibilityMathRootRadicandAttribute];
      break;
    case roles::MATHML_SQUARE_ROOT:
      [additional addObject:NSAccessibilityMathRootRadicandAttribute];
      break;
    case roles::MATHML_FRACTION:
      [additional addObject:NSAccessibilityMathFractionNumeratorAttribute];
      [additional addObject:NSAccessibilityMathFractionDenominatorAttribute];
      [additional addObject:NSAccessibilityMathLineThicknessAttribute];
      break;
    case roles::MATHML_SUB:
    case roles::MATHML_SUP:
    case roles::MATHML_SUB_SUP:
      [additional addObject:NSAccessibilityMathBaseAttribute];
      [additional addObject:NSAccessibilityMathSubscriptAttribute];
      [additional addObject:NSAccessibilityMathSuperscriptAttribute];
      break;
    case roles::MATHML_UNDER:
    case roles::MATHML_OVER:
    case roles::MATHML_UNDER_OVER:
      [additional addObject:NSAccessibilityMathBaseAttribute];
      [additional addObject:NSAccessibilityMathUnderAttribute];
      [additional addObject:NSAccessibilityMathOverAttribute];
      break;
    // XXX bug 1176983
    // roles::MATHML_MULTISCRIPTS should also have the following attributes:
    // - NSAccessibilityMathPrescriptsAttribute
    // - NSAccessibilityMathPostscriptsAttribute
    // XXX bug 1176970
    // roles::MATHML_FENCED should also have the following attributes:
    // - NSAccessibilityMathFencedOpenAttribute
    // - NSAccessibilityMathFencedCloseAttribute
    default:
      break;
  }

  return additional;
}

- (NSArray*)accessibilityAttributeNames
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  // if we're expired, we don't support any attributes.
  AccessibleWrap* accWrap = [self getGeckoAccessible];
  ProxyAccessible* proxy = [self getProxyAccessible];
  if (!accWrap && !proxy)
    return [NSArray array];

  static NSArray* generalAttributes = nil;

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
#if DEBUG
                                                           @"AXMozDescription",
#endif
                                                           nil];
  }

  NSArray* objectAttributes = generalAttributes;

  NSArray* additionalAttributes = [self additionalAccessibilityAttributeNames];
  if ([additionalAttributes count])
    objectAttributes = [objectAttributes arrayByAddingObjectsFromArray:additionalAttributes];

  return objectAttributes;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)childAt:(uint32_t)i
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (AccessibleWrap* accWrap = [self getGeckoAccessible]) {
    Accessible* child = accWrap->GetChildAt(i);
    return child ? GetNativeFromGeckoAccessible(child) : nil;
  } else if (ProxyAccessible* proxy = [self getProxyAccessible]) {
    ProxyAccessible* child = proxy->ChildAt(i);
    return child ? GetNativeFromProxy(child) : nil;
  }

  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)accessibilityAttributeValue:(NSString*)attribute
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  AccessibleWrap* accWrap = [self getGeckoAccessible];
  ProxyAccessible* proxy = [self getProxyAccessible];
  if (!accWrap && !proxy)
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
    if (accWrap) {
      Relation rel = accWrap->RelationByType(RelationType::LABELLED_BY);
      Accessible* tempAcc = rel.Next();
      return tempAcc ? GetNativeFromGeckoAccessible(tempAcc) : nil;
    }
    nsTArray<ProxyAccessible*> rel = proxy->RelationByType(RelationType::LABELLED_BY);
    ProxyAccessible* tempProxy = rel.SafeElementAt(0);
    return tempProxy ? GetNativeFromProxy(tempProxy) : nil;
  }
  if ([attribute isEqualToString:NSAccessibilityHelpAttribute])
    return [self help];

  switch (mRole) {
  case roles::MATHML_ROOT:
    if ([attribute isEqualToString:NSAccessibilityMathRootRadicandAttribute])
      return [self childAt:0];
    if ([attribute isEqualToString:NSAccessibilityMathRootIndexAttribute])
      return [self childAt:1];
    break;
  case roles::MATHML_SQUARE_ROOT:
    if ([attribute isEqualToString:NSAccessibilityMathRootRadicandAttribute])
      return [self childAt:0];
    break;
  case roles::MATHML_FRACTION:
    if ([attribute isEqualToString:NSAccessibilityMathFractionNumeratorAttribute])
      return [self childAt:0];
    if ([attribute isEqualToString:NSAccessibilityMathFractionDenominatorAttribute])
      return [self childAt:1];
    if ([attribute isEqualToString:NSAccessibilityMathLineThicknessAttribute]) {
      // WebKit sets line thickness to some logical value parsed in the
      // renderer object of the <mfrac> element. It's not clear whether the
      // exact value is relevant to assistive technologies. From a semantic
      // point of view, the only important point is to distinguish between
      // <mfrac> elements that have a fraction bar and those that do not.
      // Per the MathML 3 spec, the latter happens iff the linethickness
      // attribute is of the form [zero-float][optional-unit]. In that case we
      // set line thickness to zero and in the other cases we set it to one.
      nsAutoString thickness;
      if (accWrap) {
        nsCOMPtr<nsIPersistentProperties> attributes = accWrap->Attributes();
        nsAccUtils::GetAccAttr(attributes, nsGkAtoms::linethickness_, thickness);
      } else {
        AutoTArray<Attribute, 10> attrs;
        proxy->Attributes(&attrs);
        for (size_t i = 0 ; i < attrs.Length() ; i++) {
          if (attrs.ElementAt(i).Name() == "thickness") {
            thickness = attrs.ElementAt(i).Value();
            break;
          }
        }
      }
      double value = 1.0;
      if (!thickness.IsEmpty())
        value = PR_strtod(NS_LossyConvertUTF16toASCII(thickness).get(),
                          nullptr);
      return [NSNumber numberWithInteger:(value ? 1 : 0)];
    }
    break;
  case roles::MATHML_SUB:
    if ([attribute isEqualToString:NSAccessibilityMathBaseAttribute])
      return [self childAt:0];
    if ([attribute isEqualToString:NSAccessibilityMathSubscriptAttribute])
      return [self childAt:1];
#ifdef DEBUG
    if ([attribute isEqualToString:NSAccessibilityMathSuperscriptAttribute])
      return nil;
#endif
    break;
  case roles::MATHML_SUP:
    if ([attribute isEqualToString:NSAccessibilityMathBaseAttribute])
      return [self childAt:0];
#ifdef DEBUG
    if ([attribute isEqualToString:NSAccessibilityMathSubscriptAttribute])
      return nil;
#endif
    if ([attribute isEqualToString:NSAccessibilityMathSuperscriptAttribute])
      return [self childAt:1];
    break;
  case roles::MATHML_SUB_SUP:
    if ([attribute isEqualToString:NSAccessibilityMathBaseAttribute])
      return [self childAt:0];
    if ([attribute isEqualToString:NSAccessibilityMathSubscriptAttribute])
      return [self childAt:1];
    if ([attribute isEqualToString:NSAccessibilityMathSuperscriptAttribute])
      return [self childAt:2];
    break;
  case roles::MATHML_UNDER:
    if ([attribute isEqualToString:NSAccessibilityMathBaseAttribute])
      return [self childAt:0];
    if ([attribute isEqualToString:NSAccessibilityMathUnderAttribute])
      return [self childAt:1];
#ifdef DEBUG
    if ([attribute isEqualToString:NSAccessibilityMathOverAttribute])
      return nil;
#endif
    break;
  case roles::MATHML_OVER:
    if ([attribute isEqualToString:NSAccessibilityMathBaseAttribute])
      return [self childAt:0];
#ifdef DEBUG
    if ([attribute isEqualToString:NSAccessibilityMathUnderAttribute])
      return nil;
#endif
    if ([attribute isEqualToString:NSAccessibilityMathOverAttribute])
      return [self childAt:1];
    break;
  case roles::MATHML_UNDER_OVER:
    if ([attribute isEqualToString:NSAccessibilityMathBaseAttribute])
      return [self childAt:0];
    if ([attribute isEqualToString:NSAccessibilityMathUnderAttribute])
      return [self childAt:1];
    if ([attribute isEqualToString:NSAccessibilityMathOverAttribute])
      return [self childAt:2];
    break;
  // XXX bug 1176983
  // roles::MATHML_MULTISCRIPTS should also have the following attributes:
  // - NSAccessibilityMathPrescriptsAttribute
  // - NSAccessibilityMathPostscriptsAttribute
  // XXX bug 1176970
  // roles::MATHML_FENCED should also have the following attributes:
  // - NSAccessibilityMathFencedOpenAttribute
  // - NSAccessibilityMathFencedCloseAttribute
  default:
    break;
  }

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
  ProxyAccessible* proxy = [self getProxyAccessible];
  if (!accWrap && !proxy)
    return nil;

  // Convert the given screen-global point in the cocoa coordinate system (with
  // origin in the bottom-left corner of the screen) into point in the Gecko
  // coordinate system (with origin in a top-left screen point).
  NSScreen* mainView = [[NSScreen screens] objectAtIndex:0];
  NSPoint tmpPoint = NSMakePoint(point.x,
                                 [mainView frame].size.height - point.y);
  LayoutDeviceIntPoint geckoPoint = nsCocoaUtils::
    CocoaPointsToDevPixels(tmpPoint, nsCocoaUtils::GetBackingScaleFactor(mainView));

  mozAccessible* nativeChild = nil;
  if (accWrap) {
    Accessible* child = accWrap->ChildAtPoint(geckoPoint.x, geckoPoint.y,
                                  Accessible::eDeepestChild);
    if (child)
      nativeChild = GetNativeFromGeckoAccessible(child);
  } else if (proxy) {
    ProxyAccessible* child = proxy->ChildAtPoint(geckoPoint.x, geckoPoint.y,
                                  Accessible::eDeepestChild);
    if (child)
      nativeChild = GetNativeFromProxy(child);
  }

  if (nativeChild)
    return nativeChild;

  // if we didn't find anything, return ourself or child view.
  return GetObjectOrRepresentedView(self);
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
  ProxyAccessible* proxy = [self getProxyAccessible];
  if (!accWrap && !proxy)
    return nil;

  mozAccessible* focusedChild = nil;
  if (accWrap) {
    Accessible* focusedGeckoChild = accWrap->FocusedChild();
    if (focusedGeckoChild)
      focusedChild = GetNativeFromGeckoAccessible(focusedGeckoChild);
  } else if (proxy) {
    ProxyAccessible* focusedGeckoChild = proxy->FocusedChild();
    if (focusedGeckoChild)
      focusedChild = GetNativeFromProxy(focusedGeckoChild);
  }

  if (focusedChild)
    return GetObjectOrRepresentedView(focusedChild);

  // return ourself if we can't get a native focused child.
  return GetObjectOrRepresentedView(self);
}

#pragma mark -

- (id <mozAccessible>)parent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  id nativeParent = nil;
  if (AccessibleWrap* accWrap = [self getGeckoAccessible]) {
    Accessible* accessibleParent = accWrap->Parent();
    if (accessibleParent)
      nativeParent = GetNativeFromGeckoAccessible(accessibleParent);
    if (nativeParent)
      return GetObjectOrRepresentedView(nativeParent);

    // Return native of root accessible if we have no direct parent
    nativeParent = GetNativeFromGeckoAccessible(accWrap->RootAccessible());
  } else if (ProxyAccessible* proxy = [self getProxyAccessible]) {
    if (ProxyAccessible* proxyParent = proxy->Parent()) {
      nativeParent = GetNativeFromProxy(proxyParent);
    }

    if (nativeParent)
      return GetObjectOrRepresentedView(nativeParent);

    Accessible* outerDoc = proxy->OuterDocOfRemoteBrowser();
    nativeParent = outerDoc ?
      GetNativeFromGeckoAccessible(outerDoc) : nil;
  } else {
    return nil;
  }

  NSAssert1 (nativeParent, @"!!! we can't find a parent for %@", self);

  return GetObjectOrRepresentedView(nativeParent);

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

  // get the array of children.
  mChildren = [[NSMutableArray alloc] init];

  AccessibleWrap* accWrap = [self getGeckoAccessible];
  if (accWrap) {
    uint32_t childCount = accWrap->ChildCount();
    for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
      mozAccessible* nativeChild = GetNativeFromGeckoAccessible(accWrap->GetChildAt(childIdx));
      if (nativeChild)
        [mChildren addObject:nativeChild];
    }

    // children from child if this is an outerdoc
    OuterDocAccessible* docOwner = accWrap->AsOuterDoc();
    if (docOwner) {
      if (ProxyAccessible* proxyDoc = docOwner->RemoteChildDoc()) {
        mozAccessible* nativeRemoteChild = GetNativeFromProxy(proxyDoc);
        [mChildren insertObject:nativeRemoteChild atIndex:0];
        NSAssert1 (nativeRemoteChild, @"%@ found a child remote doc missing a native\n", self);
      }
    }
  } else if (ProxyAccessible* proxy = [self getProxyAccessible]) {
      uint32_t childCount = proxy->ChildrenCount();
      for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
        mozAccessible* nativeChild = GetNativeFromProxy(proxy->ChildAt(childIdx));
        if (nativeChild)
          [mChildren addObject:nativeChild];
      }

  }

  return mChildren;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSValue*)position
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsIntRect rect;
  if (AccessibleWrap* accWrap = [self getGeckoAccessible])
    rect = accWrap->Bounds();
  else if (ProxyAccessible* proxy = [self getProxyAccessible])
    rect = proxy->Bounds();
  else
    return nil;

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

  nsIntRect rect;
  if (AccessibleWrap* accWrap = [self getGeckoAccessible])
    rect = accWrap->Bounds();
  else if (ProxyAccessible* proxy = [self getProxyAccessible])
    rect = proxy->Bounds();
  else
    return nil;

  CGFloat scaleFactor =
    nsCocoaUtils::GetBackingScaleFactor([[NSScreen screens] objectAtIndex:0]);
  return [NSValue valueWithSize:NSMakeSize(static_cast<CGFloat>(rect.width) / scaleFactor,
                                           static_cast<CGFloat>(rect.height) / scaleFactor)];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSString*)role
{
  AccessibleWrap* accWrap = [self getGeckoAccessible];
  if (accWrap) {
    #ifdef DEBUG_A11Y
      NS_ASSERTION(nsAccUtils::IsTextInterfaceSupportCorrect(accWrap),
                   "Does not support Text when it should");
    #endif
  } else if (![self getProxyAccessible]) {
    return nil;
  }

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
  ProxyAccessible* proxy = [self getProxyAccessible];

  // Deal with landmarks first
  nsIAtom* landmark = nullptr;
  if (accWrap)
    landmark = accWrap->LandmarkRole();
  else if (proxy)
    landmark = proxy->LandmarkRole();

  // HTML Elements treated as landmarks
  // XXX bug 1371712
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

  // macOS groups the specific landmark types of DPub ARIA into two broad
  // categories with corresponding subroles: Navigation and region/container.
  if (mRole == roles::NAVIGATION)
    return @"AXLandmarkNavigation";
  if (mRole == roles::LANDMARK)
    return @"AXLandmarkRegion";

  // Now, deal with widget roles
  nsIAtom* roleAtom = nullptr;
  if (accWrap && accWrap->HasARIARole()) {
    const nsRoleMapEntry* roleMap = accWrap->ARIARoleMap();
    roleAtom = *roleMap->roleAtom;
  }
  if (proxy)
    roleAtom = proxy->ARIARoleAtom();

  if (roleAtom) {
    if (roleAtom == nsGkAtoms::alert)
      return @"AXApplicationAlert";
    if (roleAtom == nsGkAtoms::alertdialog)
      return @"AXApplicationAlertDialog";
    if (roleAtom == nsGkAtoms::article)
      return @"AXDocumentArticle";
    if (roleAtom == nsGkAtoms::dialog)
      return @"AXApplicationDialog";
    if (roleAtom == nsGkAtoms::document)
      return @"AXDocument";
    if (roleAtom == nsGkAtoms::log_)
      return @"AXApplicationLog";
    if (roleAtom == nsGkAtoms::marquee)
      return @"AXApplicationMarquee";
    if (roleAtom == nsGkAtoms::math)
      return @"AXDocumentMath";
    if (roleAtom == nsGkAtoms::note_)
      return @"AXDocumentNote";
    if (roleAtom == nsGkAtoms::region)
      return @"AXDocumentRegion";
    if (roleAtom == nsGkAtoms::status)
      return @"AXApplicationStatus";
    if (roleAtom == nsGkAtoms::tabpanel)
      return @"AXTabPanel";
    if (roleAtom == nsGkAtoms::timer)
      return @"AXApplicationTimer";
    if (roleAtom == nsGkAtoms::tooltip)
      return @"AXUserInterfaceTooltip";
  }

  switch (mRole) {
    case roles::LIST:
      return @"AXContentList"; // 10.6+ NSAccessibilityContentListSubrole;

    case roles::ENTRY:
      if ((accWrap && accWrap->IsSearchbox()) ||
          (proxy && proxy->IsSearchbox()))
        return @"AXSearchField";
      break;

    case roles::DEFINITION_LIST:
      return @"AXDefinitionList"; // 10.6+ NSAccessibilityDefinitionListSubrole;

    case roles::TERM:
      return @"AXTerm";

    case roles::DEFINITION:
      return @"AXDefinition";

    case roles::MATHML_MATH:
      return @"AXDocumentMath";

    case roles::MATHML_FRACTION:
      return @"AXMathFraction";

    case roles::MATHML_FENCED:
      // XXX bug 1176970
      // This should be AXMathFence, but doing so without implementing the
      // whole fence interface seems to make VoiceOver crash, so we present it
      // as a row for now.
      return @"AXMathRow";

    case roles::MATHML_SUB:
    case roles::MATHML_SUP:
    case roles::MATHML_SUB_SUP:
      return @"AXMathSubscriptSuperscript";

    case roles::MATHML_ROW:
    case roles::MATHML_STYLE:
    case roles::MATHML_ERROR:
      return @"AXMathRow";

    case roles::MATHML_UNDER:
    case roles::MATHML_OVER:
    case roles::MATHML_UNDER_OVER:
      return @"AXMathUnderOver";

    case roles::MATHML_SQUARE_ROOT:
      return @"AXMathSquareRoot";

    case roles::MATHML_ROOT:
      return @"AXMathRoot";

    case roles::MATHML_TEXT:
      return @"AXMathText";

    case roles::MATHML_NUMBER:
      return @"AXMathNumber";

    case roles::MATHML_IDENTIFIER:
      return @"AXMathIdentifier";

    case roles::MATHML_TABLE:
      return @"AXMathTable";

    case roles::MATHML_TABLE_ROW:
      return @"AXMathTableRow";

    case roles::MATHML_CELL:
      return @"AXMathTableCell";

    // XXX: NSAccessibility also uses subroles AXMathSeparatorOperator and
    // AXMathFenceOperator. We should use the NS_MATHML_OPERATOR_FENCE and
    // NS_MATHML_OPERATOR_SEPARATOR bits of nsOperatorFlags, but currently they
    // are only available from the MathML layout code. Hence we just fallback
    // to subrole AXMathOperator for now.
    // XXX bug 1175747 WebKit also creates anonymous operators for <mfenced>
    // which have subroles AXMathSeparatorOperator and AXMathFenceOperator.
    case roles::MATHML_OPERATOR:
      return @"AXMathOperator";

    case roles::MATHML_MULTISCRIPTS:
      return @"AXMathMultiscript";

    case roles::SWITCH:
      return @"AXSwitch";

    case roles::ALERT:
      return @"AXApplicationAlert";

    case roles::SEPARATOR:
      return @"AXContentSeparator";

    case roles::PROPERTYPAGE:
      return @"AXTabPanel";

    case roles::DETAILS:
      return @"AXDetails";

    case roles::SUMMARY:
      return @"AXSummary";

    case roles::NOTE:
      return @"AXDocumentNote";

    // macOS added an AXSubrole value to distinguish generic AXGroup objects
    // from those which are AXGroups as a result of an explicit ARIA role,
    // such as the non-landmark, non-listitem text containers in DPub ARIA.
    case roles::SECTION:
      if (roleAtom)
        return @"AXApplicationGroup";
      break;

    default:
      break;
  }

  return nil;
}

struct RoleDescrMap
{
  NSString* role;
  const nsString description;
};

static const RoleDescrMap sRoleDescrMap[] = {
  { @"AXApplicationAlert", NS_LITERAL_STRING("alert") },
  { @"AXApplicationAlertDialog", NS_LITERAL_STRING("alertDialog") },
  { @"AXApplicationLog", NS_LITERAL_STRING("log") },
  { @"AXApplicationMarquee", NS_LITERAL_STRING("marquee") },
  { @"AXApplicationStatus", NS_LITERAL_STRING("status") },
  { @"AXApplicationTimer", NS_LITERAL_STRING("timer") },
  { @"AXContentSeparator", NS_LITERAL_STRING("separator") },
  { @"AXDefinition", NS_LITERAL_STRING("definition") },
  { @"AXDocument", NS_LITERAL_STRING("document") },
  { @"AXDocumentArticle", NS_LITERAL_STRING("article") },
  { @"AXDocumentMath", NS_LITERAL_STRING("math") },
  { @"AXDocumentNote", NS_LITERAL_STRING("note") },
  { @"AXDocumentRegion", NS_LITERAL_STRING("region") },
  { @"AXLandmarkApplication", NS_LITERAL_STRING("application") },
  { @"AXLandmarkBanner", NS_LITERAL_STRING("banner") },
  { @"AXLandmarkComplementary", NS_LITERAL_STRING("complementary") },
  { @"AXLandmarkContentInfo", NS_LITERAL_STRING("content") },
  { @"AXLandmarkMain", NS_LITERAL_STRING("main") },
  { @"AXLandmarkNavigation", NS_LITERAL_STRING("navigation") },
  { @"AXLandmarkSearch", NS_LITERAL_STRING("search") },
  { @"AXSearchField", NS_LITERAL_STRING("searchTextField") },
  { @"AXTabPanel", NS_LITERAL_STRING("tabPanel") },
  { @"AXTerm", NS_LITERAL_STRING("term") },
  { @"AXUserInterfaceTooltip", NS_LITERAL_STRING("tooltip") }
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

  if (subrole) {
    size_t idx = 0;
    if (BinarySearchIf(sRoleDescrMap, 0, ArrayLength(sRoleDescrMap),
                       RoleDescrComparator(subrole), &idx)) {
      return utils::LocalizedString(sRoleDescrMap[idx].description);
    }
  }

  return NSAccessibilityRoleDescription([self role], subrole);
}

- (NSString*)title
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsAutoString title;
  if (AccessibleWrap* accWrap = [self getGeckoAccessible])
    accWrap->Name(title);
  else if (ProxyAccessible* proxy = [self getProxyAccessible])
    proxy->Name(title);

  return nsCocoaUtils::ToNSString(title);

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)value
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsAutoString value;
  if (AccessibleWrap* accWrap = [self getGeckoAccessible])
    accWrap->Value(value);
  else if (ProxyAccessible* proxy = [self getProxyAccessible])
    proxy->Value(value);

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

- (void)documentLoadComplete
{
  id realSelf = GetObjectOrRepresentedView(self);
  NSAccessibilityPostNotification(realSelf, NSAccessibilityFocusedUIElementChangedNotification);
  NSAccessibilityPostNotification(realSelf, @"AXLoadComplete");
  NSAccessibilityPostNotification(realSelf, @"AXLayoutComplete");
}

- (NSString*)help
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  // What needs to go here is actually the accDescription of an item.
  // The MSAA acc_help method has nothing to do with this one.
  nsAutoString helpText;
  if (AccessibleWrap* accWrap = [self getGeckoAccessible])
    accWrap->Description(helpText);
  else if (ProxyAccessible* proxy = [self getProxyAccessible])
    proxy->Description(helpText);

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
  if (AccessibleWrap* accWrap = [self getGeckoAccessible])
      return accWrap->InteractiveState() & states::FOCUSABLE;

  if (ProxyAccessible* proxy = [self getProxyAccessible])
    return proxy->State() & states::FOCUSABLE;

  return false;
}

- (BOOL)focus
{
  if (AccessibleWrap* accWrap = [self getGeckoAccessible])
    accWrap->TakeFocus();
  else if (ProxyAccessible* proxy = [self getProxyAccessible])
    proxy->TakeFocus();
  else
    return NO;

  return YES;
}

- (BOOL)isEnabled
{
  if (AccessibleWrap* accWrap = [self getGeckoAccessible])
    return ((accWrap->InteractiveState() & states::UNAVAILABLE) == 0);

  if (ProxyAccessible* proxy = [self getProxyAccessible])
    return ((proxy->State() & states::UNAVAILABLE) == 0);

  return false;
}

// The root accessible calls this when the focused node was
// changed to us.
- (void)didReceiveFocus
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#ifdef DEBUG_hakan
  NSLog (@"%@ received focus!", self);
#endif
  NSAccessibilityPostNotification(GetObjectOrRepresentedView(self),
                                  NSAccessibilityFocusedUIElementChangedNotification);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (NSWindow*)window
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  // Get a pointer to the native window (NSWindow) we reside in.
  NSWindow *nativeWindow = nil;
  DocAccessible* docAcc = nullptr;
  if (AccessibleWrap* accWrap = [self getGeckoAccessible]) {
    docAcc = accWrap->Document();
  } else if (ProxyAccessible* proxy = [self getProxyAccessible]) {
    Accessible* outerDoc = proxy->OuterDocOfRemoteBrowser();
    if (outerDoc)
      docAcc = outerDoc->Document();
  }

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
    [mChildren addObject:curNative];
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
  return ![self getGeckoAccessible] && ![self getProxyAccessible];
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
