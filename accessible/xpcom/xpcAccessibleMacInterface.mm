/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleMacInterface.h"

#include "nsCocoaUtils.h"
#include "nsContentUtils.h"
#include "mozilla/dom/ToJSValue.h"

#import "mozAccessible.h"

using namespace mozilla::a11y;

// The values in gAttributeToGetterMap were generated using tthe folling snippet in the browser
// toolbox:
//
// let r = /when clients request
// NSAccessibility(\w+)Attribute.*\n@property.*\W(\w+)\sAPI_AVA/g
// fetch('https://raw.githubusercontent.com/phracker/MacOSX-SDKs/master/MacOSX10.15.sdk' +
//   '/System/Library/Frameworks/AppKit.framework/Versions/C/Headers/NSAccessibilityProtocols.h')
//   .then((response) => response.text()).then((data) => {
//     let pairs =
//       [...data.matchAll(r)].map(e => ['AX' + e[1], e[2]]);
//     console.log("static NSDictionary* gAttributeToGetterMap = @{")
//     for (let [attr, selector] of pairs) {
//       console.log(`  @"${attr}" : @"${selector}",`);
//     }
//     console.log("};");
//   });

static NSDictionary* gAttributeToGetterMap = @{
  @"AXSize" : @"accessibilityFrame",
  @"AXFocused" : @"accessibilityFocused",
  @"AXTopLevelUIElement" : @"accessibilityTopLevelUIElement",
  @"AXURL" : @"accessibilityURL",
  @"AXValue" : @"accessibilityValue",
  @"AXValueDescription" : @"accessibilityValueDescription",
  @"AXVisibleChildren" : @"accessibilityVisibleChildren",
  @"AXSubrole" : @"accessibilitySubrole",
  @"AXTitle" : @"accessibilityTitle",
  @"AXTitleUIElement" : @"accessibilityTitleUIElement",
  @"AXNextContents" : @"accessibilityNextContents",
  @"AXOrientation" : @"accessibilityOrientation",
  @"AXOverflowButton" : @"accessibilityOverflowButton",
  @"AXParent" : @"accessibilityParent",
  @"AXPlaceholderValue" : @"accessibilityPlaceholderValue",
  @"AXPreviousContents" : @"accessibilityPreviousContents",
  @"AXRole" : @"accessibilityRole",
  @"AXRoleDescription" : @"accessibilityRoleDescription",
  @"AXSearchButton" : @"accessibilitySearchButton",
  @"AXSearchMenu" : @"accessibilitySearchMenu",
  @"AXSelected" : @"accessibilitySelected",
  @"AXSelectedChildren" : @"accessibilitySelectedChildren",
  @"AXServesAsTitleForUIElements" : @"accessibilityServesAsTitleForUIElements",
  @"AXShownMenu" : @"accessibilityShownMenu",
  @"AXMinValue" : @"accessibilityMinValue",
  @"AXMaxValue" : @"accessibilityMaxValue",
  @"AXLinkedUIElements" : @"accessibilityLinkedUIElements",
  @"AXWindow" : @"accessibilityWindow",
  @"AXIdentifier" : @"accessibilityIdentifier",
  @"AXHelp" : @"accessibilityHelp",
  @"AXFilename" : @"accessibilityFilename",
  @"AXExpanded" : @"accessibilityExpanded",
  @"AXEdited" : @"accessibilityEdited",
  @"AXEnabled" : @"accessibilityEnabled",
  @"AXChildren" : @"accessibilityChildren",
  @"AXClearButton" : @"accessibilityClearButton",
  @"AXCancelButton" : @"accessibilityCancelButton",
  @"AXContainsProtectedContent" : @"accessibilityProtectedContent",
  @"AXContents" : @"accessibilityContents",
  @"AXDescription" : @"accessibilityLabel",
  @"AXAlternateUIVisible" : @"accessibilityAlternateUIVisible",
  @"AXSharedFocusElements" : @"accessibilitySharedFocusElements",
  @"AXRequired" : @"accessibilityRequired",
  @"AXFocusedUIElement" : @"accessibilityApplicationFocusedUIElement",
  @"AXMainWindow" : @"accessibilityMainWindow",
  @"AXHidden" : @"accessibilityHidden",
  @"AXFrontmost" : @"accessibilityFrontmost",
  @"AXFocusedWindow" : @"accessibilityFocusedWindow",
  @"AXWindows" : @"accessibilityWindows",
  @"AXExtrasMenuBar" : @"accessibilityExtrasMenuBar",
  @"AXMenuBar" : @"accessibilityMenuBar",
  @"AXColumnTitles" : @"accessibilityColumnTitles",
  @"AXOrderedByRow" : @"accessibilityOrderedByRow",
  @"AXHorizontalUnits" : @"accessibilityHorizontalUnits",
  @"AXVerticalUnits" : @"accessibilityVerticalUnits",
  @"AXHorizontalUnitDescription" : @"accessibilityHorizontalUnitDescription",
  @"AXVerticalUnitDescription" : @"accessibilityVerticalUnitDescription",
  @"AXHandles" : @"accessibilityHandles",
  @"AXWarningValue" : @"accessibilityWarningValue",
  @"AXCriticalValue" : @"accessibilityCriticalValue",
  @"AXDisclosing" : @"accessibilityDisclosed",
  @"AXDisclosedByRow" : @"accessibilityDisclosedByRow",
  @"AXDisclosedRows" : @"accessibilityDisclosedRows",
  @"AXDisclosureLevel" : @"accessibilityDisclosureLevel",
  @"AXMarkerUIElements" : @"accessibilityMarkerUIElements",
  @"AXMarkerValues" : @"accessibilityMarkerValues",
  @"AXMarkerGroupUIElement" : @"accessibilityMarkerGroupUIElement",
  @"AXUnits" : @"accessibilityUnits",
  @"AXUnitDescription" : @"accessibilityUnitDescription",
  @"AXMarkerType" : @"accessibilityRulerMarkerType",
  @"AXMarkerTypeDescription" : @"accessibilityMarkerTypeDescription",
  @"AXHorizontalScrollBar" : @"accessibilityHorizontalScrollBar",
  @"AXVerticalScrollBar" : @"accessibilityVerticalScrollBar",
  @"AXAllowedValues" : @"accessibilityAllowedValues",
  @"AXLabelUIElements" : @"accessibilityLabelUIElements",
  @"AXLabelValue" : @"accessibilityLabelValue",
  @"AXSplitters" : @"accessibilitySplitters",
  @"AXDecrementButton" : @"accessibilityDecrementButton",
  @"AXIncrementButton" : @"accessibilityIncrementButton",
  @"AXTabs" : @"accessibilityTabs",
  @"AXHeader" : @"accessibilityHeader",
  @"AXColumnCount" : @"accessibilityColumnCount",
  @"AXRowCount" : @"accessibilityRowCount",
  @"AXIndex" : @"accessibilityIndex",
  @"AXColumns" : @"accessibilityColumns",
  @"AXRows" : @"accessibilityRows",
  @"AXVisibleRows" : @"accessibilityVisibleRows",
  @"AXSelectedRows" : @"accessibilitySelectedRows",
  @"AXVisibleColumns" : @"accessibilityVisibleColumns",
  @"AXSelectedColumns" : @"accessibilitySelectedColumns",
  @"AXSortDirection" : @"accessibilitySortDirection",
  @"AXRowHeaderUIElements" : @"accessibilityRowHeaderUIElements",
  @"AXSelectedCells" : @"accessibilitySelectedCells",
  @"AXVisibleCells" : @"accessibilityVisibleCells",
  @"AXColumnHeaderUIElements" : @"accessibilityColumnHeaderUIElements",
  @"AXRowIndexRange" : @"accessibilityRowIndexRange",
  @"AXColumnIndexRange" : @"accessibilityColumnIndexRange",
  @"AXInsertionPointLineNumber" : @"accessibilityInsertionPointLineNumber",
  @"AXSharedCharacterRange" : @"accessibilitySharedCharacterRange",
  @"AXSharedTextUIElements" : @"accessibilitySharedTextUIElements",
  @"AXVisibleCharacterRange" : @"accessibilityVisibleCharacterRange",
  @"AXNumberOfCharacters" : @"accessibilityNumberOfCharacters",
  @"AXSelectedText" : @"accessibilitySelectedText",
  @"AXSelectedTextRange" : @"accessibilitySelectedTextRange",
  @"AXSelectedTextRanges" : @"accessibilitySelectedTextRanges",
  @"AXToolbarButton" : @"accessibilityToolbarButton",
  @"AXModal" : @"accessibilityModal",
  @"AXProxy" : @"accessibilityProxy",
  @"AXMain" : @"accessibilityMain",
  @"AXFullScreenButton" : @"accessibilityFullScreenButton",
  @"AXGrowArea" : @"accessibilityGrowArea",
  @"AXDocument" : @"accessibilityDocument",
  @"AXDefaultButton" : @"accessibilityDefaultButton",
  @"AXCloseButton" : @"accessibilityCloseButton",
  @"AXZoomButton" : @"accessibilityZoomButton",
  @"AXMinimizeButton" : @"accessibilityMinimizeButton",
  @"AXMinimized" : @"accessibilityMinimized",
};

