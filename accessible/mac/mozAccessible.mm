/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozAccessible.h"
#include "MOXAccessibleBase.h"

#import "MacUtils.h"
#import "mozView.h"
#import "MOXSearchInfo.h"
#import "mozTextAccessible.h"

#include "LocalAccessible-inl.h"
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

- (void)maybePostLiveRegionChanged;
@end

@implementation mozAccessible

- (id)initWithAccessible:(AccessibleOrProxy)aAccOrProxy {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;
  MOZ_ASSERT(!aAccOrProxy.IsNull(), "Cannot init mozAccessible with null");
  if ((self = [super init])) {
    mGeckoAccessible = aAccOrProxy;
    mRole = aAccOrProxy.Role();
  }

  return self;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (void)dealloc {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [super dealloc];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
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
  if (LocalAccessible* acc = mGeckoAccessible.AsAccessible()) {
    if (acc->IsContent() && acc->GetContent()->IsXULElement()) {
      if (acc->VisibilityState() & states::INVISIBLE) {
        return YES;
      }
    }
  }

  return [parent moxIgnoreChild:self];
}

- (BOOL)moxIgnoreChild:(mozAccessible*)child {
  return nsAccUtils::MustPrune(mGeckoAccessible);
}

- (id)childAt:(uint32_t)i {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  AccessibleOrProxy child = mGeckoAccessible.ChildAt(i);
  return !child.IsNull() ? GetNativeFromGeckoAccessible(child) : nil;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

static const uint64_t kCachedStates =
    states::CHECKED | states::PRESSED | states::MIXED | states::EXPANDED |
    states::CURRENT | states::SELECTED | states::TRAVERSED | states::LINKED |
    states::HASPOPUP | states::BUSY | states::MULTI_LINE;
static const uint64_t kCacheInitialized = ((uint64_t)0x1) << 63;

- (uint64_t)state {
  uint64_t state = 0;

  if (LocalAccessible* acc = mGeckoAccessible.AsAccessible()) {
    state = acc->State();
  }

  if (RemoteAccessible* proxy = mGeckoAccessible.AsProxy()) {
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
  if ((state & kCachedStates) != 0) {
    if (!(mCachedState & kCacheInitialized)) {
      [self state];
    } else {
      if (enabled) {
        mCachedState |= state;
      } else {
        mCachedState &= ~state;
      }
    }
  }

  if (state == states::BUSY) {
    [self moxPostNotification:@"AXElementBusyChanged"];
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
         mRole == roles::DOCUMENT || mRole == roles::OUTLINE;
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

  if (selector == @selector(moxARIALive) ||
      selector == @selector(moxARIAAtomic) ||
      selector == @selector(moxARIARelevant)) {
    return ![self moxIsLiveRegion];
  }

  return [super moxBlockSelector:selector];
}

- (id)moxFocusedUIElement {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  LocalAccessible* acc = mGeckoAccessible.AsAccessible();
  RemoteAccessible* proxy = mGeckoAccessible.AsProxy();

  mozAccessible* focusedChild = nil;
  if (acc) {
    LocalAccessible* focusedGeckoChild = acc->FocusedChild();
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
    RemoteAccessible* focusedGeckoChild = proxy->FocusedChild();
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

- (BOOL)moxIsLiveRegion {
  return mIsLiveRegion;
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
      geckoPoint.x, geckoPoint.y, Accessible::EWhichChildAtPoint::DeepestChild);

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
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;
  if ([self isExpired]) {
    return nil;
  }

  AccessibleOrProxy parent = mGeckoAccessible.Parent();

  if (parent.IsNull()) {
    return nil;
  }

  id nativeParent = GetNativeFromGeckoAccessible(parent);
  if (parent.Role() == roles::DOCUMENT &&
      [nativeParent respondsToSelector:@selector(rootGroup)]) {
    // Before returning a WebArea as parent, check to see if
    // there is a generated root group that is an intermediate container.
    if (id<mozAccessible> rootGroup = [nativeParent rootGroup]) {
      nativeParent = rootGroup;
    }
  }

  if (!nativeParent && mGeckoAccessible.IsAccessible()) {
    // Return native of root accessible if we have no direct parent.
    // XXX: need to return a sensible fallback in proxy case as well
    nativeParent = GetNativeFromGeckoAccessible(
        mGeckoAccessible.AsAccessible()->RootAccessible());
  }

  return GetObjectOrRepresentedView(nativeParent);

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
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
  CGRect frame = [[self moxFrame] rectValue];

  return [NSValue valueWithPoint:NSMakePoint(frame.origin.x, frame.origin.y)];
}

- (NSValue*)moxSize {
  CGRect frame = [[self moxFrame] rectValue];

  return
      [NSValue valueWithSize:NSMakeSize(frame.size.width, frame.size.height)];
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

- (nsStaticAtom*)ARIARole {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  if (LocalAccessible* acc = mGeckoAccessible.AsAccessible()) {
    if (acc->HasARIARole()) {
      const nsRoleMapEntry* roleMap = acc->ARIARoleMap();
      return roleMap->roleAtom;
    }

    return nsGkAtoms::_empty;
  }

  if (!mARIARole) {
    mARIARole = mGeckoAccessible.AsProxy()->ARIARoleAtom();
    if (!mARIARole) {
      mARIARole = nsGkAtoms::_empty;
    }
  }

  return mARIARole;
}

- (NSString*)moxSubrole {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  LocalAccessible* acc = mGeckoAccessible.AsAccessible();
  RemoteAccessible* proxy = mGeckoAccessible.AsProxy();

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
    roleAtom = [self ARIARole];

    if (roleAtom == nsGkAtoms::alertdialog) {
      return @"AXApplicationAlertDialog";
    }
    if (roleAtom == nsGkAtoms::dialog) {
      return @"AXApplicationDialog";
    }
  }

  if (mRole == roles::FORM) {
    roleAtom = [self ARIARole];

    if (roleAtom == nsGkAtoms::form) {
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
  roleAtom = [self ARIARole];

  if (roleAtom == nsGkAtoms::log_) {
    return @"AXApplicationLog";
  }

  if (roleAtom == nsGkAtoms::timer) {
    return @"AXApplicationTimer";
  }
  // macOS added an AXSubrole value to distinguish generic AXGroup objects
  // from those which are AXGroups as a result of an explicit ARIA role,
  // such as the non-landmark, non-listitem text containers in DPub ARIA.
  if (mRole == roles::FOOTNOTE || mRole == roles::SECTION) {
    return @"AXApplicationGroup";
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
  if (NSString* ariaRoleDescription =
          utils::GetAccAttr(self, "roledescription")) {
    if ([ariaRoleDescription length]) {
      return ariaRoleDescription;
    }
  }

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

  LocalAccessible* acc = mGeckoAccessible.AsAccessible();
  RemoteAccessible* proxy = mGeckoAccessible.AsProxy();
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
  } else if (proxy) {
    uint32_t flag = proxy->Name(name);
    if (flag == eNameFromSubtree) {
      return nil;
    }
  }

  if (![self providesLabelNotTitle]) {
    NSArray* relations = [self getRelationsByType:RelationType::LABELLED_BY];
    if ([relations count] == 1) {
      return nil;
    }
  }

  return nsCocoaUtils::ToNSString(name);
}

- (NSString*)moxTitle {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // In some special cases we provide the name in the label (AXDescription).
  if ([self providesLabelNotTitle]) {
    return nil;
  }

  nsAutoString title;
  if (LocalAccessible* acc = mGeckoAccessible.AsAccessible()) {
    acc->Name(title);
  } else {
    mGeckoAccessible.AsProxy()->Name(title);
  }

  return nsCocoaUtils::ToNSString(title);

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (id)moxValue {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  nsAutoString value;
  if (LocalAccessible* acc = mGeckoAccessible.AsAccessible()) {
    acc->Value(value);
  } else {
    mGeckoAccessible.AsProxy()->Value(value);
  }

  return nsCocoaUtils::ToNSString(value);

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (NSString*)moxHelp {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // What needs to go here is actually the accDescription of an item.
  // The MSAA acc_help method has nothing to do with this one.
  nsAutoString helpText;
  if (LocalAccessible* acc = mGeckoAccessible.AsAccessible()) {
    acc->Description(helpText);
  } else {
    mGeckoAccessible.AsProxy()->Description(helpText);
  }

  return nsCocoaUtils::ToNSString(helpText);

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (NSWindow*)moxWindow {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // Get a pointer to the native window (NSWindow) we reside in.
  NSWindow* nativeWindow = nil;
  DocAccessible* docAcc = nullptr;
  if (LocalAccessible* acc = mGeckoAccessible.AsAccessible()) {
    docAcc = acc->Document();
  } else {
    RemoteAccessible* proxy = mGeckoAccessible.AsProxy();
    LocalAccessible* outerDoc = proxy->OuterDocOfRemoteBrowser();
    if (outerDoc) docAcc = outerDoc->Document();
  }

  if (docAcc) nativeWindow = static_cast<NSWindow*>(docAcc->GetNativeWindow());

  MOZ_ASSERT(nativeWindow, "Couldn't get native window");
  return nativeWindow;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
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

- (NSValue*)moxFrame {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  nsIntRect rect = mGeckoAccessible.IsAccessible()
                       ? mGeckoAccessible.AsAccessible()->Bounds()
                       : mGeckoAccessible.AsProxy()->Bounds();
  NSScreen* mainView = [[NSScreen screens] objectAtIndex:0];
  CGFloat scaleFactor = nsCocoaUtils::GetBackingScaleFactor(mainView);

  return [NSValue
      valueWithRect:NSMakeRect(
                        static_cast<CGFloat>(rect.x) / scaleFactor,
                        [mainView frame].size.height -
                            static_cast<CGFloat>(rect.y + rect.height) /
                                scaleFactor,
                        static_cast<CGFloat>(rect.width) / scaleFactor,
                        static_cast<CGFloat>(rect.height) / scaleFactor)];
}

- (NSString*)moxARIACurrent {
  if (![self stateWithMask:states::CURRENT]) {
    return nil;
  }

  return utils::GetAccAttr(self, "current");
}

- (NSNumber*)moxARIAAtomic {
  return @(utils::GetAccAttr(self, "atomic") != nil);
}

- (NSString*)moxARIALive {
  return utils::GetAccAttr(self, "live");
}

- (NSString*)moxARIARelevant {
  if (NSString* relevant = utils::GetAccAttr(self, "container-relevant")) {
    return relevant;
  }

  // Default aria-relevant value
  return @"additions text";
}

- (id)moxTitleUIElement {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  NSArray* relations = [self getRelationsByType:RelationType::LABELLED_BY];
  if ([relations count] == 1) {
    return [relations firstObject];
  }

  return nil;
}

- (NSString*)moxDOMIdentifier {
  MOZ_ASSERT(!mGeckoAccessible.IsNull());

  nsAutoString id;
  if (LocalAccessible* acc = mGeckoAccessible.AsAccessible()) {
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

- (NSNumber*)moxElementBusy {
  return @([self stateWithMask:states::BUSY] != 0);
}

- (NSArray*)moxLinkedUIElements {
  return [self getRelationsByType:RelationType::FLOWS_TO];
}

- (NSArray*)moxARIAControls {
  return [self getRelationsByType:RelationType::CONTROLLER_FOR];
}

- (mozAccessible*)topWebArea {
  AccessibleOrProxy doc = [self geckoDocument];
  while (!doc.IsNull()) {
    if (doc.IsAccessible()) {
      DocAccessible* docAcc = doc.AsAccessible()->AsDoc();
      if (docAcc->DocumentNode()->GetBrowsingContext()->IsTopContent()) {
        return GetNativeFromGeckoAccessible(docAcc);
      }

      doc = docAcc->ParentDocument();
    } else {
      DocAccessibleParent* docProxy = doc.AsProxy()->AsDoc();
      if (docProxy->IsTopLevel()) {
        return GetNativeFromGeckoAccessible(docProxy);
      }
      doc = docProxy->ParentDoc();
    }
  }

  return nil;
}

- (void)handleRoleChanged:(mozilla::a11y::role)newRole {
  mRole = newRole;
  mARIARole = nullptr;

  // For testing purposes
  [self moxPostNotification:@"AXMozRoleChanged"];
}

- (id)moxEditableAncestor {
  return [self moxFindAncestor:^BOOL(id moxAcc, BOOL* stop) {
    return [moxAcc isKindOfClass:[mozTextAccessible class]];
  }];
}

- (id)moxHighestEditableAncestor {
  id highestAncestor = [self moxEditableAncestor];
  while ([highestAncestor conformsToProtocol:@protocol(MOXAccessible)]) {
    id ancestorParent = [highestAncestor moxUnignoredParent];
    if (![ancestorParent conformsToProtocol:@protocol(MOXAccessible)]) {
      break;
    }

    id higherAncestor = [ancestorParent moxEditableAncestor];

    if (!higherAncestor) {
      break;
    }

    highestAncestor = higherAncestor;
  }

  return highestAncestor;
}

- (id)moxFocusableAncestor {
  // XXX: Checking focusable state up the chain can be expensive. For now,
  // we can just return AXEditableAncestor since the main use case for this
  // is rich text editing with links.
  return [self moxEditableAncestor];
}

#ifndef RELEASE_OR_BETA
- (NSString*)moxMozDebugDescription {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NSMutableString* domInfo = [NSMutableString string];
  if (NSString* tagName = utils::GetAccAttr(self, "tag")) {
    [domInfo appendFormat:@" %@", tagName];
    NSString* domID = [self moxDOMIdentifier];
    if ([domID length]) {
      [domInfo appendFormat:@"#%@", domID];
    }
    if (NSString* className = utils::GetAccAttr(self, "class")) {
      [domInfo
          appendFormat:@".%@",
                       [className stringByReplacingOccurrencesOfString:@" "
                                                            withString:@"."]];
    }
  }

  return [NSString stringWithFormat:@"<%@: %p %@%@>",
                                    NSStringFromClass([self class]), self,
                                    [self moxRole], domInfo];

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}
#endif

- (NSArray*)moxUIElementsForSearchPredicate:(NSDictionary*)searchPredicate {
  // Create our search object and set it up with the searchPredicate
  // params. The init function does additional parsing. We pass a
  // reference to the web area to use as a start element if one is not
  // specified.
  MOXSearchInfo* search =
      [[MOXSearchInfo alloc] initWithParameters:searchPredicate andRoot:self];

  return [search performSearch];
}

- (NSNumber*)moxUIElementCountForSearchPredicate:
    (NSDictionary*)searchPredicate {
  return [NSNumber
      numberWithDouble:[[self moxUIElementsForSearchPredicate:searchPredicate]
                           count]];
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
    RefPtr<LocalAccessible> acc = mGeckoAccessible.AsAccessible();
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

  LocalAccessible* rootAcc =
      mGeckoAccessible.IsAccessible()
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
  widget->SynthesizeNativeMouseEvent(
      p, nsIWidget::NativeMouseMessage::ButtonDown, MouseButton::eSecondary,
      nsIWidget::Modifiers::NO_MODIFIERS, nullptr);
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

- (BOOL)disableChild:(mozAccessible*)child {
  return NO;
}

- (void)maybePostLiveRegionChanged {
  id<MOXAccessible> liveRegion =
      [self moxFindAncestor:^BOOL(id<MOXAccessible> moxAcc, BOOL* stop) {
        return [moxAcc moxIsLiveRegion];
      }];

  if (liveRegion) {
    [liveRegion moxPostNotification:@"AXLiveRegionChanged"];
  }
}

- (NSArray<mozAccessible*>*)getRelationsByType:(RelationType)relationType {
  if (LocalAccessible* acc = mGeckoAccessible.AsAccessible()) {
    NSMutableArray<mozAccessible*>* relations = [[NSMutableArray alloc] init];
    Relation rel = acc->RelationByType(relationType);
    while (LocalAccessible* relAcc = rel.Next()) {
      if (mozAccessible* relNative = GetNativeFromGeckoAccessible(relAcc)) {
        [relations addObject:relNative];
      }
    }

    return relations;
  }

  RemoteAccessible* proxy = mGeckoAccessible.AsProxy();
  nsTArray<RemoteAccessible*> rel = proxy->RelationByType(relationType);
  return utils::ConvertToNSArray(rel);
}

- (void)handleAccessibleTextChangeEvent:(NSString*)change
                               inserted:(BOOL)isInserted
                            inContainer:(const AccessibleOrProxy&)container
                                     at:(int32_t)start {
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
      if (![self stateWithMask:states::SELECTABLE_TEXT]) {
        break;
      }
      // We consider any caret move event to be a selected text change event.
      // So dispatching an event for EVENT_TEXT_SELECTION_CHANGED would be
      // reduntant.
      MOXTextMarkerDelegate* delegate =
          static_cast<MOXTextMarkerDelegate*>([self moxTextMarkerDelegate]);
      NSMutableDictionary* userInfo =
          [[delegate selectionChangeInfo] mutableCopy];
      userInfo[@"AXTextChangeElement"] = self;

      mozAccessible* webArea = [self topWebArea];
      [webArea
          moxPostNotification:NSAccessibilitySelectedTextChangedNotification
                 withUserInfo:userInfo];
      [self moxPostNotification:NSAccessibilitySelectedTextChangedNotification
                   withUserInfo:userInfo];
      break;
    }
    case nsIAccessibleEvent::EVENT_LIVE_REGION_ADDED:
      mIsLiveRegion = true;
      [self moxPostNotification:@"AXLiveRegionCreated"];
      break;
    case nsIAccessibleEvent::EVENT_LIVE_REGION_REMOVED:
      mIsLiveRegion = false;
      break;
    case nsIAccessibleEvent::EVENT_REORDER:
      [self maybePostLiveRegionChanged];
      break;
    case nsIAccessibleEvent::EVENT_NAME_CHANGE: {
      if (![self providesLabelNotTitle]) {
        [self moxPostNotification:NSAccessibilityTitleChangedNotification];
      }
      [self maybePostLiveRegionChanged];
      break;
    }
  }
}

- (void)expire {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [self invalidateState];

  mGeckoAccessible.SetBits(0);

  [self moxPostNotification:NSAccessibilityUIElementDestroyedNotification];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (BOOL)isExpired {
  return !mGeckoAccessible.AsAccessible() && !mGeckoAccessible.AsProxy();
}

@end
