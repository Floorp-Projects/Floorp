/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozAccessible.h"

#import "MacUtils.h"
#import "mozView.h"

#include "Accessible-inl.h"
#include "nsAccUtils.h"
#include "xpcAccessibleMacInterface.h"
#include "nsIPersistentProperties2.h"
#include "DocAccessibleParent.h"
#include "Relation.h"
#include "Role.h"
#include "RootAccessible.h"
#include "TableAccessible.h"
#include "TableCellAccessible.h"
#include "mozilla/a11y/PDocAccessible.h"
#include "mozilla/dom/BrowserParent.h"
#include "OuterDocAccessible.h"
#include "nsChildView.h"

#include "nsRect.h"
#include "nsCocoaUtils.h"
#include "nsCoord.h"
#include "nsObjCExceptions.h"
#include "nsWhitespaceTokenizer.h"
#include <prdtoa.h>

using namespace mozilla;
using namespace mozilla::a11y;

#define NSAccessibilityARIACurrentAttribute @"AXARIACurrent"
#define NSAccessibilityDOMIdentifierAttribute @"AXDOMIdentifier"
#define NSAccessibilityHasPopupAttribute @"AXHasPopup"
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
#define NSAccessibilityScrollToVisibleAction @"AXScrollToVisible"

// XXX WebKit also defines the following attributes.
// See bugs 1176970 and 1176983.
// - NSAccessibilityMathFencedOpenAttribute @"AXMathFencedOpen"
// - NSAccessibilityMathFencedCloseAttribute @"AXMathFencedClose"
// - NSAccessibilityMathPrescriptsAttribute @"AXMathPrescripts"
// - NSAccessibilityMathPostscriptsAttribute @"AXMathPostscripts"

// convert an array of Gecko accessibles to an NSArray of native accessibles
static inline NSMutableArray* ConvertToNSArray(nsTArray<Accessible*>& aArray) {
  NSMutableArray* nativeArray = [[NSMutableArray alloc] init];

  // iterate through the list, and get each native accessible.
  size_t totalCount = aArray.Length();
  for (size_t i = 0; i < totalCount; i++) {
    Accessible* curAccessible = aArray.ElementAt(i);
    mozAccessible* curNative = GetNativeFromGeckoAccessible(curAccessible);
    if (curNative) [nativeArray addObject:GetObjectOrRepresentedView(curNative)];
  }

  return nativeArray;
}

// convert an array of Gecko proxy accessibles to an NSArray of native accessibles
static inline NSMutableArray* ConvertToNSArray(nsTArray<ProxyAccessible*>& aArray) {
  NSMutableArray* nativeArray = [[NSMutableArray alloc] init];

  // iterate through the list, and get each native accessible.
  size_t totalCount = aArray.Length();
  for (size_t i = 0; i < totalCount; i++) {
    ProxyAccessible* curAccessible = aArray.ElementAt(i);
    mozAccessible* curNative = GetNativeFromProxy(curAccessible);
    if (curNative) [nativeArray addObject:GetObjectOrRepresentedView(curNative)];
  }

  return nativeArray;
}

#pragma mark -

@implementation mozAccessible