// The values in gActionToMethodMap were generated using the folling snippet in the browser
// toolbox:
//
// let r = /when clients perform NSAccessibility(\w+)Action.*\n-.*\W(\w+)\sAPI_AVA/g
// fetch('https://raw.githubusercontent.com/phracker/MacOSX-SDKs/master/MacOSX10.15.sdk' +
//   '/System/Library/Frameworks/AppKit.framework/Versions/C/Headers/NSAccessibilityProtocols.h')
//   .then((response) => response.text()).then((data) => {
//     let pairs =
//       [...data.matchAll(r)].map(e => ['AX' + e[1], e[2]]);
//     console.log("static NSDictionary* gActionToMethodMap = @{")
//     for (let [attr, selector] of pairs) {
//       console.log(`  @"${attr}" : @"${selector}",`);
//     }
//     console.log("};");
//   });

static NSDictionary* gActionToMethodMap = @{
  @"AXCancel" : @"accessibilityPerformCancel",
  @"AXConfirm" : @"accessibilityPerformConfirm",
  @"AXDecrement" : @"accessibilityPerformDecrement",
  @"AXDelete" : @"accessibilityPerformDelete",
  @"AXIncrement" : @"accessibilityPerformIncrement",
  @"AXPick" : @"accessibilityPerformPick",
  @"AXPress" : @"accessibilityPerformPress",
  @"AXRaise" : @"accessibilityPerformRaise",
  @"AXShowAlternateUI" : @"accessibilityPerformShowAlternateUI",
  @"AXShowDefaultUI" : @"accessibilityPerformShowDefaultUI",
  @"AXShowMenu" : @"accessibilityPerformShowMenu",
};

