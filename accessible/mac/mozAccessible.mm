/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozAccessible.h"

#import "MacUtils.h"
#import "mozView.h"

#include "Accessible-inl.h"
#include "nsAccUtils.h"
#include "nsIPersistentProperties2.h"
#include "DocAccessibleParent.h"
#include "Relation.h"
#include "Role.h"
#include "RootAccessible.h"
#include "TableAccessible.h"
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

#pragma mark -

@interface mozAccessible ()
- (BOOL)providesLabelNotTitle;
@end

@implementation mozAccessible

- (id)initWithAccessible:(AccessibleOrProxy)aAccOrProxy {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  MOZ_ASSERT(!aAccOrProxy.IsNull(), "Cannot init mozAccessible with null");
  if ((self = [super init])) {
    mGeckoAccessible = aAccOrProxy;
    mRole = aAccOrProxy.Role();
  }

  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)dealloc {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

#pragma mark - mozAccessible widget

- (BOOL)hasRepresentedView {
  return NO;
}

- (id)representedView {
  return nil;
}

- (BOOL)isRoot {
  return NO;
}

#pragma mark -

- (BOOL)moxIgnoreWithParent:(mozAccessible*)parent {
  if (Accessible* acc = mGeckoAccessible.AsAccessible()) {
    if (acc->IsContent() && acc->GetContent()->IsXULElement()) {
      if (acc->VisibilityState() & states::INVISIBLE) {
        return YES;
      }
    }
  }

  return [parent moxIgnoreChild:self];
}

- (BOOL)moxIgnoreChild:(mozAccessible*)child {
  return NO;
}

- (id)childAt:(uint32_t)i {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  AccessibleOrProxy child = mGeckoAccessible.ChildAt(i);
  return !child.IsNull() ? GetNativeFromGeckoAccessible(child) : nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

static const uint64_t kCachedStates =
    states::CHECKED | states::PRESSED | states::MIXED | states::EXPANDED |
    states::CURRENT | states::SELECTED | states::TRAVERSED | states::LINKED |
    states::HASPOPUP;
static const uint64_t kCacheInitialized = ((uint64_t)0x1) << 63;

- (uint64_t)state {
  uint64_t state = 0;

  if (Accessible* acc = mGeckoAccessible.AsAccessible()) {
    state = acc->State();
  }

  if (ProxyAccessible* proxy = mGeckoAccessible.AsProxy()) {
    state = proxy->State();
  }

  if (!(mCachedState & kCacheInitialized)) {
    mCachedState = state & kCachedStates;
    mCachedState |= kCacheInitialized;
  }

  return state;
}

- (uint64_t)stateWithMask:(uint64_t)mask {
  if ((mask & kCachedStates) == mask &&
      (mCachedState & kCacheInitialized) != 0) {
    return mCachedState & mask;
  }

  return [self state] & mask;
}

- (void)stateChanged:(uint64_t)state isEnabled:(BOOL)enabled {
  if ((state & kCachedStates) == 0) {
    return;
  }

  if (!(mCachedState & kCacheInitialized)) {
    [self state];
    return;
  }

  if (enabled) {
    mCachedState |= state;
  } else {
    mCachedState &= ~state;
  }
}

- (void)invalidateState {
  mCachedState = 0;
}

- (BOOL)providesLabelNotTitle {
  // These accessible types are the exception to the rule of label vs. title:
  // They may be named explicitly, but they still provide a label not a title.
  return mRole == roles::GROUPING || mRole == roles::RADIO_GROUP ||
         mRole == roles::FIGURE || mRole == roles::GRAPHIC ||
         mRole == roles::DOCUMENT;
}

- (mozilla::a11y::AccessibleOrProxy)geckoAccessible {
  return mGeckoAccessible;
}

- (mozilla::a11y::AccessibleOrProxy)geckoDocument {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  if (mGeckoAccessible.IsAccessible()) {
    if (mGeckoAccessible.AsAccessible()->IsDoc()) {
      return mGeckoAccessible;
    }
    return mGeckoAccessible.AsAccessible()->Document();
  }

  if (mGeckoAccessible.AsProxy()->IsDoc()) {
    return mGeckoAccessible;
  }

  return mGeckoAccessible.AsProxy()->Document();
}

#pragma mark - MOXAccessible protocol

- (BOOL)moxBlockSelector:(SEL)selector {
  if (selector == @selector(moxPerformPress)) {
    uint8_t actionCount = mGeckoAccessible.IsAccessible()
                              ? mGeckoAccessible.AsAccessible()->ActionCount()
                              : mGeckoAccessible.AsProxy()->ActionCount();

    // If we have no action, we don't support press, so return YES.
    return actionCount == 0;
  }

  if (selector == @selector(moxSetFocused:)) {
    return [self stateWithMask:states::FOCUSABLE] == 0;
  }

  return [super moxBlockSelector:selector];
}

- (id)moxFocusedUIElement {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  Accessible* acc = mGeckoAccessible.AsAccessible();
  ProxyAccessible* proxy = mGeckoAccessible.AsProxy();

  mozAccessible* focusedChild = nil;
  if (acc) {
    Accessible* focusedGeckoChild = acc->FocusedChild();
    if (focusedGeckoChild) {
      focusedChild = GetNativeFromGeckoAccessible(focusedGeckoChild);
    } else {
      dom::BrowserParent* browser = dom::BrowserParent::GetFocused();
      if (browser) {
        a11y::DocAccessibleParent* proxyDoc =
            browser->GetTopLevelDocAccessible();
        if (proxyDoc) {
          mozAccessible* nativeRemoteChild =
              GetNativeFromGeckoAccessible(proxyDoc);
          return [nativeRemoteChild accessibilityFocusedUIElement];
        }
      }
    }
  } else if (proxy) {
    ProxyAccessible* focusedGeckoChild = proxy->FocusedChild();
    if (focusedGeckoChild) {
      focusedChild = GetNativeFromGeckoAccessible(focusedGeckoChild);
    }
  }

  if ([focusedChild isAccessibilityElement]) {
    return focusedChild;
  }

  // return ourself if we can't get a native focused child.
  return self;
}

- (id<MOXTextMarkerSupport>)moxTextMarkerDelegate {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  if (mGeckoAccessible.IsAccessible()) {
    return [MOXTextMarkerDelegate
        getOrCreateForDoc:mGeckoAccessible.AsAccessible()->Document()];
  }

  return [MOXTextMarkerDelegate
      getOrCreateForDoc:mGeckoAccessible.AsProxy()->Document()];
}

- (id)moxHitTest:(NSPoint)point {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  // Convert the given screen-global point in the cocoa coordinate system (with
  // origin in the bottom-left corner of the screen) into point in the Gecko
  // coordinate system (with origin in a top-left screen point).
  NSScreen* mainView = [[NSScreen screens] objectAtIndex:0];
  NSPoint tmpPoint =
      NSMakePoint(point.x, [mainView frame].size.height - point.y);
  LayoutDeviceIntPoint geckoPoint = nsCocoaUtils::CocoaPointsToDevPixels(
      tmpPoint, nsCocoaUtils::GetBackingScaleFactor(mainView));

  AccessibleOrProxy child = mGeckoAccessible.ChildAtPoint(
      geckoPoint.x, geckoPoint.y, Accessible::eDeepestChild);

  if (!child.IsNull()) {
    mozAccessible* nativeChild = GetNativeFromGeckoAccessible(child);
    return [nativeChild isAccessibilityElement]
               ? nativeChild
               : [nativeChild moxUnignoredParent];
  }

  // if we didn't find anything, return ourself or child view.
  return self;
}

- (id<mozAccessible>)moxParent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;
  if ([self isExpired]) {
    return nil;
  }

  AccessibleOrProxy parent = mGeckoAccessible.Parent();

  if (parent.IsNull()) {
    return nil;
  }

  id nativeParent = GetNativeFromGeckoAccessible(parent);
  if (!nativeParent && mGeckoAccessible.IsAccessible()) {
    // Return native of root accessible if we have no direct parent.
    // XXX: need to return a sensible fallback in proxy case as well
    nativeParent = GetNativeFromGeckoAccessible(
        mGeckoAccessible.AsAccessible()->RootAccessible());
  }

  return GetObjectOrRepresentedView(nativeParent);

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

// gets all our native children lazily, including those that are ignored.
- (NSArray*)moxChildren {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  NSMutableArray* children =
      [[NSMutableArray alloc] initWithCapacity:mGeckoAccessible.ChildCount()];

  for (uint32_t childIdx = 0; childIdx < mGeckoAccessible.ChildCount();
       childIdx++) {
    AccessibleOrProxy child = mGeckoAccessible.ChildAt(childIdx);
    mozAccessible* nativeChild = GetNativeFromGeckoAccessible(child);
    if (!nativeChild) {
      continue;
    }

    [children addObject:nativeChild];
  }

  return children;
}

- (NSValue*)moxPosition {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  nsIntRect rect = mGeckoAccessible.IsAccessible()
                       ? mGeckoAccessible.AsAccessible()->Bounds()
                       : mGeckoAccessible.AsProxy()->Bounds();

  NSScreen* mainView = [[NSScreen screens] objectAtIndex:0];
  CGFloat scaleFactor = nsCocoaUtils::GetBackingScaleFactor(mainView);
  NSPoint p =
      NSMakePoint(static_cast<CGFloat>(rect.x) / scaleFactor,
                  [mainView frame].size.height -
                      static_cast<CGFloat>(rect.y + rect.height) / scaleFactor);

  return [NSValue valueWithPoint:p];
}

- (NSValue*)moxSize {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  nsIntRect rect = mGeckoAccessible.IsAccessible()
                       ? mGeckoAccessible.AsAccessible()->Bounds()
                       : mGeckoAccessible.AsProxy()->Bounds();

  CGFloat scaleFactor =
      nsCocoaUtils::GetBackingScaleFactor([[NSScreen screens] objectAtIndex:0]);
  return [NSValue
      valueWithSize:NSMakeSize(
                        static_cast<CGFloat>(rect.width) / scaleFactor,
                        static_cast<CGFloat>(rect.height) / scaleFactor)];
}

- (NSString*)moxRole {
#define ROLE(geckoRole, stringRole, atkRole, macRole, macSubrole, msaaRole, \
             ia2Role, androidClass, nameRule)                               \
  case roles::geckoRole:                                                    \
    return macRole;

  switch (mRole) {
#include "RoleMap.h"
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown role.");
      return NSAccessibilityUnknownRole;
  }

#undef ROLE
}

- (NSString*)moxSubrole {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  Accessible* acc = mGeckoAccessible.AsAccessible();
  ProxyAccessible* proxy = mGeckoAccessible.AsProxy();

  // Deal with landmarks first
  // macOS groups the specific landmark types of DPub ARIA into two broad
  // categories with corresponding subroles: Navigation and region/container.
  if (mRole == roles::LANDMARK) {
    nsAtom* landmark = acc ? acc->LandmarkRole() : proxy->LandmarkRole();
    // HTML Elements treated as landmarks, and ARIA landmarks.
    if (landmark) {
      if (landmark == nsGkAtoms::banner) return @"AXLandmarkBanner";
      if (landmark == nsGkAtoms::complementary)
        return @"AXLandmarkComplementary";
      if (landmark == nsGkAtoms::contentinfo) return @"AXLandmarkContentInfo";
      if (landmark == nsGkAtoms::main) return @"AXLandmarkMain";
      if (landmark == nsGkAtoms::navigation) return @"AXLandmarkNavigation";
      if (landmark == nsGkAtoms::search) return @"AXLandmarkSearch";
    }

    // None of the above, so assume DPub ARIA.
    return @"AXLandmarkRegion";
  }

  // Now, deal with widget roles
  nsStaticAtom* roleAtom = nullptr;

  if (mRole == roles::DIALOG) {
    if (acc && acc->HasARIARole()) {
      const nsRoleMapEntry* roleMap = acc->ARIARoleMap();
      roleAtom = roleMap->roleAtom;
    } else if (proxy) {
      roleAtom = proxy->ARIARoleAtom();
    }

    if (roleAtom) {
      if (roleAtom == nsGkAtoms::alertdialog) {
        return @"AXApplicationAlertDialog";
      }
      if (roleAtom == nsGkAtoms::dialog) {
        return @"AXApplicationDialog";
      }
    }
  }

  if (mRole == roles::FORM) {
    // This only gets exposed as a landmark if the role comes from ARIA.
    if (acc && acc->HasARIARole()) {
      const nsRoleMapEntry* roleMap = acc->ARIARoleMap();
      roleAtom = roleMap->roleAtom;
    } else if (proxy) {
      roleAtom = proxy->ARIARoleAtom();
    }

    if (roleAtom && roleAtom == nsGkAtoms::form) {
      return @"AXLandmarkForm";
    }
  }

#define ROLE(geckoRole, stringRole, atkRole, macRole, macSubrole, msaaRole, \
             ia2Role, androidClass, nameRule)                               \
  case roles::geckoRole:                                                    \
    if (![macSubrole isEqualToString:NSAccessibilityUnknownSubrole]) {      \
      return macSubrole;                                                    \
    } else {                                                                \
      break;                                                                \
    }

  switch (mRole) {
#include "RoleMap.h"
  }

  // These are special. They map to roles::NOTHING
  // and are instructed by the ARIA map to use the native host role.
  if (acc && acc->HasARIARole()) {
    const nsRoleMapEntry* roleMap = acc->ARIARoleMap();
    roleAtom = roleMap->roleAtom;
  }
  if (proxy) roleAtom = proxy->ARIARoleAtom();

  if (roleAtom) {
    if (roleAtom == nsGkAtoms::log_) return @"AXApplicationLog";
    if (roleAtom == nsGkAtoms::timer) return @"AXApplicationTimer";
    // macOS added an AXSubrole value to distinguish generic AXGroup objects
    // from those which are AXGroups as a result of an explicit ARIA role,
    // such as the non-landmark, non-listitem text containers in DPub ARIA.
    if (mRole == roles::FOOTNOTE || mRole == roles::SECTION) {
      return @"AXApplicationGroup";
    }
  }

  return NSAccessibilityUnknownSubrole;

#undef ROLE
}