- (id)initWithAccessible:(uintptr_t)aGeckoAccessible {
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

- (void)dealloc {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mChildren release];
  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (mozilla::a11y::AccessibleWrap*)getGeckoAccessible {
  // Check if mGeckoAccessible points at a proxy
  if (mGeckoAccessible & IS_PROXY) return nil;

  return reinterpret_cast<AccessibleWrap*>(mGeckoAccessible);
}

- (mozilla::a11y::ProxyAccessible*)getProxyAccessible {
  // Check if mGeckoAccessible points at a proxy
  if (!(mGeckoAccessible & IS_PROXY)) return nil;

  return reinterpret_cast<ProxyAccessible*>(mGeckoAccessible & ~IS_PROXY);
}

#pragma mark -

- (BOOL)accessibilityIsIgnored {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  // unknown (either unimplemented, or irrelevant) elements are marked as ignored
  // as well as expired elements.

  return [[self role] isEqualToString:NSAccessibilityUnknownRole] &&
         [self stateWithMask:states::FOCUSABLE];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (NSArray*)additionalAccessibilityAttributeNames {
  NSMutableArray* additional = [NSMutableArray array];
  [additional addObject:NSAccessibilityDOMIdentifierAttribute];
  switch (mRole) {
    case roles::SUMMARY:
      [additional addObject:NSAccessibilityExpandedAttribute];
      break;
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

- (NSArray*)accessibilityAttributeNames {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  // if we're expired, we don't support any attributes.
  AccessibleWrap* accWrap = [self getGeckoAccessible];
  ProxyAccessible* proxy = [self getProxyAccessible];
  if (!accWrap && !proxy) return [NSArray array];

  static NSArray* generalAttributes = nil;

  if (!generalAttributes) {
    // standard attributes that are shared and supported by all generic elements.
    generalAttributes = [[NSArray alloc]
        initWithObjects:NSAccessibilityChildrenAttribute, NSAccessibilityParentAttribute,
                        NSAccessibilityRoleAttribute, NSAccessibilityTitleAttribute,
                        NSAccessibilityValueAttribute, NSAccessibilitySubroleAttribute,
                        NSAccessibilityRoleDescriptionAttribute, NSAccessibilityPositionAttribute,
                        NSAccessibilityEnabledAttribute, NSAccessibilitySizeAttribute,
                        NSAccessibilityWindowAttribute, NSAccessibilityFocusedAttribute,
                        NSAccessibilityHelpAttribute, NSAccessibilityTitleUIElementAttribute,
                        NSAccessibilityTopLevelUIElementAttribute, NSAccessibilityHasPopupAttribute,
                        NSAccessibilityARIACurrentAttribute,
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

- (id)childAt:(uint32_t)i {
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

- (uint64_t)state {
  if (AccessibleWrap* accWrap = [self getGeckoAccessible]) {
    return accWrap->State();
  }

  if (ProxyAccessible* proxy = [self getProxyAccessible]) {
    return proxy->State();
  }

  return 0;
}

- (uint64_t)stateWithMask:(uint64_t)mask {
  return [self state] & mask;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  AccessibleWrap* accWrap = [self getGeckoAccessible];
  ProxyAccessible* proxy = [self getProxyAccessible];
  if (!accWrap && !proxy) return nil;

#if DEBUG
  if ([attribute isEqualToString:@"AXMozDescription"])
    return [NSString stringWithFormat:@"role = %u native = %@", mRole, [self class]];
#endif

  if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) return [self children];
  if ([attribute isEqualToString:NSAccessibilityExpandedAttribute]) {
    return [NSNumber numberWithBool:[self stateWithMask:states::EXPANDED] != 0];
  }
  if ([attribute isEqualToString:NSAccessibilityParentAttribute]) return [self parent];

#ifdef DEBUG_hakan
  NSLog(@"(%@ responding to attr %@)", self, attribute);
#endif

  if ([attribute isEqualToString:NSAccessibilityRoleAttribute]) return [self role];
  if ([attribute isEqualToString:NSAccessibilityPositionAttribute]) return [self position];
  if ([attribute isEqualToString:NSAccessibilitySubroleAttribute]) return [self subrole];
  if ([attribute isEqualToString:NSAccessibilityEnabledAttribute])
    return [NSNumber numberWithBool:[self isEnabled]];
  if ([attribute isEqualToString:NSAccessibilityHasPopupAttribute]) {
    return [NSNumber numberWithBool:[self stateWithMask:states::HASPOPUP] != 0];
  }
  if ([attribute isEqualToString:NSAccessibilityValueAttribute]) return [self value];
  if ([attribute isEqualToString:NSAccessibilityARIACurrentAttribute]) {
    return utils::GetAccAttr(self, "current");
  }
  if ([attribute isEqualToString:NSAccessibilityRoleDescriptionAttribute])
    return [self roleDescription];
  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute])
    return [NSNumber numberWithBool:[self isFocused]];
  if ([attribute isEqualToString:NSAccessibilitySizeAttribute]) return [self size];
  if ([attribute isEqualToString:NSAccessibilityWindowAttribute]) return [self window];
  if ([attribute isEqualToString:NSAccessibilityTopLevelUIElementAttribute]) return [self window];
  if ([attribute isEqualToString:NSAccessibilityTitleAttribute]) return [self title];
  if ([attribute isEqualToString:NSAccessibilityTitleUIElementAttribute]) {
    /* If our accessible is labelled by more than one item, its label
     * should be set by accessibilityLabel instead of here, so we return nil.
     */
    if (accWrap) {
      Relation rel = accWrap->RelationByType(RelationType::LABELLED_BY);
      Accessible* tempAcc = rel.Next();
      if (tempAcc && !rel.Next()) {
        return GetNativeFromGeckoAccessible(tempAcc);
      } else {
        return nil;
      }
    }
    nsTArray<ProxyAccessible*> rel = proxy->RelationByType(RelationType::LABELLED_BY);
    ProxyAccessible* tempProxy = rel.SafeElementAt(0);
    if (tempProxy && rel.Length() <= 1) {
      return GetNativeFromProxy(tempProxy);
    } else {
      return nil;
    }
  }
  if ([attribute isEqualToString:NSAccessibilityHelpAttribute]) return [self help];
  if ([attribute isEqualToString:NSAccessibilityOrientationAttribute]) return [self orientation];

  if ([attribute isEqualToString:NSAccessibilityDOMIdentifierAttribute]) {
    nsAutoString id;
    if (accWrap && accWrap->GetContent())
      nsCoreUtils::GetID(accWrap->GetContent(), id);
    else
      proxy->DOMNodeID(id);
    return nsCocoaUtils::ToNSString(id);
  }

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
        if (NSString* thickness = utils::GetAccAttr(self, "thickness")) {
          NSNumberFormatter* formatter = [[[NSNumberFormatter alloc] init] autorelease];
          NSNumber* value = [formatter numberFromString:thickness];
          return [NSNumber numberWithBool:[value boolValue]];
        } else {
          return [NSNumber numberWithInteger:0];
        }
      }
      break;
    case roles::MATHML_SUB:
      if ([attribute isEqualToString:NSAccessibilityMathBaseAttribute]) return [self childAt:0];
      if ([attribute isEqualToString:NSAccessibilityMathSubscriptAttribute])
        return [self childAt:1];
#ifdef DEBUG
      if ([attribute isEqualToString:NSAccessibilityMathSuperscriptAttribute]) return nil;
#endif
      break;
    case roles::MATHML_SUP:
      if ([attribute isEqualToString:NSAccessibilityMathBaseAttribute]) return [self childAt:0];
#ifdef DEBUG
      if ([attribute isEqualToString:NSAccessibilityMathSubscriptAttribute]) return nil;
#endif
      if ([attribute isEqualToString:NSAccessibilityMathSuperscriptAttribute])
        return [self childAt:1];
      break;
    case roles::MATHML_SUB_SUP:
      if ([attribute isEqualToString:NSAccessibilityMathBaseAttribute]) return [self childAt:0];
      if ([attribute isEqualToString:NSAccessibilityMathSubscriptAttribute])
        return [self childAt:1];
      if ([attribute isEqualToString:NSAccessibilityMathSuperscriptAttribute])
        return [self childAt:2];
      break;
    case roles::MATHML_UNDER:
      if ([attribute isEqualToString:NSAccessibilityMathBaseAttribute]) return [self childAt:0];
      if ([attribute isEqualToString:NSAccessibilityMathUnderAttribute]) return [self childAt:1];
#ifdef DEBUG
      if ([attribute isEqualToString:NSAccessibilityMathOverAttribute]) return nil;
#endif
      break;
    case roles::MATHML_OVER:
      if ([attribute isEqualToString:NSAccessibilityMathBaseAttribute]) return [self childAt:0];
#ifdef DEBUG
      if ([attribute isEqualToString:NSAccessibilityMathUnderAttribute]) return nil;
#endif
      if ([attribute isEqualToString:NSAccessibilityMathOverAttribute]) return [self childAt:1];
      break;
    case roles::MATHML_UNDER_OVER:
      if ([attribute isEqualToString:NSAccessibilityMathBaseAttribute]) return [self childAt:0];
      if ([attribute isEqualToString:NSAccessibilityMathUnderAttribute]) return [self childAt:1];
      if ([attribute isEqualToString:NSAccessibilityMathOverAttribute]) return [self childAt:2];
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
  NSLog(@"!!! %@ can't respond to attribute %@", self, attribute);
#endif
  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) return [self canBeFocused];

  return NO;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#ifdef DEBUG_hakan
  NSLog(@"[%@] %@='%@'", self, attribute, value);
#endif

  // we only support focusing elements so far.
  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute] && [value boolValue])
    [self focus];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (id)accessibilityHitTest:(NSPoint)point {
  AccessibleWrap* accWrap = [self getGeckoAccessible];
  ProxyAccessible* proxy = [self getProxyAccessible];
  if (!accWrap && !proxy) return nil;

  // Convert the given screen-global point in the cocoa coordinate system (with
  // origin in the bottom-left corner of the screen) into point in the Gecko
  // coordinate system (with origin in a top-left screen point).
  NSScreen* mainView = [[NSScreen screens] objectAtIndex:0];
  NSPoint tmpPoint = NSMakePoint(point.x, [mainView frame].size.height - point.y);
  LayoutDeviceIntPoint geckoPoint =
      nsCocoaUtils::CocoaPointsToDevPixels(tmpPoint, nsCocoaUtils::GetBackingScaleFactor(mainView));

  mozAccessible* nativeChild = nil;
  if (accWrap) {
    Accessible* child =
        accWrap->ChildAtPoint(geckoPoint.x, geckoPoint.y, Accessible::eDeepestChild);
    // If this is an outer doc, drill down further into proxies to find deepest remote child.
    if (OuterDocAccessible* docOwner = child->AsOuterDoc()) {
      if (ProxyAccessible* proxyDoc = docOwner->RemoteChildDoc()) {
        mozAccessible* nativeRemoteChild = GetNativeFromProxy(proxyDoc);
        return [nativeRemoteChild accessibilityHitTest:point];
      }
    } else if (child) {
      nativeChild = GetNativeFromGeckoAccessible(child);
    }
  } else if (proxy) {
    ProxyAccessible* child =
        proxy->ChildAtPoint(geckoPoint.x, geckoPoint.y, Accessible::eDeepestChild);
    if (child) nativeChild = GetNativeFromProxy(child);
  }

  if (nativeChild) return nativeChild;

  // if we didn't find anything, return ourself or child view.
  return GetObjectOrRepresentedView(self);
}

- (NSArray*)accessibilityActionNames {
  AccessibleWrap* accWrap = [self getGeckoAccessible];
  ProxyAccessible* proxy = [self getProxyAccessible];
  // Create actions array
  NSMutableArray* actions = [NSMutableArray new];
  if (!accWrap && !proxy) return actions;

  uint8_t count = 0;
  if (accWrap) {
    count = accWrap->ActionCount();
  } else if (proxy) {
    count = proxy->ActionCount();
  }

  // Check if the accessible has an existing gecko
  // action, and add the corresponding Mac action to
  // the actions array. `count` is guaranteed to be 0 or 1
  if (count) {
    nsAutoString name;
    if (accWrap) {
      accWrap->ActionNameAt(0, name);
    } else if (proxy) {
      proxy->ActionNameAt(0, name);
    }
    if (name.EqualsLiteral("select")) {
      [actions addObject:NSAccessibilityPickAction];
    } else {
      [actions addObject:NSAccessibilityPressAction];
    }
  }

  // Regardless of `count`, add actions that should be
  // performable on all accessibles. If we added a press
  // action, it will be first in the list. We append other
  // actions here to maintain that invariant.
  [actions addObject:NSAccessibilityScrollToVisibleAction];
  [actions addObject:NSAccessibilityShowMenuAction];

  return actions;
}

- (NSString*)accessibilityActionDescription:(NSString*)action {
  // by default we return whatever the MacOS API know about.
  // if you have custom actions, override.
  return NSAccessibilityActionDescription(action);
}

- (NSString*)accessibilityLabel {
  AccessibleWrap* accWrap = [self getGeckoAccessible];
  ProxyAccessible* proxy = [self getProxyAccessible];
  if (!accWrap && !proxy) {
    return nil;
  }

  nsAutoString name;

  /* If our accessible is:
   * 1. Named by invisible text, or
   * 2. Has more than one labeling relation, or
   * 3. Is a grouping
   *   ... return its name as a label (AXDescription).
   */
  if (accWrap) {
    ENameValueFlag flag = accWrap->Name(name);
    if (flag == eNameFromSubtree) {
      return nil;
    }

    if (mRole != roles::GROUPING && mRole != roles::RADIO_GROUP) {
      Relation rel = accWrap->RelationByType(RelationType::LABELLED_BY);
      if (rel.Next() && !rel.Next()) {
        return nil;
      }
    }
  } else if (proxy) {
    uint32_t flag = proxy->Name(name);
    if (flag == eNameFromSubtree) {
      return nil;
    }

    if (mRole != roles::GROUPING && mRole != roles::RADIO_GROUP) {
      nsTArray<ProxyAccessible*> rels = proxy->RelationByType(RelationType::LABELLED_BY);
      if (rels.Length() == 1) {
        return nil;
      }
    }
  }

  return nsCocoaUtils::ToNSString(name);
}

- (void)accessibilityPerformAction:(NSString*)action {
  RefPtr<AccessibleWrap> accWrap = [self getGeckoAccessible];
  ProxyAccessible* proxy = [self getProxyAccessible];

  if ([action isEqualToString:NSAccessibilityScrollToVisibleAction]) {
    if (accWrap) {
      accWrap->ScrollTo(nsIAccessibleScrollType::SCROLL_TYPE_ANYWHERE);
    } else if (proxy) {
      proxy->ScrollTo(nsIAccessibleScrollType::SCROLL_TYPE_ANYWHERE);
    }
  } else if ([action isEqualToString:NSAccessibilityShowMenuAction]) {
    // We don't need to convert this rect into mac coordinates because the
    // mouse event synthesizer expects layout (gecko) coordinates.
    LayoutDeviceIntRect geckoRect;
    id objOrView = nil;
    if (accWrap) {
      geckoRect = LayoutDeviceIntRect::FromUnknownRect(accWrap->Bounds());
      objOrView =
          GetObjectOrRepresentedView(GetNativeFromGeckoAccessible(accWrap->RootAccessible()));
    } else if (proxy) {
      geckoRect = LayoutDeviceIntRect::FromUnknownRect(proxy->Bounds());
      objOrView = GetObjectOrRepresentedView(
          GetNativeFromGeckoAccessible(proxy->OuterDocOfRemoteBrowser()->RootAccessible()));
    }

    LayoutDeviceIntPoint p = LayoutDeviceIntPoint(geckoRect.X(), geckoRect.Y());
    nsIWidget* widget = [objOrView widget];
    // XXX: NSRightMouseDown is depreciated in 10.12, should be
    // changed to NSEventTypeRightMouseDown after refactoring.
    widget->SynthesizeNativeMouseEvent(p, NSRightMouseDown, 0, nullptr);

  } else {
    if (accWrap) {
      accWrap->DoAction(0);
    } else if (proxy) {
      proxy->DoAction(0);
    }
  }
}

- (id)accessibilityFocusedUIElement {
  AccessibleWrap* accWrap = [self getGeckoAccessible];
  ProxyAccessible* proxy = [self getProxyAccessible];
  if (!accWrap && !proxy) return nil;

  mozAccessible* focusedChild = nil;
  if (accWrap) {
    Accessible* focusedGeckoChild = accWrap->FocusedChild();
    if (focusedGeckoChild) {
      focusedChild = GetNativeFromGeckoAccessible(focusedGeckoChild);
    } else {
      dom::BrowserParent* browser = dom::BrowserParent::GetFocused();
      if (browser) {
        a11y::DocAccessibleParent* proxyDoc = browser->GetTopLevelDocAccessible();
        if (proxyDoc) {
          mozAccessible* nativeRemoteChild = GetNativeFromProxy(proxyDoc);
          return [nativeRemoteChild accessibilityFocusedUIElement];
        }
      }
    }
  } else if (proxy) {
    ProxyAccessible* focusedGeckoChild = proxy->FocusedChild();
    if (focusedGeckoChild) focusedChild = GetNativeFromProxy(focusedGeckoChild);
  }

  if (focusedChild) return GetObjectOrRepresentedView(focusedChild);

  // return ourself if we can't get a native focused child.
  return GetObjectOrRepresentedView(self);
}

#pragma mark -

- (id<mozAccessible>)parent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  id nativeParent = nil;
  if (AccessibleWrap* accWrap = [self getGeckoAccessible]) {
    Accessible* accessibleParent = accWrap->Parent();
    if (accessibleParent) nativeParent = GetNativeFromGeckoAccessible(accessibleParent);
    if (nativeParent) return GetObjectOrRepresentedView(nativeParent);

    // Return native of root accessible if we have no direct parent
    nativeParent = GetNativeFromGeckoAccessible(accWrap->RootAccessible());
  } else if (ProxyAccessible* proxy = [self getProxyAccessible]) {
    if (ProxyAccessible* proxyParent = proxy->Parent()) {
      nativeParent = GetNativeFromProxy(proxyParent);
    }

    if (nativeParent) return GetObjectOrRepresentedView(nativeParent);

    Accessible* outerDoc = proxy->OuterDocOfRemoteBrowser();
    nativeParent = outerDoc ? GetNativeFromGeckoAccessible(outerDoc) : nil;
  } else {
    return nil;
  }

  NSAssert1(nativeParent, @"!!! we can't find a parent for %@", self);

  return GetObjectOrRepresentedView(nativeParent);

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)hasRepresentedView {
  return NO;
}

- (id)representedView {
  return nil;
}

- (BOOL)isRoot {
  return NO;
}

// gets our native children lazily.
// returns nil when there are no children.
- (NSArray*)children {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (mChildren) return mChildren;

  // get the array of children.
  mChildren = [[NSMutableArray alloc] init];

  AccessibleWrap* accWrap = [self getGeckoAccessible];
  if (accWrap) {
    uint32_t childCount = accWrap->ChildCount();
    for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
      mozAccessible* nativeChild = GetNativeFromGeckoAccessible(accWrap->GetChildAt(childIdx));
      if (nativeChild) [mChildren addObject:nativeChild];
    }

    // children from child if this is an outerdoc
    OuterDocAccessible* docOwner = accWrap->AsOuterDoc();
    if (docOwner) {
      if (ProxyAccessible* proxyDoc = docOwner->RemoteChildDoc()) {
        mozAccessible* nativeRemoteChild = GetNativeFromProxy(proxyDoc);
        [mChildren insertObject:nativeRemoteChild atIndex:0];
        NSAssert1(nativeRemoteChild, @"%@ found a child remote doc missing a native\n", self);
      }
    }
  } else if (ProxyAccessible* proxy = [self getProxyAccessible]) {
    uint32_t childCount = proxy->ChildrenCount();
    for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
      mozAccessible* nativeChild = GetNativeFromProxy(proxy->ChildAt(childIdx));
      if (nativeChild) [mChildren addObject:nativeChild];
    }
  }

  return mChildren;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSValue*)position {
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
  NSPoint p = NSMakePoint(
      static_cast<CGFloat>(rect.x) / scaleFactor,
      [mainView frame].size.height - static_cast<CGFloat>(rect.y + rect.height) / scaleFactor);

  return [NSValue valueWithPoint:p];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSValue*)size {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsIntRect rect;
  if (AccessibleWrap* accWrap = [self getGeckoAccessible])
    rect = accWrap->Bounds();
  else if (ProxyAccessible* proxy = [self getProxyAccessible])
    rect = proxy->Bounds();
  else
    return nil;

  CGFloat scaleFactor = nsCocoaUtils::GetBackingScaleFactor([[NSScreen screens] objectAtIndex:0]);
  return [NSValue valueWithSize:NSMakeSize(static_cast<CGFloat>(rect.width) / scaleFactor,
                                           static_cast<CGFloat>(rect.height) / scaleFactor)];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSString*)role {
  AccessibleWrap* accWrap = [self getGeckoAccessible];
  if (accWrap) {
#ifdef DEBUG_A11Y
    NS_ASSERTION(nsAccUtils::IsTextInterfaceSupportCorrect(accWrap),
                 "Does not support Text when it should");
#endif
  } else if (![self getProxyAccessible]) {
    return nil;
  }

#define ROLE(geckoRole, stringRole, atkRole, macRole, msaaRole, ia2Role, androidClass, nameRule) \
  case roles::geckoRole:                                                                         \
    return macRole;

  switch (mRole) {
#include "RoleMap.h"
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown role.");
      return NSAccessibilityUnknownRole;
  }

#undef ROLE
}

