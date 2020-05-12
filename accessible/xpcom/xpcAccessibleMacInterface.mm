/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleMacInterface.h"

#include "nsCocoaUtils.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "mozilla/dom/ToJSValue.h"

#import "mozAccessible.h"

using namespace mozilla::a11y;

// A mapping of "AX" attributes to getters and setters expected by NSAccessibilityProtocol
// The values in gAttributeToPropertyMap were generated using the folling snippet in the browser
// toolbox:
//
// let r = /when clients request NSAccessibility(\w+)Attribute.*\n@property(?: \(getter =
// (\w+))?.*\W(\w+)\sAPI_AVA/g
// fetch('https://raw.githubusercontent.com/phracker/MacOSX-SDKs/master/MacOSX10.15.sdk' +
//   '/System/Library/Frameworks/AppKit.framework/Versions/C/Headers/NSAccessibilityProtocols.h')
//   .then((response) => response.text()).then((data) => {
//     let pairs =
//       [...data.matchAll(r)].map(e => ['AX' + e[1], e[2] ? e[2] : e[3], "set" +
//       e[3].substr(0,1).toLocaleUpperCase() + e[3].substr(1) + ":"]);
//     let output = "static NSDictionary* gAttributeToPropertyMap = @{\n";
//     for (let [attr, getter, setter] of pairs) {
//       output += `  @"${attr}" : @[@"${getter}", @"${setter}"],\n`;
//     }
//     output += "};";
//   console.log(output);
//   });