struct RoleDescrMap {
  NSString* role;
  const nsString description;
};

static const RoleDescrMap sRoleDescrMap[] = {
    {@"AXApplicationAlert", u"alert"_ns},
    {@"AXApplicationAlertDialog", u"alertDialog"_ns},
    {@"AXApplicationDialog", u"dialog"_ns},
    {@"AXApplicationLog", u"log"_ns},
    {@"AXApplicationMarquee", u"marquee"_ns},
    {@"AXApplicationStatus", u"status"_ns},
    {@"AXApplicationTimer", u"timer"_ns},
    {@"AXContentSeparator", u"separator"_ns},
    {@"AXDefinition", u"definition"_ns},
    {@"AXDetails", u"details"_ns},
    {@"AXDocument", u"document"_ns},
    {@"AXDocumentArticle", u"article"_ns},
    {@"AXDocumentMath", u"math"_ns},
    {@"AXDocumentNote", u"note"_ns},
    {@"AXLandmarkApplication", u"application"_ns},
    {@"AXLandmarkBanner", u"banner"_ns},
    {@"AXLandmarkComplementary", u"complementary"_ns},
    {@"AXLandmarkContentInfo", u"content"_ns},
    {@"AXLandmarkMain", u"main"_ns},
    {@"AXLandmarkNavigation", u"navigation"_ns},
    {@"AXLandmarkRegion", u"region"_ns},
    {@"AXLandmarkSearch", u"search"_ns},
    {@"AXSearchField", u"searchTextField"_ns},
    {@"AXSummary", u"summary"_ns},
    {@"AXTabPanel", u"tabPanel"_ns},
    {@"AXTerm", u"term"_ns},
    {@"AXUserInterfaceTooltip", u"tooltip"_ns}};