- (NSString*)subrole {
  AccessibleWrap* accWrap = [self getGeckoAccessible];
  ProxyAccessible* proxy = [self getProxyAccessible];

  // Deal with landmarks first
  nsAtom* landmark = nullptr;
  if (accWrap)
    landmark = accWrap->LandmarkRole();
  else if (proxy)
    landmark = proxy->LandmarkRole();

  // HTML Elements treated as landmarks, and ARIA landmarks.
  if (landmark) {
    if (landmark == nsGkAtoms::application) return @"AXLandmarkApplication";
    if (landmark == nsGkAtoms::banner) return @"AXLandmarkBanner";
    if (landmark == nsGkAtoms::complementary) return @"AXLandmarkComplementary";
    if (landmark == nsGkAtoms::contentinfo) return @"AXLandmarkContentInfo";
    if (landmark == nsGkAtoms::form) return @"AXLandmarkForm";
    if (landmark == nsGkAtoms::main) return @"AXLandmarkMain";
    if (landmark == nsGkAtoms::navigation) return @"AXLandmarkNavigation";
    if (landmark == nsGkAtoms::search) return @"AXLandmarkSearch";
    if (landmark == nsGkAtoms::searchbox) return @"AXSearchField";
  }

  // macOS groups the specific landmark types of DPub ARIA into two broad
  // categories with corresponding subroles: Navigation and region/container.
  if (mRole == roles::NAVIGATION) return @"AXLandmarkNavigation";
  if (mRole == roles::LANDMARK) return @"AXLandmarkRegion";

  // Now, deal with widget roles
  nsStaticAtom* roleAtom = nullptr;
  if (accWrap && accWrap->HasARIARole()) {
    const nsRoleMapEntry* roleMap = accWrap->ARIARoleMap();
    roleAtom = roleMap->roleAtom;
  }
  if (proxy) roleAtom = proxy->ARIARoleAtom();

  if (roleAtom) {
    if (roleAtom == nsGkAtoms::alert) return @"AXApplicationAlert";
    if (roleAtom == nsGkAtoms::alertdialog) return @"AXApplicationAlertDialog";
    if (roleAtom == nsGkAtoms::article) return @"AXDocumentArticle";
    if (roleAtom == nsGkAtoms::dialog) return @"AXApplicationDialog";
    if (roleAtom == nsGkAtoms::document) return @"AXDocument";
    if (roleAtom == nsGkAtoms::log_) return @"AXApplicationLog";
    if (roleAtom == nsGkAtoms::marquee) return @"AXApplicationMarquee";
    if (roleAtom == nsGkAtoms::math) return @"AXDocumentMath";
    if (roleAtom == nsGkAtoms::note_) return @"AXDocumentNote";
    if (roleAtom == nsGkAtoms::region) return mRole == roles::REGION ? @"AXLandmarkRegion" : nil;
    if (roleAtom == nsGkAtoms::status) return @"AXApplicationStatus";
    if (roleAtom == nsGkAtoms::tabpanel) return @"AXTabPanel";
    if (roleAtom == nsGkAtoms::timer) return @"AXApplicationTimer";
    if (roleAtom == nsGkAtoms::tooltip) return @"AXUserInterfaceTooltip";
  }

  switch (mRole) {
    case roles::LIST:
      return @"AXContentList";  // 10.6+ NSAccessibilityContentListSubrole;

    case roles::ENTRY:
      if ((accWrap && accWrap->IsSearchbox()) || (proxy && proxy->IsSearchbox()))
        return @"AXSearchField";
      break;

    case roles::DEFINITION_LIST:
      return @"AXDefinitionList";  // 10.6+ NSAccessibilityDefinitionListSubrole;

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

    case roles::PROPERTYPAGE:
      return @"AXTabPanel";

    case roles::DETAILS:
      return @"AXDetails";

    case roles::SUMMARY:
      return @"AXSummary";

    case roles::NOTE:
      return @"AXDocumentNote";

    case roles::OUTLINEITEM:
      return @"AXOutlineRow";

    case roles::ARTICLE:
      return @"AXDocumentArticle";

    case roles::NON_NATIVE_DOCUMENT:
      return @"AXDocument";

    // macOS added an AXSubrole value to distinguish generic AXGroup objects
    // from those which are AXGroups as a result of an explicit ARIA role,
    // such as the non-landmark, non-listitem text containers in DPub ARIA.
    case roles::FOOTNOTE:
    case roles::SECTION:
      if (roleAtom) return @"AXApplicationGroup";
      break;

    case roles::CONTENT_DELETION:
      return @"AXDeleteStyleGroup";

    case roles::CONTENT_INSERTION:
      return @"AXInsertStyleGroup";

    case roles::CODE:
      return @"AXCodeStyleGroup";

    case roles::TOGGLE_BUTTON:
      return @"AXToggle";

    default:
      break;
  }

  return nil;
}

