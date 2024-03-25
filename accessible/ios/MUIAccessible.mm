/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "MUIAccessible.h"

#include "nsString.h"
#include "RootAccessibleWrap.h"

using namespace mozilla;
using namespace mozilla::a11y;

#ifdef A11Y_LOG
#  define DEBUG_HINTS
#endif

#ifdef DEBUG_HINTS
static NSString* ToNSString(const nsACString& aCString) {
  if (aCString.IsEmpty()) {
    return [NSString string];
  }
  return [[[NSString alloc] initWithBytes:aCString.BeginReading()
                                   length:aCString.Length()
                                 encoding:NSUTF8StringEncoding] autorelease];
}
#endif

static NSString* ToNSString(const nsAString& aString) {
  if (aString.IsEmpty()) {
    return [NSString string];
  }
  return [NSString stringWithCharacters:reinterpret_cast<const unichar*>(
                                            aString.BeginReading())
                                 length:aString.Length()];
}

// These rules offer conditions for whether a gecko accessible
// should be considered a UIKit accessibility element. Each role is mapped to a
// rule.
enum class IsAccessibilityElementRule {
  // Always yes
  Yes,
  // Always no
  No,
  // If the accessible has no children. For example an empty header
  // which is labeled.
  IfChildless,
  // If the accessible has no children and it is named and focusable.
  IfChildlessWithNameAndFocusable,
  // If this accessible isn't a child of an accessibility element. For example,
  // a text leaf child of a button.
  IfParentIsntElementWithName,
  // If this accessible has multiple leafs that should functionally be
  // united, for example a link with span elements.
  IfBrokenUp,
};

class Trait {
 public:
  static const uint64_t None = 0;
  static const uint64_t Button = ((uint64_t)0x1) << 0;
  static const uint64_t Link = ((uint64_t)0x1) << 1;
  static const uint64_t Image = ((uint64_t)0x1) << 2;
  static const uint64_t Selected = ((uint64_t)0x1) << 3;
  static const uint64_t PlaysSound = ((uint64_t)0x1) << 4;
  static const uint64_t KeyboardKey = ((uint64_t)0x1) << 5;
  static const uint64_t StaticText = ((uint64_t)0x1) << 6;
  static const uint64_t SummaryElement = ((uint64_t)0x1) << 7;
  static const uint64_t NotEnabled = ((uint64_t)0x1) << 8;
  static const uint64_t UpdatesFrequently = ((uint64_t)0x1) << 9;
  static const uint64_t SearchField = ((uint64_t)0x1) << 10;
  static const uint64_t StartsMediaSession = ((uint64_t)0x1) << 11;
  static const uint64_t Adjustable = ((uint64_t)0x1) << 12;
  static const uint64_t AllowsDirectInteraction = ((uint64_t)0x1) << 13;
  static const uint64_t CausesPageTurn = ((uint64_t)0x1) << 14;
  static const uint64_t TabBar = ((uint64_t)0x1) << 15;
  static const uint64_t Header = ((uint64_t)0x1) << 16;
  static const uint64_t WebContent = ((uint64_t)0x1) << 17;
  static const uint64_t TextEntry = ((uint64_t)0x1) << 18;
  static const uint64_t PickerElement = ((uint64_t)0x1) << 19;
  static const uint64_t RadioButton = ((uint64_t)0x1) << 20;
  static const uint64_t IsEditing = ((uint64_t)0x1) << 21;
  static const uint64_t LaunchIcon = ((uint64_t)0x1) << 22;
  static const uint64_t StatusBarElement = ((uint64_t)0x1) << 23;
  static const uint64_t SecureTextField = ((uint64_t)0x1) << 24;
  static const uint64_t Inactive = ((uint64_t)0x1) << 25;
  static const uint64_t Footer = ((uint64_t)0x1) << 26;
  static const uint64_t BackButton = ((uint64_t)0x1) << 27;
  static const uint64_t TabButton = ((uint64_t)0x1) << 28;
  static const uint64_t AutoCorrectCandidate = ((uint64_t)0x1) << 29;
  static const uint64_t DeleteKey = ((uint64_t)0x1) << 30;
  static const uint64_t SelectionDismissesItem = ((uint64_t)0x1) << 31;
  static const uint64_t Visited = ((uint64_t)0x1) << 32;
  static const uint64_t Scrollable = ((uint64_t)0x1) << 33;
  static const uint64_t Spacer = ((uint64_t)0x1) << 34;
  static const uint64_t TableIndex = ((uint64_t)0x1) << 35;
  static const uint64_t Map = ((uint64_t)0x1) << 36;
  static const uint64_t TextOperationsAvailable = ((uint64_t)0x1) << 37;
  static const uint64_t Draggable = ((uint64_t)0x1) << 38;
  static const uint64_t GesturePracticeRegion = ((uint64_t)0x1) << 39;
  static const uint64_t PopupButton = ((uint64_t)0x1) << 40;
  static const uint64_t AllowsNativeSliding = ((uint64_t)0x1) << 41;
  static const uint64_t MathEquation = ((uint64_t)0x1) << 42;
  static const uint64_t ContainedByTable = ((uint64_t)0x1) << 43;
  static const uint64_t ContainedByList = ((uint64_t)0x1) << 44;
  static const uint64_t TouchContainer = ((uint64_t)0x1) << 45;
  static const uint64_t SupportsZoom = ((uint64_t)0x1) << 46;
  static const uint64_t TextArea = ((uint64_t)0x1) << 47;
  static const uint64_t BookContent = ((uint64_t)0x1) << 48;
  static const uint64_t ContainedByLandmark = ((uint64_t)0x1) << 49;
  static const uint64_t FolderIcon = ((uint64_t)0x1) << 50;
  static const uint64_t ReadOnly = ((uint64_t)0x1) << 51;
  static const uint64_t MenuItem = ((uint64_t)0x1) << 52;
  static const uint64_t Toggle = ((uint64_t)0x1) << 53;
  static const uint64_t IgnoreItemChooser = ((uint64_t)0x1) << 54;
  static const uint64_t SupportsTrackingDetail = ((uint64_t)0x1) << 55;
  static const uint64_t Alert = ((uint64_t)0x1) << 56;
  static const uint64_t ContainedByFieldset = ((uint64_t)0x1) << 57;
  static const uint64_t AllowsLayoutChangeInStatusBar = ((uint64_t)0x1) << 58;
};