struct RoleDescrComparator {
  const NSString* mRole;
  explicit RoleDescrComparator(const NSString* aRole) : mRole(aRole) {}
  int operator()(const RoleDescrMap& aEntry) const {
    return [mRole compare:aEntry.role];
  }
};

- (NSString*)moxRoleDescription {
  if (mRole == roles::DOCUMENT)
    return utils::LocalizedString(u"htmlContent"_ns);

  if (mRole == roles::FIGURE) return utils::LocalizedString(u"figure"_ns);

  if (mRole == roles::HEADING) return utils::LocalizedString(u"heading"_ns);

  if (mRole == roles::MARK) {
    return utils::LocalizedString(u"highlight"_ns);
  }

  NSString* subrole = [self moxSubrole];

  if (subrole) {
    size_t idx = 0;
    if (BinarySearchIf(sRoleDescrMap, 0, ArrayLength(sRoleDescrMap),
                       RoleDescrComparator(subrole), &idx)) {
      return utils::LocalizedString(sRoleDescrMap[idx].description);
    }
  }

  return NSAccessibilityRoleDescription([self moxRole], subrole);
}

- (NSString*)moxLabel {
  if ([self isExpired]) {
    return nil;
  }

  Accessible* acc = mGeckoAccessible.AsAccessible();
  ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
  nsAutoString name;

  /* If our accessible is:
   * 1. Named by invisible text, or
   * 2. Has more than one labeling relation, or
   * 3. Is a special role defined in providesLabelNotTitle
   *   ... return its name as a label (AXDescription).
   */
  if (acc) {
    ENameValueFlag flag = acc->Name(name);
    if (flag == eNameFromSubtree) {
      return nil;
    }

    if (![self providesLabelNotTitle]) {
      Relation rel = acc->RelationByType(RelationType::LABELLED_BY);
      if (rel.Next() && !rel.Next()) {
        return nil;
      }
    }
  } else if (proxy) {
    uint32_t flag = proxy->Name(name);
    if (flag == eNameFromSubtree) {
      return nil;
    }

    if (![self providesLabelNotTitle]) {
      nsTArray<ProxyAccessible*> rels =
          proxy->RelationByType(RelationType::LABELLED_BY);
      if (rels.Length() == 1) {
        return nil;
      }
    }
  }

  return nsCocoaUtils::ToNSString(name);
}