struct RoleDescrMap {
  NSString* role;
  const nsString description;
};

static const RoleDescrMap sRoleDescrMap[] = {
    {@"AXApplicationAlert", NS_LITERAL_STRING("alert")},
    {@"AXApplicationAlertDialog", NS_LITERAL_STRING("alertDialog")},
    {@"AXApplicationLog", NS_LITERAL_STRING("log")},
    {@"AXApplicationMarquee", NS_LITERAL_STRING("marquee")},
    {@"AXApplicationStatus", NS_LITERAL_STRING("status")},
    {@"AXApplicationTimer", NS_LITERAL_STRING("timer")},
    {@"AXContentSeparator", NS_LITERAL_STRING("separator")},
    {@"AXDefinition", NS_LITERAL_STRING("definition")},
    {@"AXDetails", NS_LITERAL_STRING("details")},
    {@"AXDocument", NS_LITERAL_STRING("document")},
    {@"AXDocumentArticle", NS_LITERAL_STRING("article")},
    {@"AXDocumentMath", NS_LITERAL_STRING("math")},
    {@"AXDocumentNote", NS_LITERAL_STRING("note")},
    {@"AXLandmarkApplication", NS_LITERAL_STRING("application")},
    {@"AXLandmarkBanner", NS_LITERAL_STRING("banner")},
    {@"AXLandmarkComplementary", NS_LITERAL_STRING("complementary")},
    {@"AXLandmarkContentInfo", NS_LITERAL_STRING("content")},
    {@"AXLandmarkMain", NS_LITERAL_STRING("main")},
    {@"AXLandmarkNavigation", NS_LITERAL_STRING("navigation")},
    {@"AXLandmarkRegion", NS_LITERAL_STRING("region")},
    {@"AXLandmarkSearch", NS_LITERAL_STRING("search")},
    {@"AXSearchField", NS_LITERAL_STRING("searchTextField")},
    {@"AXSummary", NS_LITERAL_STRING("summary")},
    {@"AXTabPanel", NS_LITERAL_STRING("tabPanel")},
    {@"AXTerm", NS_LITERAL_STRING("term")},
    {@"AXUserInterfaceTooltip", NS_LITERAL_STRING("tooltip")}};