#pragma mark -

@interface NSObject (AccessibilityPrivate)
- (void)_accessibilityUnregister;
@end

@implementation MUIAccessible

- (id)initWithAccessible:(Accessible*)aAcc {
  MOZ_ASSERT(aAcc, "Cannot init MUIAccessible with null");
  if ((self = [super init])) {
    mGeckoAccessible = aAcc;
  }

  return self;
}

- (mozilla::a11y::Accessible*)geckoAccessible {
  return mGeckoAccessible;
}

- (void)expire {
  mGeckoAccessible = nullptr;
  if ([self respondsToSelector:@selector(_accessibilityUnregister)]) {
    [self _accessibilityUnregister];
  }
}

- (void)dealloc {
  [super dealloc];
}

static bool isAccessibilityElementInternal(Accessible* aAccessible) {
  MOZ_ASSERT(aAccessible);
  IsAccessibilityElementRule rule = IsAccessibilityElementRule::No;

#define ROLE(_geckoRole, stringRole, ariaRole, atkRole, macRole, macSubrole, \
             msaaRole, ia2Role, androidClass, iosIsElement, uiaControlType,  \
             nameRule)                                                       \
  case roles::_geckoRole:                                                    \
    rule = iosIsElement;                                                     \
    break;
  switch (aAccessible->Role()) {
#include "RoleMap.h"
  }

  switch (rule) {
    case IsAccessibilityElementRule::Yes:
      return true;
    case IsAccessibilityElementRule::No:
      return false;
    case IsAccessibilityElementRule::IfChildless:
      return aAccessible->ChildCount() == 0;
    case IsAccessibilityElementRule::IfParentIsntElementWithName: {
      nsAutoString name;
      aAccessible->Name(name);
      name.CompressWhitespace();
      if (name.IsEmpty()) {
        return false;
      }

      if (isAccessibilityElementInternal(aAccessible->Parent())) {
        // This is a text leaf that needs to be pruned from a button or the
        // likes. It should also be ignored in the event of its parent being a
        // pruned link.
        return false;
      }

      return true;
    }
    case IsAccessibilityElementRule::IfChildlessWithNameAndFocusable:
      if (aAccessible->ChildCount() == 0 &&
          (aAccessible->State() & states::FOCUSABLE)) {
        nsAutoString name;
        aAccessible->Name(name);
        name.CompressWhitespace();
        return !name.IsEmpty();
      }
      return false;
    case IsAccessibilityElementRule::IfBrokenUp: {
      uint32_t childCount = aAccessible->ChildCount();
      if (childCount == 1) {
        // If this is a single child container just use the text leaf and its
        // traits will be inherited.
        return false;
      }

      for (uint32_t idx = 0; idx < childCount; idx++) {
        Accessible* child = aAccessible->ChildAt(idx);
        role accRole = child->Role();
        if (accRole != roles::STATICTEXT && accRole != roles::TEXT_LEAF &&
            accRole != roles::GRAPHIC) {
          // If this container contains anything but text leafs and images
          // ignore this accessible. Its descendants will inherit the
          // container's traits.
          return false;
        }
      }

      return true;
    }
    default:
      break;
  }

  MOZ_ASSERT_UNREACHABLE("Unhandled IsAccessibilityElementRule");

  return false;
}