- (NSString*)moxTitle {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  // In some special cases we provide the name in the label (AXDescription).
  if ([self providesLabelNotTitle]) {
    return nil;
  }

  nsAutoString title;
  if (Accessible* acc = mGeckoAccessible.AsAccessible()) {
    acc->Name(title);
  } else {
    mGeckoAccessible.AsProxy()->Name(title);
  }

  return nsCocoaUtils::ToNSString(title);

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id)moxValue {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsAutoString value;
  if (Accessible* acc = mGeckoAccessible.AsAccessible()) {
    acc->Value(value);
  } else {
    mGeckoAccessible.AsProxy()->Value(value);
  }

  return nsCocoaUtils::ToNSString(value);

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSString*)moxHelp {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  // What needs to go here is actually the accDescription of an item.
  // The MSAA acc_help method has nothing to do with this one.
  nsAutoString helpText;
  if (Accessible* acc = mGeckoAccessible.AsAccessible()) {
    acc->Description(helpText);
  } else {
    mGeckoAccessible.AsProxy()->Description(helpText);
  }

  return nsCocoaUtils::ToNSString(helpText);

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSWindow*)moxWindow {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  // Get a pointer to the native window (NSWindow) we reside in.
  NSWindow* nativeWindow = nil;
  DocAccessible* docAcc = nullptr;
  if (Accessible* acc = mGeckoAccessible.AsAccessible()) {
    docAcc = acc->Document();
  } else {
    ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
    Accessible* outerDoc = proxy->OuterDocOfRemoteBrowser();
    if (outerDoc) docAcc = outerDoc->Document();
  }

  if (docAcc) nativeWindow = static_cast<NSWindow*>(docAcc->GetNativeWindow());

  MOZ_ASSERT(nativeWindow, "Couldn't get native window");
  return nativeWindow;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSNumber*)moxEnabled {
  if ([self stateWithMask:states::UNAVAILABLE]) {
    return @NO;
  }

  if (![self isRoot]) {
    mozAccessible* parent = (mozAccessible*)[self moxUnignoredParent];
    if (![parent isRoot]) {
      return @(![parent disableChild:self]);
    }
  }

  return @YES;
}