struct RoleDescrComparator {
  const NSString* mRole;
  explicit RoleDescrComparator(const NSString* aRole) : mRole(aRole) {}
  int operator()(const RoleDescrMap& aEntry) const { return [mRole compare:aEntry.role]; }
};

- (NSString*)roleDescription {
  if (mRole == roles::DOCUMENT) return utils::LocalizedString(NS_LITERAL_STRING("htmlContent"));

  if (mRole == roles::FIGURE) return utils::LocalizedString(NS_LITERAL_STRING("figure"));

  if (mRole == roles::HEADING) return utils::LocalizedString(NS_LITERAL_STRING("heading"));

  if (mRole == roles::MARK) {
    return utils::LocalizedString(NS_LITERAL_STRING("highlight"));
  }

  NSString* subrole = [self subrole];

  if (subrole) {
    size_t idx = 0;
    if (BinarySearchIf(sRoleDescrMap, 0, ArrayLength(sRoleDescrMap), RoleDescrComparator(subrole),
                       &idx)) {
      return utils::LocalizedString(sRoleDescrMap[idx].description);
    }
  }

  return NSAccessibilityRoleDescription([self role], subrole);
}

- (NSString*)title {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  // If this is a grouping we provide the name in the label (AXDescription).
  if (mRole == roles::GROUPING || mRole == roles::RADIO_GROUP) {
    return nil;
  }

  nsAutoString title;
  if (AccessibleWrap* accWrap = [self getGeckoAccessible])
    accWrap->Name(title);
  else if (ProxyAccessible* proxy = [self getProxyAccessible])
    proxy->Name(title);

  return nsCocoaUtils::ToNSString(title);

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)value {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsAutoString value;
  if (AccessibleWrap* accWrap = [self getGeckoAccessible])
    accWrap->Value(value);
  else if (ProxyAccessible* proxy = [self getProxyAccessible])
    proxy->Value(value);

  return nsCocoaUtils::ToNSString(value);

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSString*)help {
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

- (NSString*)orientation {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  uint64_t state = [self stateWithMask:(states::HORIZONTAL | states::VERTICAL)];

  if (state & states::HORIZONTAL) {
    return NSAccessibilityHorizontalOrientationValue;
  }

  if (state & states::VERTICAL) {
    return NSAccessibilityVerticalOrientationValue;
  }

  return NSAccessibilityUnknownOrientationValue;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

// objc-style description (from NSObject); not to be confused with the accessible description above.
- (NSString*)description {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [NSString stringWithFormat:@"(%p) %@", self, [self role]];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)isFocused {
  if (AccessibleWrap* accWrap = [self getGeckoAccessible]) {
    return FocusMgr()->IsFocused(accWrap);
  }

  return false;  // XXX: proxy implementation is needed.
}

- (BOOL)canBeFocused {
  return [self stateWithMask:states::FOCUSABLE] != 0;
}

- (BOOL)focus {
  if (AccessibleWrap* accWrap = [self getGeckoAccessible])
    accWrap->TakeFocus();
  else if (ProxyAccessible* proxy = [self getProxyAccessible])
    proxy->TakeFocus();
  else
    return NO;

  return YES;
}

- (BOOL)isEnabled {
  return [self stateWithMask:states::UNAVAILABLE] == 0;
}

- (void)firePlatformEvent:(uint32_t)eventType {
  switch (eventType) {
    case nsIAccessibleEvent::EVENT_FOCUS:
      [self postNotification:NSAccessibilityFocusedUIElementChangedNotification];
      break;
    case nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE:
      [self postNotification:NSAccessibilityFocusedUIElementChangedNotification];
      [self postNotification:@"AXLoadComplete"];
      [self postNotification:@"AXLayoutComplete"];
      break;
    case nsIAccessibleEvent::EVENT_MENUPOPUP_START:
      [self postNotification:@"AXMenuOpened"];
      break;
    case nsIAccessibleEvent::EVENT_MENUPOPUP_END:
      [self postNotification:@"AXMenuClosed"];
      break;
    case nsIAccessibleEvent::EVENT_SELECTION:
    case nsIAccessibleEvent::EVENT_SELECTION_ADD:
    case nsIAccessibleEvent::EVENT_SELECTION_REMOVE:
      [self postNotification:NSAccessibilitySelectedChildrenChangedNotification];
      break;
  }
}

- (void)postNotification:(NSString*)notification {
  // This sends events via nsIObserverService to be consumed by our mochitests.
  xpcAccessibleMacInterface::FireEvent(self, notification);

  if (gfxPlatform::IsHeadless()) {
    // Using a headless toolkit for tests and whatnot, posting accessibility
    // notification won't work.
    return;
  }

  NSAccessibilityPostNotification(GetObjectOrRepresentedView(self), notification);
}

- (NSWindow*)window {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  // Get a pointer to the native window (NSWindow) we reside in.
  NSWindow* nativeWindow = nil;
  DocAccessible* docAcc = nullptr;
  if (AccessibleWrap* accWrap = [self getGeckoAccessible]) {
    docAcc = accWrap->Document();
  } else if (ProxyAccessible* proxy = [self getProxyAccessible]) {
    Accessible* outerDoc = proxy->OuterDocOfRemoteBrowser();
    if (outerDoc) docAcc = outerDoc->Document();
  }

  if (docAcc) nativeWindow = static_cast<NSWindow*>(docAcc->GetNativeWindow());

  NSAssert1(nativeWindow, @"Could not get native window for %@", self);
  return nativeWindow;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)invalidateChildren {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // make room for new children
  [mChildren release];
  mChildren = nil;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)appendChild:(Accessible*)aAccessible {
  // if mChildren is nil, then we don't even need to bother
  if (!mChildren) return;

  mozAccessible* curNative = GetNativeFromGeckoAccessible(aAccessible);
  if (curNative) [mChildren addObject:curNative];
}

- (BOOL)accessibilityNotifiesWhenDestroyed {
  return YES;
}

- (void)expire {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [self invalidateChildren];

  mGeckoAccessible = 0;

  [self postNotification:NSAccessibilityUIElementDestroyedNotification];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (BOOL)isExpired {
  return ![self getGeckoAccessible] && ![self getProxyAccessible];
}

#pragma mark -
#pragma mark Debug methods
#pragma mark -

#ifdef DEBUG

// will check that our children actually reference us as their
// parent.
- (void)sanityCheckChildren:(NSArray*)children {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSEnumerator* iter = [children objectEnumerator];
  mozAccessible* curObj = nil;

  NSLog(@"sanity checking %@", self);

  while ((curObj = [iter nextObject])) {
    id realSelf = GetObjectOrRepresentedView(self);
    NSLog(@"checking %@", realSelf);
    NSAssert2([curObj parent] == realSelf,
              @"!!! %@ not returning %@ as AXParent, even though it is a AXChild of it!", curObj,
              realSelf);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)sanityCheckChildren {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [self sanityCheckChildren:[self children]];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)printHierarchy {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [self printHierarchyWithLevel:0];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)printHierarchyWithLevel:(unsigned)level {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSAssert(![self isExpired], @"!!! trying to print hierarchy of expired object!");

  // print this node
  NSMutableString* indent = [NSMutableString stringWithCapacity:level];
  unsigned i = 0;
  for (; i < level; i++) [indent appendString:@" "];

  NSLog(@"%@(#%i) %@", indent, level, self);

  // use |children| method to make sure our children are lazily fetched first.
  NSArray* children = [self children];
  if (!children) return;

  [self sanityCheckChildren];

  NSEnumerator* iter = [children objectEnumerator];
  mozAccessible* object = nil;

  while (iter && (object = [iter nextObject]))
    // print every child node's subtree, increasing the indenting
    // by two for every level.
    [object printHierarchyWithLevel:(level + 1)];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

#endif /* DEBUG */

@end