static NSDictionary* gAttributeToPropertyMap = @{
  @"AXSize" : @[ @"accessibilityFrame", @"setAccessibilityFrame:" ],
  @"AXFocused" : @[ @"isAccessibilityFocused", @"setAccessibilityFocused:" ],
  @"AXTopLevelUIElement" :
      @[ @"accessibilityTopLevelUIElement", @"setAccessibilityTopLevelUIElement:" ],
  @"AXURL" : @[ @"accessibilityURL", @"setAccessibilityURL:" ],
  @"AXValue" : @[ @"accessibilityValue", @"setAccessibilityValue:" ],
  @"AXValueDescription" :
      @[ @"accessibilityValueDescription", @"setAccessibilityValueDescription:" ],
  @"AXVisibleChildren" : @[ @"accessibilityVisibleChildren", @"setAccessibilityVisibleChildren:" ],
  @"AXSubrole" : @[ @"accessibilitySubrole", @"setAccessibilitySubrole:" ],
  @"AXTitle" : @[ @"accessibilityTitle", @"setAccessibilityTitle:" ],
  @"AXTitleUIElement" : @[ @"accessibilityTitleUIElement", @"setAccessibilityTitleUIElement:" ],
  @"AXNextContents" : @[ @"accessibilityNextContents", @"setAccessibilityNextContents:" ],
  @"AXOrientation" : @[ @"accessibilityOrientation", @"setAccessibilityOrientation:" ],
  @"AXOverflowButton" : @[ @"accessibilityOverflowButton", @"setAccessibilityOverflowButton:" ],
  @"AXParent" : @[ @"accessibilityParent", @"setAccessibilityParent:" ],
  @"AXPlaceholderValue" :
      @[ @"accessibilityPlaceholderValue", @"setAccessibilityPlaceholderValue:" ],
  @"AXPreviousContents" :
      @[ @"accessibilityPreviousContents", @"setAccessibilityPreviousContents:" ],
  @"AXRole" : @[ @"accessibilityRole", @"setAccessibilityRole:" ],
  @"AXRoleDescription" : @[ @"accessibilityRoleDescription", @"setAccessibilityRoleDescription:" ],
  @"AXSearchButton" : @[ @"accessibilitySearchButton", @"setAccessibilitySearchButton:" ],
  @"AXSearchMenu" : @[ @"accessibilitySearchMenu", @"setAccessibilitySearchMenu:" ],
  @"AXSelected" : @[ @"isAccessibilitySelected", @"setAccessibilitySelected:" ],
  @"AXSelectedChildren" :
      @[ @"accessibilitySelectedChildren", @"setAccessibilitySelectedChildren:" ],
  @"AXServesAsTitleForUIElements" : @[
    @"accessibilityServesAsTitleForUIElements", @"setAccessibilityServesAsTitleForUIElements:"
  ],
  @"AXShownMenu" : @[ @"accessibilityShownMenu", @"setAccessibilityShownMenu:" ],
  @"AXMinValue" : @[ @"accessibilityMinValue", @"setAccessibilityMinValue:" ],
  @"AXMaxValue" : @[ @"accessibilityMaxValue", @"setAccessibilityMaxValue:" ],
  @"AXLinkedUIElements" :
      @[ @"accessibilityLinkedUIElements", @"setAccessibilityLinkedUIElements:" ],
  @"AXWindow" : @[ @"accessibilityWindow", @"setAccessibilityWindow:" ],
  @"AXIdentifier" : @[ @"accessibilityIdentifier", @"setAccessibilityIdentifier:" ],
  @"AXHelp" : @[ @"accessibilityHelp", @"setAccessibilityHelp:" ],
  @"AXFilename" : @[ @"accessibilityFilename", @"setAccessibilityFilename:" ],
  @"AXExpanded" : @[ @"isAccessibilityExpanded", @"setAccessibilityExpanded:" ],
  @"AXEdited" : @[ @"isAccessibilityEdited", @"setAccessibilityEdited:" ],
  @"AXEnabled" : @[ @"isAccessibilityEnabled", @"setAccessibilityEnabled:" ],
  @"AXChildren" : @[ @"accessibilityChildren", @"setAccessibilityChildren:" ],
  @"AXClearButton" : @[ @"accessibilityClearButton", @"setAccessibilityClearButton:" ],
  @"AXCancelButton" : @[ @"accessibilityCancelButton", @"setAccessibilityCancelButton:" ],
  @"AXContainsProtectedContent" :
      @[ @"isAccessibilityProtectedContent", @"setAccessibilityProtectedContent:" ],
  @"AXContents" : @[ @"accessibilityContents", @"setAccessibilityContents:" ],
  @"AXDescription" : @[ @"accessibilityLabel", @"setAccessibilityLabel:" ],
  @"AXAlternateUIVisible" :
      @[ @"isAccessibilityAlternateUIVisible", @"setAccessibilityAlternateUIVisible:" ],
  @"AXSharedFocusElements" :
      @[ @"accessibilitySharedFocusElements", @"setAccessibilitySharedFocusElements:" ],
  @"AXRequired" : @[ @"accessibilityRequired", @"setAccessibilityRequired:" ],
  @"AXFocusedUIElement" : @[
    @"accessibilityApplicationFocusedUIElement", @"setAccessibilityApplicationFocusedUIElement:"
  ],
  @"AXMainWindow" : @[ @"accessibilityMainWindow", @"setAccessibilityMainWindow:" ],
  @"AXHidden" : @[ @"isAccessibilityHidden", @"setAccessibilityHidden:" ],
  @"AXFrontmost" : @[ @"isAccessibilityFrontmost", @"setAccessibilityFrontmost:" ],
  @"AXFocusedWindow" : @[ @"accessibilityFocusedWindow", @"setAccessibilityFocusedWindow:" ],
  @"AXWindows" : @[ @"accessibilityWindows", @"setAccessibilityWindows:" ],
  @"AXExtrasMenuBar" : @[ @"accessibilityExtrasMenuBar", @"setAccessibilityExtrasMenuBar:" ],
  @"AXMenuBar" : @[ @"accessibilityMenuBar", @"setAccessibilityMenuBar:" ],
  @"AXColumnTitles" : @[ @"accessibilityColumnTitles", @"setAccessibilityColumnTitles:" ],
  @"AXOrderedByRow" : @[ @"isAccessibilityOrderedByRow", @"setAccessibilityOrderedByRow:" ],
  @"AXHorizontalUnits" : @[ @"accessibilityHorizontalUnits", @"setAccessibilityHorizontalUnits:" ],
  @"AXVerticalUnits" : @[ @"accessibilityVerticalUnits", @"setAccessibilityVerticalUnits:" ],
  @"AXHorizontalUnitDescription" :
      @[ @"accessibilityHorizontalUnitDescription", @"setAccessibilityHorizontalUnitDescription:" ],
  @"AXVerticalUnitDescription" :
      @[ @"accessibilityVerticalUnitDescription", @"setAccessibilityVerticalUnitDescription:" ],
  @"AXHandles" : @[ @"accessibilityHandles", @"setAccessibilityHandles:" ],
  @"AXWarningValue" : @[ @"accessibilityWarningValue", @"setAccessibilityWarningValue:" ],
  @"AXCriticalValue" : @[ @"accessibilityCriticalValue", @"setAccessibilityCriticalValue:" ],
  @"AXDisclosing" : @[ @"isAccessibilityDisclosed", @"setAccessibilityDisclosed:" ],
  @"AXDisclosedByRow" : @[ @"accessibilityDisclosedByRow", @"setAccessibilityDisclosedByRow:" ],
  @"AXDisclosedRows" : @[ @"accessibilityDisclosedRows", @"setAccessibilityDisclosedRows:" ],
  @"AXDisclosureLevel" : @[ @"accessibilityDisclosureLevel", @"setAccessibilityDisclosureLevel:" ],
  @"AXMarkerUIElements" :
      @[ @"accessibilityMarkerUIElements", @"setAccessibilityMarkerUIElements:" ],
  @"AXMarkerValues" : @[ @"accessibilityMarkerValues", @"setAccessibilityMarkerValues:" ],
  @"AXMarkerGroupUIElement" :
      @[ @"accessibilityMarkerGroupUIElement", @"setAccessibilityMarkerGroupUIElement:" ],
  @"AXUnits" : @[ @"accessibilityUnits", @"setAccessibilityUnits:" ],
  @"AXUnitDescription" : @[ @"accessibilityUnitDescription", @"setAccessibilityUnitDescription:" ],
  @"AXMarkerType" : @[ @"accessibilityRulerMarkerType", @"setAccessibilityRulerMarkerType:" ],
  @"AXMarkerTypeDescription" :
      @[ @"accessibilityMarkerTypeDescription", @"setAccessibilityMarkerTypeDescription:" ],
  @"AXHorizontalScrollBar" :
      @[ @"accessibilityHorizontalScrollBar", @"setAccessibilityHorizontalScrollBar:" ],
  @"AXVerticalScrollBar" :
      @[ @"accessibilityVerticalScrollBar", @"setAccessibilityVerticalScrollBar:" ],
  @"AXAllowedValues" : @[ @"accessibilityAllowedValues", @"setAccessibilityAllowedValues:" ],
  @"AXLabelUIElements" : @[ @"accessibilityLabelUIElements", @"setAccessibilityLabelUIElements:" ],
  @"AXLabelValue" : @[ @"accessibilityLabelValue", @"setAccessibilityLabelValue:" ],
  @"AXSplitters" : @[ @"accessibilitySplitters", @"setAccessibilitySplitters:" ],
  @"AXDecrementButton" : @[ @"accessibilityDecrementButton", @"setAccessibilityDecrementButton:" ],
  @"AXIncrementButton" : @[ @"accessibilityIncrementButton", @"setAccessibilityIncrementButton:" ],
  @"AXTabs" : @[ @"accessibilityTabs", @"setAccessibilityTabs:" ],
  @"AXHeader" : @[ @"accessibilityHeader", @"setAccessibilityHeader:" ],
  @"AXColumnCount" : @[ @"accessibilityColumnCount", @"setAccessibilityColumnCount:" ],
  @"AXRowCount" : @[ @"accessibilityRowCount", @"setAccessibilityRowCount:" ],
  @"AXIndex" : @[ @"accessibilityIndex", @"setAccessibilityIndex:" ],
  @"AXColumns" : @[ @"accessibilityColumns", @"setAccessibilityColumns:" ],
  @"AXRows" : @[ @"accessibilityRows", @"setAccessibilityRows:" ],
  @"AXVisibleRows" : @[ @"accessibilityVisibleRows", @"setAccessibilityVisibleRows:" ],
  @"AXSelectedRows" : @[ @"accessibilitySelectedRows", @"setAccessibilitySelectedRows:" ],
  @"AXVisibleColumns" : @[ @"accessibilityVisibleColumns", @"setAccessibilityVisibleColumns:" ],
  @"AXSelectedColumns" : @[ @"accessibilitySelectedColumns", @"setAccessibilitySelectedColumns:" ],
  @"AXSortDirection" : @[ @"accessibilitySortDirection", @"setAccessibilitySortDirection:" ],
  @"AXRowHeaderUIElements" :
      @[ @"accessibilityRowHeaderUIElements", @"setAccessibilityRowHeaderUIElements:" ],
  @"AXSelectedCells" : @[ @"accessibilitySelectedCells", @"setAccessibilitySelectedCells:" ],
  @"AXVisibleCells" : @[ @"accessibilityVisibleCells", @"setAccessibilityVisibleCells:" ],
  @"AXColumnHeaderUIElements" :
      @[ @"accessibilityColumnHeaderUIElements", @"setAccessibilityColumnHeaderUIElements:" ],
  @"AXRowIndexRange" : @[ @"accessibilityRowIndexRange", @"setAccessibilityRowIndexRange:" ],
  @"AXColumnIndexRange" :
      @[ @"accessibilityColumnIndexRange", @"setAccessibilityColumnIndexRange:" ],
  @"AXInsertionPointLineNumber" :
      @[ @"accessibilityInsertionPointLineNumber", @"setAccessibilityInsertionPointLineNumber:" ],
  @"AXSharedCharacterRange" :
      @[ @"accessibilitySharedCharacterRange", @"setAccessibilitySharedCharacterRange:" ],
  @"AXSharedTextUIElements" :
      @[ @"accessibilitySharedTextUIElements", @"setAccessibilitySharedTextUIElements:" ],
  @"AXVisibleCharacterRange" :
      @[ @"accessibilityVisibleCharacterRange", @"setAccessibilityVisibleCharacterRange:" ],
  @"AXNumberOfCharacters" :
      @[ @"accessibilityNumberOfCharacters", @"setAccessibilityNumberOfCharacters:" ],
  @"AXSelectedText" : @[ @"accessibilitySelectedText", @"setAccessibilitySelectedText:" ],
  @"AXSelectedTextRange" :
      @[ @"accessibilitySelectedTextRange", @"setAccessibilitySelectedTextRange:" ],
  @"AXSelectedTextRanges" :
      @[ @"accessibilitySelectedTextRanges", @"setAccessibilitySelectedTextRanges:" ],
  @"AXToolbarButton" : @[ @"accessibilityToolbarButton", @"setAccessibilityToolbarButton:" ],
  @"AXModal" : @[ @"isAccessibilityModal", @"setAccessibilityModal:" ],
  @"AXProxy" : @[ @"accessibilityProxy", @"setAccessibilityProxy:" ],
  @"AXMain" : @[ @"isAccessibilityMain", @"setAccessibilityMain:" ],
  @"AXFullScreenButton" :
      @[ @"accessibilityFullScreenButton", @"setAccessibilityFullScreenButton:" ],
  @"AXGrowArea" : @[ @"accessibilityGrowArea", @"setAccessibilityGrowArea:" ],
  @"AXDocument" : @[ @"accessibilityDocument", @"setAccessibilityDocument:" ],
  @"AXDefaultButton" : @[ @"accessibilityDefaultButton", @"setAccessibilityDefaultButton:" ],
  @"AXCloseButton" : @[ @"accessibilityCloseButton", @"setAccessibilityCloseButton:" ],
  @"AXZoomButton" : @[ @"accessibilityZoomButton", @"setAccessibilityZoomButton:" ],
  @"AXMinimizeButton" : @[ @"accessibilityMinimizeButton", @"setAccessibilityMinimizeButton:" ],
  @"AXMinimized" : @[ @"isAccessibilityMinimized", @"setAccessibilityMinimized:" ],
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

  for (NSString* key in gAttributeToPropertyMap) {
    if (SupportsSelector(NSSelectorFromString(gAttributeToPropertyMap[key][0]))) {
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
  if (NSArray* selectors = gAttributeToPropertyMap[attribName]) {
    SEL selector = NSSelectorFromString(selectors[0]);
    if (SupportsSelector(selector)) {
      return NSObjectToJsValue([mNativeObject performSelector:selector], aCx, aResult);
    }
  }

  return NSObjectToJsValue([mNativeObject accessibilityAttributeValue:attribName], aCx, aResult);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT
}

NS_IMETHODIMP
xpcAccessibleMacInterface::IsAttributeSettable(const nsAString& aAttributeName, bool* aIsSettable) {
  NS_ENSURE_ARG_POINTER(aIsSettable);

  NSString* attribName = nsCocoaUtils::ToNSString(aAttributeName);
  if (NSArray* selectors = gAttributeToPropertyMap[attribName]) {
    SEL selector = NSSelectorFromString(selectors[1]);
    if (SupportsSelector(selector)) {
      *aIsSettable = true;
      return NS_OK;
    }
  }

  if ([mNativeObject respondsToSelector:@selector(accessibilityIsAttributeSettable:)]) {
    *aIsSettable = [mNativeObject accessibilityIsAttributeSettable:attribName];
    return NS_OK;
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
xpcAccessibleMacInterface::SetAttributeValue(const nsAString& aAttributeName,
                                             JS::HandleValue aAttributeValue, JSContext* aCx) {
  nsresult rv = NS_OK;
  id obj = JsValueToNSObject(aAttributeValue, aCx, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  NSString* attribName = nsCocoaUtils::ToNSString(aAttributeName);
  if (NSArray* selectors = gAttributeToPropertyMap[attribName]) {
    SEL selector = NSSelectorFromString(selectors[1]);
    if (SupportsSelector(selector)) {
      if ([obj isKindOfClass:[NSValue class]]) {
        // If this is a simple data type the setter function doesn't
        // take an NSObject, but takes a primitive instead. We need to
        // do this NSInvocation dance to call it.
        NSMethodSignature* sig = [mNativeObject methodSignatureForSelector:selector];
        NSInvocation* inv = [NSInvocation invocationWithMethodSignature:sig];
        [inv setSelector:selector];
        [inv setTarget:mNativeObject];
        if (strcmp([(NSValue*)obj objCType], @encode(BOOL)) == 0) {
          BOOL arg = [obj boolValue];
          [inv setArgument:&arg atIndex:2];
        } else if (strcmp([(NSValue*)obj objCType], @encode(int)) == 0) {
          int arg = [obj intValue];
          [inv setArgument:&arg atIndex:2];
        } else {
          return NS_ERROR_FAILURE;
        }
        [inv invoke];
      } else {
        [mNativeObject performSelector:selector withObject:obj];
      }
      return NS_OK;
    }
  }

  if ([mNativeObject respondsToSelector:@selector(accessibilitySetValue:forAttribute:)]) {
    // The NSObject has an attribute setter, call that.
    [mNativeObject accessibilitySetValue:obj forAttribute:attribName];
    return NS_OK;
  }

  return NS_ERROR_NOT_IMPLEMENTED;
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
  } else if ([aObj respondsToSelector:@selector(isAccessibilityElement)]) {
    // We expect all of our accessibility objects to implement isAccessibilityElement
    // at the very least. If it is implemented we will assume its an accessibility object.
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

id xpcAccessibleMacInterface::JsValueToNSObject(JS::HandleValue aValue, JSContext* aCx,
                                                nsresult* aResult) {
  *aResult = NS_OK;
  if (aValue.isInt32()) {
    return [NSNumber numberWithInteger:aValue.toInt32()];
  } else if (aValue.isBoolean()) {
    return [NSNumber numberWithBool:aValue.toBoolean()];
  } else if (aValue.isObject()) {
    JS::Rooted<JSObject*> obj(aCx, aValue.toObjectOrNull());
    bool isArray;
    JS::IsArrayObject(aCx, obj, &isArray);
    if (isArray) {
      // If this is an array, we construct an NSArray and insert the js
      // array's elements by recursively calling this function.
      uint32_t len;
      JS::GetArrayLength(aCx, obj, &len);
      NSMutableArray* array = [NSMutableArray arrayWithCapacity:len];
      for (uint32_t i = 0; i < len; i++) {
        JS::RootedValue v(aCx);
        JS_GetElement(aCx, obj, i, &v);
        [array addObject:JsValueToNSObject(v, aCx, aResult)];
        NS_ENSURE_SUCCESS(*aResult, nil);
      }
      return array;
    } else {
      // This may be another nsIAccessibleMacInterface instance.
      // If so, return the wrapped NSObject.
      nsCOMPtr<nsIXPConnect> xpc = nsIXPConnect::XPConnect();

      nsCOMPtr<nsIXPConnectWrappedNative> wrappedObj;
      nsresult rv = xpc->GetWrappedNativeOfJSObject(aCx, obj, getter_AddRefs(wrappedObj));
      NS_ENSURE_SUCCESS(rv, nil);
      nsCOMPtr<nsIAccessibleMacInterface> macIface = do_QueryInterface(wrappedObj->Native());
      return macIface->GetNativeMacAccessible();
    }
  }

  *aResult = NS_ERROR_FAILURE;
  return nil;
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

id xpcAccessibleMacInterface::GetNativeMacAccessible() const { return mNativeObject; }