- (NSNumber*)moxFocused {
  return @([self stateWithMask:states::FOCUSED] != 0);
}

- (NSNumber*)moxSelected {
  return @NO;
}

- (NSString*)moxARIACurrent {
  if (![self stateWithMask:states::CURRENT]) {
    return nil;
  }

  return utils::GetAccAttr(self, "current");
}

- (id)moxTitleUIElement {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  if (Accessible* acc = mGeckoAccessible.AsAccessible()) {
    Relation rel = acc->RelationByType(RelationType::LABELLED_BY);
    Accessible* tempAcc = rel.Next();
    if (tempAcc && !rel.Next()) {
      mozAccessible* label = GetNativeFromGeckoAccessible(tempAcc);
      return [label isAccessibilityElement] ? label : nil;
    }

    return nil;
  }

  ProxyAccessible* proxy = mGeckoAccessible.AsProxy();
  nsTArray<ProxyAccessible*> rel =
      proxy->RelationByType(RelationType::LABELLED_BY);
  ProxyAccessible* tempProxy = rel.SafeElementAt(0);
  if (tempProxy && rel.Length() <= 1) {
    mozAccessible* label = GetNativeFromGeckoAccessible(tempProxy);
    return [label isAccessibilityElement] ? label : nil;
  }

  return nil;
}