- (BOOL)isAccessibilityElement {
  if (!mGeckoAccessible) {
    return NO;
  }

  return isAccessibilityElementInternal(mGeckoAccessible) ? YES : NO;
}

- (NSString*)accessibilityLabel {
  if (!mGeckoAccessible) {
    return @"";
  }

  nsAutoString name;
  mGeckoAccessible->Name(name);

  return ToNSString(name);
}

- (NSString*)accessibilityHint {
  if (!mGeckoAccessible) {
    return @"";
  }

#ifdef DEBUG_HINTS
  // Just put in a debug description as the label so we get a clue about which
  // accessible ends up where.
  nsAutoCString desc;
  mGeckoAccessible->DebugDescription(desc);
  return ToNSString(desc);
#else
  return @"";
#endif
}

- (CGRect)accessibilityFrame {
  RootAccessibleWrap* rootAcc = static_cast<RootAccessibleWrap*>(
      mGeckoAccessible->IsLocal()
          ? mGeckoAccessible->AsLocal()->RootAccessible()
          : mGeckoAccessible->AsRemote()
                ->OuterDocOfRemoteBrowser()
                ->RootAccessible());

  if (!rootAcc) {
    return CGRectMake(0, 0, 0, 0);
  }

  LayoutDeviceIntRect rect = mGeckoAccessible->Bounds();
  return rootAcc->DevPixelsRectToUIKit(rect);
}

- (NSString*)accessibilityValue {
  if (!mGeckoAccessible) {
    return nil;
  }

  uint64_t state = mGeckoAccessible->State();
  if (state & states::LINKED) {
    // Value returns the URL. We don't want to expose that as the value on iOS.
    return nil;
  }

  if (state & states::CHECKABLE) {
    if (state & states::CHECKED) {
      return @"1";
    }
    if (state & states::MIXED) {
      return @"2";
    }
    return @"0";
  }

  if (mGeckoAccessible->IsPassword()) {
    // Accessible::Value returns an empty string. On iOS, we need to return the
    // masked password so that AT knows how many characters are in the password.
    Accessible* leaf = mGeckoAccessible->FirstChild();
    if (!leaf) {
      return nil;
    }
    nsAutoString masked;
    leaf->AppendTextTo(masked);
    return ToNSString(masked);
  }

  // If there is a heading ancestor, self has the header trait, so value should
  // be the heading level.
  for (Accessible* acc = mGeckoAccessible; acc; acc = acc->Parent()) {
    if (acc->Role() == roles::HEADING) {
      return [NSString stringWithFormat:@"%d", acc->GroupPosition().level];
    }
  }

  nsAutoString value;
  mGeckoAccessible->Value(value);
  return ToNSString(value);
}