NS_IMPL_ISUPPORTS(xpcAccessibleMacInterface, nsIAccessibleMacInterface)

xpcAccessibleMacInterface::xpcAccessibleMacInterface(AccessibleOrProxy aObj) {
  mNativeObject = aObj.IsProxy()
                      ? GetNativeFromProxy(aObj.AsProxy())
                      : static_cast<AccessibleWrap*>(aObj.AsAccessible())->GetNativeObject();
  [mNativeObject retain];
}

xpcAccessibleMacInterface::xpcAccessibleMacInterface(id aNativeObj) : mNativeObject(aNativeObj) {
  [mNativeObject retain];
}

xpcAccessibleMacInterface::~xpcAccessibleMacInterface() { [mNativeObject release]; }

NS_IMETHODIMP
xpcAccessibleMacInterface::GetAttributeNames(nsTArray<nsString>& aAttributeNames) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT

  if (!mNativeObject || [mNativeObject isExpired]) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NSMutableSet* attributeNames =
      [NSMutableSet setWithArray:[mNativeObject accessibilityAttributeNames]];

  for (NSString* key in gAttributeToGetterMap) {
    if (SupportsSelector(NSSelectorFromString(gAttributeToGetterMap[key]))) {
      [attributeNames addObject:key];
    }
  }

  for (NSString* name in attributeNames) {
    nsAutoString attribName;
    nsCocoaUtils::GetStringForNSString(name, attribName);
    aAttributeNames.AppendElement(attribName);
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT
}

NS_IMETHODIMP
xpcAccessibleMacInterface::GetActionNames(nsTArray<nsString>& aActionNames) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT

  if (!mNativeObject || [mNativeObject isExpired]) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NSMutableSet* actionNames = [NSMutableSet setWithArray:[mNativeObject accessibilityActionNames]];

  for (NSString* key in gActionToMethodMap) {
    if (SupportsSelector(NSSelectorFromString(gActionToMethodMap[key]))) {
      [actionNames addObject:key];
    }
  }

  // Bug 1625316: Support accessibilityCustomActions too.

  for (NSString* name in actionNames) {
    nsAutoString actionName;
    nsCocoaUtils::GetStringForNSString(name, actionName);
    aActionNames.AppendElement(actionName);
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT
}

NS_IMETHODIMP
xpcAccessibleMacInterface::PerformAction(const nsAString& aActionName) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT

  if (!mNativeObject || [mNativeObject isExpired]) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NSString* actionName = nsCocoaUtils::ToNSString(aActionName);
  if (NSString* selectorString = gActionToMethodMap[actionName]) {
    SEL selector = NSSelectorFromString(selectorString);
    if (SupportsSelector(selector)) {
      [mNativeObject performSelector:selector];
      return NS_OK;
    }
  }

  [mNativeObject accessibilityPerformAction:actionName];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT
}

NS_IMETHODIMP
xpcAccessibleMacInterface::GetAttributeValue(const nsAString& aAttributeName, JSContext* aCx,
                                             JS::MutableHandleValue aResult) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT

  if (!mNativeObject || [mNativeObject isExpired]) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NSString* attribName = nsCocoaUtils::ToNSString(aAttributeName);
  if (NSString* selectorString = gAttributeToGetterMap[attribName]) {
    SEL selector = NSSelectorFromString(selectorString);
    if (SupportsSelector(selector)) {
      return NSObjectToJsValue([mNativeObject performSelector:selector], aCx, aResult);
    }
  }

  return NSObjectToJsValue([mNativeObject accessibilityAttributeValue:attribName], aCx, aResult);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT
}