- (NSString*)moxDOMIdentifier {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  nsAutoString id;
  if (Accessible* acc = mGeckoAccessible.AsAccessible()) {
    if (acc->GetContent()) {
      nsCoreUtils::GetID(acc->GetContent(), id);
    }
  } else {
    mGeckoAccessible.AsProxy()->DOMNodeID(id);
  }

  return nsCocoaUtils::ToNSString(id);
}

- (NSNumber*)moxRequired {
  return @([self stateWithMask:states::REQUIRED] != 0);
}

- (void)moxSetFocused:(NSNumber*)focused {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  if ([focused boolValue]) {
    if (mGeckoAccessible.IsAccessible()) {
      mGeckoAccessible.AsAccessible()->TakeFocus();
    } else {
      mGeckoAccessible.AsProxy()->TakeFocus();
    }
  }
}

- (void)moxPerformScrollToVisible {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  if (mGeckoAccessible.IsAccessible()) {
    // Need strong ref because of MOZ_CAN_RUN_SCRIPT
    RefPtr<Accessible> acc = mGeckoAccessible.AsAccessible();
    acc->ScrollTo(nsIAccessibleScrollType::SCROLL_TYPE_ANYWHERE);
  } else {
    mGeckoAccessible.AsProxy()->ScrollTo(
        nsIAccessibleScrollType::SCROLL_TYPE_ANYWHERE);
  }
}