static uint64_t GetAccessibilityTraits(Accessible* aAccessible) {
  uint64_t state = aAccessible->State();
  uint64_t traits = Trait::WebContent;
  switch (aAccessible->Role()) {
    case roles::LINK:
      traits |= Trait::Link;
      break;
    case roles::GRAPHIC:
      traits |= Trait::Image;
      break;
    case roles::PAGETAB:
      traits |= Trait::TabButton;
      break;
    case roles::PUSHBUTTON:
    case roles::SUMMARY:
    case roles::COMBOBOX:
    case roles::BUTTONMENU:
    case roles::TOGGLE_BUTTON:
    case roles::CHECKBUTTON:
    case roles::SWITCH:
      traits |= Trait::Button;
      break;
    case roles::RADIOBUTTON:
      traits |= Trait::RadioButton;
      break;
    case roles::HEADING:
      traits |= Trait::Header;
      break;
    case roles::STATICTEXT:
    case roles::TEXT_LEAF:
      traits |= Trait::StaticText;
      break;
    case roles::SLIDER:
    case roles::SPINBUTTON:
      traits |= Trait::Adjustable;
      break;
    case roles::MENUITEM:
    case roles::PARENT_MENUITEM:
    case roles::CHECK_MENU_ITEM:
    case roles::RADIO_MENU_ITEM:
      traits |= Trait::MenuItem;
      break;
    case roles::PASSWORD_TEXT:
      traits |= Trait::SecureTextField;
      break;
    default:
      break;
  }

  if ((traits & Trait::Link) && (state & states::TRAVERSED)) {
    traits |= Trait::Visited;
  }

  if ((traits & Trait::Button) && (state & states::HASPOPUP)) {
    traits |= Trait::PopupButton;
  }

  if (state & states::SELECTED) {
    traits |= Trait::Selected;
  }

  if (state & states::CHECKABLE) {
    traits |= Trait::Toggle;
  }

  if (!(state & states::ENABLED)) {
    traits |= Trait::NotEnabled;
  }

  if (state & states::EDITABLE) {
    traits |= Trait::TextEntry;
    if (state & states::FOCUSED) {
      // XXX: Also add "has text cursor" trait
      traits |= Trait::IsEditing | Trait::TextOperationsAvailable;
    }

    if (aAccessible->IsSearchbox()) {
      traits |= Trait::SearchField;
    }

    if (state & states::MULTI_LINE) {
      traits |= Trait::TextArea;
    }
  }

  return traits;
}

- (uint64_t)accessibilityTraits {
  if (!mGeckoAccessible) {
    return Trait::None;
  }

  uint64_t traits = GetAccessibilityTraits(mGeckoAccessible);

  for (Accessible* parent = mGeckoAccessible->Parent(); parent;
       parent = parent->Parent()) {
    traits |= GetAccessibilityTraits(parent);
  }

  return traits;
}

- (NSInteger)accessibilityElementCount {
  return mGeckoAccessible ? mGeckoAccessible->ChildCount() : 0;
}

- (nullable id)accessibilityElementAtIndex:(NSInteger)index {
  if (!mGeckoAccessible) {
    return nil;
  }

  Accessible* child = mGeckoAccessible->ChildAt(index);
  return GetNativeFromGeckoAccessible(child);
}

- (NSInteger)indexOfAccessibilityElement:(id)element {
  Accessible* acc = [(MUIAccessible*)element geckoAccessible];
  if (!acc || mGeckoAccessible != acc->Parent()) {
    return -1;
  }

  return acc->IndexInParent();
}

- (NSArray* _Nullable)accessibilityElements {
  NSMutableArray* children = [[[NSMutableArray alloc] init] autorelease];
  uint32_t childCount = mGeckoAccessible->ChildCount();
  for (uint32_t i = 0; i < childCount; i++) {
    if (MUIAccessible* child =
            GetNativeFromGeckoAccessible(mGeckoAccessible->ChildAt(i))) {
      [children addObject:child];
    }
  }

  return children;
}

- (UIAccessibilityContainerType)accessibilityContainerType {
  return UIAccessibilityContainerTypeNone;
}

- (NSRange)_accessibilitySelectedTextRange {
  if (!mGeckoAccessible || !mGeckoAccessible->IsHyperText()) {
    return NSMakeRange(NSNotFound, 0);
  }
  // XXX This will only work in simple plain text boxes. It will break horribly
  // if there are any embedded objects. Also, it only supports caret, not
  // selection.
  int32_t caret = mGeckoAccessible->AsHyperTextBase()->CaretOffset();
  if (caret != -1) {
    return NSMakeRange(caret, 0);
  }
  return NSMakeRange(NSNotFound, 0);
}

- (void)_accessibilitySetSelectedTextRange:(NSRange)range {
  if (!mGeckoAccessible || !mGeckoAccessible->IsHyperText()) {
    return;
  }
  // XXX This will only work in simple plain text boxes. It will break horribly
  // if there are any embedded objects. Also, it only supports caret, not
  // selection.
  mGeckoAccessible->AsHyperTextBase()->SetCaretOffset(range.location);
}

@end