bool xpcAccessibleMacInterface::SupportsSelector(SEL aSelector) {
  // return true if we have this selector, and if isAccessibilitySelectorAllowed
  // is implemented too whether it is "allowed".
  return [mNativeObject respondsToSelector:aSelector] &&
         (![mNativeObject respondsToSelector:@selector(isAccessibilitySelectorAllowed:selector:)] ||
          [mNativeObject isAccessibilitySelectorAllowed:aSelector]);
}

nsresult xpcAccessibleMacInterface::NSObjectToJsValue(id aObj, JSContext* aCx,
                                                      JS::MutableHandleValue aResult) {
  if (!aObj) {
    aResult.set(JS::NullValue());
  } else if ([aObj isKindOfClass:[NSString class]]) {
    nsAutoString strVal;
    nsCocoaUtils::GetStringForNSString((NSString*)aObj, strVal);
    if (!mozilla::dom::ToJSValue(aCx, strVal, aResult)) {
      return NS_ERROR_FAILURE;
    }
  } else if ([aObj isKindOfClass:[NSNumber class]]) {
    // If the type being held by the NSNumber is a BOOL set js value
    // to boolean. Otherwise use a double value.
    if (strcmp([(NSNumber*)aObj objCType], @encode(BOOL)) == 0) {
      if (!mozilla::dom::ToJSValue(aCx, [(NSNumber*)aObj boolValue], aResult)) {
        return NS_ERROR_FAILURE;
      }
    } else {
      if (!mozilla::dom::ToJSValue(aCx, [(NSNumber*)aObj doubleValue], aResult)) {
        return NS_ERROR_FAILURE;
      }
    }
  } else if ([aObj isKindOfClass:[NSValue class]] &&
             strcmp([(NSValue*)aObj objCType], @encode(NSPoint)) == 0) {
    NSPoint point = [(NSValue*)aObj pointValue];
    return NSObjectToJsValue(
        @[ [NSNumber numberWithDouble:point.x], [NSNumber numberWithDouble:point.y] ], aCx,
        aResult);
  } else if ([aObj isKindOfClass:[NSValue class]] &&
             strcmp([(NSValue*)aObj objCType], @encode(NSSize)) == 0) {
    NSSize size = [(NSValue*)aObj sizeValue];
    return NSObjectToJsValue(
        @[ [NSNumber numberWithDouble:size.width], [NSNumber numberWithDouble:size.height] ], aCx,
        aResult);
  } else if ([aObj respondsToSelector:@selector(accessibilityAttributeNames)]) {
    nsCOMPtr<nsIAccessibleMacInterface> obj = new xpcAccessibleMacInterface(aObj);
    return nsContentUtils::WrapNative(aCx, obj, &NS_GET_IID(nsIAccessibleMacInterface), aResult);
  } else if ([aObj isKindOfClass:[NSArray class]]) {
    NSArray* objArr = (NSArray*)aObj;

    JS::RootedVector<JS::Value> v(aCx);
    if (!v.resize([objArr count])) {
      return NS_ERROR_FAILURE;
    }
    for (size_t i = 0; i < [objArr count]; ++i) {
      nsresult rv = NSObjectToJsValue(objArr[i], aCx, v[i]);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    JSObject* arrayObj = JS::NewArrayObject(aCx, v);
    if (!arrayObj) {
      return NS_ERROR_FAILURE;
    }
    aResult.setObject(*arrayObj);
  } else {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return NS_OK;
}

void xpcAccessibleMacInterface::FireEvent(id aNativeObj, id aNotification) {
  if (nsCOMPtr<nsIObserverService> obsService = services::GetObserverService()) {
    nsCOMPtr<nsISimpleEnumerator> observers;
    // Get all observers for the mac event topic.
    obsService->EnumerateObservers(NS_ACCESSIBLE_MAC_EVENT_TOPIC, getter_AddRefs(observers));
    if (observers) {
      bool hasObservers = false;
      observers->HasMoreElements(&hasObservers);
      // If we have observers, notify them.
      if (hasObservers) {
        nsCOMPtr<nsIAccessibleMacInterface> xpcIface = new xpcAccessibleMacInterface(aNativeObj);
        nsAutoString notificationStr;
        nsCocoaUtils::GetStringForNSString(aNotification, notificationStr);
        obsService->NotifyObservers(xpcIface, NS_ACCESSIBLE_MAC_EVENT_TOPIC, notificationStr.get());
      }
    }
  }
}