- (void)moxPerformShowMenu {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  nsIntRect bounds = mGeckoAccessible.IsAccessible()
                         ? mGeckoAccessible.AsAccessible()->Bounds()
                         : mGeckoAccessible.AsProxy()->Bounds();
  // We don't need to convert this rect into mac coordinates because the
  // mouse event synthesizer expects layout (gecko) coordinates.
  LayoutDeviceIntRect geckoRect = LayoutDeviceIntRect::FromUnknownRect(bounds);

  Accessible* rootAcc = mGeckoAccessible.IsAccessible()
                            ? mGeckoAccessible.AsAccessible()->RootAccessible()
                            : mGeckoAccessible.AsProxy()
                                  ->OuterDocOfRemoteBrowser()
                                  ->RootAccessible();
  id objOrView =
      GetObjectOrRepresentedView(GetNativeFromGeckoAccessible(rootAcc));

  LayoutDeviceIntPoint p =
      LayoutDeviceIntPoint(geckoRect.X() + (geckoRect.Width() / 2),
                           geckoRect.Y() + (geckoRect.Height() / 2));
  nsIWidget* widget = [objOrView widget];
  // XXX: NSRightMouseDown is depreciated in 10.12, should be
  // changed to NSEventTypeRightMouseDown after refactoring.
  widget->SynthesizeNativeMouseEvent(p, NSRightMouseDown, 0, nullptr);
}

- (void)moxPerformPress {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  if (mGeckoAccessible.IsAccessible()) {
    mGeckoAccessible.AsAccessible()->DoAction(0);
  } else {
    mGeckoAccessible.AsProxy()->DoAction(0);
  }

  // Activating accessible may alter its state.
  [self invalidateState];
}

#pragma mark -

// objc-style description (from NSObject); not to be confused with the
// accessible description above.
- (NSString*)description {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [NSString stringWithFormat:@"(%p) %@", self, [self moxRole]];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)disableChild:(mozAccessible*)child {
  return NO;
}

- (void)handleAccessibleTextChangeEvent:(NSString*)change
                               inserted:(BOOL)isInserted
                            inContainer:(const AccessibleOrProxy&)container
                                     at:(int32_t)start {
  // XXX: Eventually live region handling will go here.
}

- (void)handleAccessibleEvent:(uint32_t)eventType {
  switch (eventType) {
    case nsIAccessibleEvent::EVENT_FOCUS:
      [self moxPostNotification:
                NSAccessibilityFocusedUIElementChangedNotification];
      break;
    case nsIAccessibleEvent::EVENT_MENUPOPUP_START:
      [self moxPostNotification:@"AXMenuOpened"];
      break;
    case nsIAccessibleEvent::EVENT_MENUPOPUP_END:
      [self moxPostNotification:@"AXMenuClosed"];
      break;
    case nsIAccessibleEvent::EVENT_SELECTION:
    case nsIAccessibleEvent::EVENT_SELECTION_ADD:
    case nsIAccessibleEvent::EVENT_SELECTION_REMOVE:
    case nsIAccessibleEvent::EVENT_SELECTION_WITHIN:
      [self moxPostNotification:
                NSAccessibilitySelectedChildrenChangedNotification];
      break;
    case nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED: {
      // We consider any caret move event to be a selected text change event.
      // So dispatching an event for EVENT_TEXT_SELECTION_CHANGED would be
      // reduntant.
      id<MOXTextMarkerSupport> delegate = [self moxTextMarkerDelegate];
      id selectedRange = [delegate moxSelectedTextMarkerRange];
      NSDictionary* userInfo = @{
        @"AXTextChangeElement" : self,
        @"AXSelectedTextMarkerRange" :
            (selectedRange ? selectedRange : [NSNull null])
      };

      mozAccessible* webArea =
          GetNativeFromGeckoAccessible([self geckoDocument]);
      [webArea
          moxPostNotification:NSAccessibilitySelectedTextChangedNotification
                 withUserInfo:userInfo];
      [self moxPostNotification:NSAccessibilitySelectedTextChangedNotification
                   withUserInfo:userInfo];
      break;
    }
  }
}

- (void)expire {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [self invalidateState];

  mGeckoAccessible.SetBits(0);

  [self moxPostNotification:NSAccessibilityUIElementDestroyedNotification];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (BOOL)isExpired {
  return !mGeckoAccessible.AsAccessible() && !mGeckoAccessible.AsProxy();
}

@end
