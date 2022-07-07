/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SDKDeclarations.h"

@protocol MOXTextMarkerSupport;
@protocol mozAccessible;

// This protocol's primary use is for abstracting the NSAccessibility informal
// protocol into a formal internal API. Conforming classes get to choose a
// subset of the optional methods to implement. Those methods will be mapped to
// NSAccessibility attributes or actions. A conforming class can implement
// moxBlockSelector to control which of its implemented methods should be
// exposed to NSAccessibility.

@protocol MOXAccessible

// The deepest descendant of the accessible subtree that contains the specified
// point. Forwarded from accessibilityHitTest.
- (id _Nullable)moxHitTest:(NSPoint)point;

// The deepest descendant of the accessible subtree that has the focus.
// Forwarded from accessibilityFocusedUIElement.
- (id _Nullable)moxFocusedUIElement;

// Sends a notification to any observing assistive applications.
- (void)moxPostNotification:(NSString* _Nonnull)notification;

- (void)moxPostNotification:(NSString* _Nonnull)notification
               withUserInfo:(NSDictionary* _Nullable)userInfo;

// Return YES if selector should be considered not supported.
// Used in implementations such as:
// - accessibilityAttributeNames
// - accessibilityActionNames
// - accessibilityIsAttributeSettable
- (BOOL)moxBlockSelector:(SEL _Nonnull)selector;

// Returns a list of all children, doesn't do ignore filtering.
- (NSArray* _Nullable)moxChildren;

// Returns our parent, doesn't do ignore filtering.
- (id<mozAccessible> _Nullable)moxParent;

// This is called by isAccessibilityElement. If a subclass wants
// to alter the isAccessibilityElement return value, it must
// override this and not isAccessibilityElement directly.
- (BOOL)moxIgnoreWithParent:(id<MOXAccessible> _Nullable)parent;

// Should the child be ignored. This allows subclasses to determine
// what kinds of accessibles are valid children. This causes the child
// to be skipped, but the unignored descendants will be added to the
// container in the default children getter.
- (BOOL)moxIgnoreChild:(id<MOXAccessible> _Nullable)child;

// Return text delegate if it exists.
- (id<MOXTextMarkerSupport> _Nullable)moxTextMarkerDelegate;

// Return true if this accessible is a live region
- (BOOL)moxIsLiveRegion;

// Find the nearest ancestor that returns true with the given block function
- (id<MOXAccessible> _Nullable)moxFindAncestor:
    (BOOL (^_Nonnull)(id<MOXAccessible> _Nonnull moxAcc,
                      BOOL* _Nonnull stop))findBlock;

@optional

#pragma mark - AttributeGetters

// AXChildren
- (NSArray* _Nullable)moxUnignoredChildren;

// AXParent
- (id _Nullable)moxUnignoredParent;

// AXRole
- (NSString* _Nullable)moxRole;

// AXRoleDescription
- (NSString* _Nullable)moxRoleDescription;

// AXSubrole
- (NSString* _Nullable)moxSubrole;

// AXTitle
- (NSString* _Nullable)moxTitle;

// AXDescription
- (NSString* _Nullable)moxLabel;

// AXHelp
- (NSString* _Nullable)moxHelp;

// AXValue
- (id _Nullable)moxValue;

// AXValueDescription
- (NSString* _Nullable)moxValueDescription;

// AXSize
- (NSValue* _Nullable)moxSize;

// AXPosition
- (NSValue* _Nullable)moxPosition;

// AXEnabled
- (NSNumber* _Nullable)moxEnabled;

// AXFocused
- (NSNumber* _Nullable)moxFocused;

// AXWindow
- (id _Nullable)moxWindow;

// AXFrame
- (NSValue* _Nullable)moxFrame;

// AXTitleUIElement
- (id _Nullable)moxTitleUIElement;

// AXTopLevelUIElement
- (id _Nullable)moxTopLevelUIElement;

// AXHasPopup
- (NSNumber* _Nullable)moxHasPopup;

// AXARIACurrent
- (NSString* _Nullable)moxARIACurrent;

// AXSelected
- (NSNumber* _Nullable)moxSelected;

// AXRequired
- (NSNumber* _Nullable)moxRequired;

// AXElementBusy
- (NSNumber* _Nullable)moxElementBusy;

// AXLinkedUIElements
- (NSArray* _Nullable)moxLinkedUIElements;

// AXARIAControls
- (NSArray* _Nullable)moxARIAControls;

// AXDOMIdentifier
- (NSString* _Nullable)moxDOMIdentifier;

// AXURL
- (NSURL* _Nullable)moxURL;

// AXLinkUIElements
- (NSArray* _Nullable)moxLinkUIElements;

// AXPopupValue
- (NSString* _Nullable)moxPopupValue;

// AXVisited
- (NSNumber* _Nullable)moxVisited;

// AXExpanded
- (NSNumber* _Nullable)moxExpanded;

// AXMain
- (NSNumber* _Nullable)moxMain;

// AXMinimized
- (NSNumber* _Nullable)moxMinimized;

// AXSelectedChildren
- (NSArray* _Nullable)moxSelectedChildren;

// AXTabs
- (NSArray* _Nullable)moxTabs;

// AXContents
- (NSArray* _Nullable)moxContents;

// AXOrientation
- (NSString* _Nullable)moxOrientation;

// AXMenuItemMarkChar
- (NSString* _Nullable)moxMenuItemMarkChar;

// AXLoaded
- (NSNumber* _Nullable)moxLoaded;

// AXLoadingProgress
- (NSNumber* _Nullable)moxLoadingProgress;

// AXMinValue
- (id _Nullable)moxMinValue;

// AXMaxValue
- (id _Nullable)moxMaxValue;

// Webkit also implements the following:
// // AXCaretBrowsingEnabled
// - (NSString* _Nullable)moxCaretBrowsingEnabled;

// // AXLayoutCount
// - (NSString* _Nullable)moxLayoutCount;

// // AXWebSessionID
// - (NSString* _Nullable)moxWebSessionID;

// // AXPreventKeyboardDOMEventDispatch
// - (NSString* _Nullable)moxPreventKeyboardDOMEventDispatch;

// Table Attributes

// AXRowCount
- (NSNumber* _Nullable)moxRowCount;

// AXColumnCount
- (NSNumber* _Nullable)moxColumnCount;

// AXRows
- (NSArray* _Nullable)moxRows;

// AXColumns
- (NSArray* _Nullable)moxColumns;

// AXIndex
- (NSNumber* _Nullable)moxIndex;

// AXRowIndexRange
- (NSValue* _Nullable)moxRowIndexRange;

// AXColumnIndexRange
- (NSValue* _Nullable)moxColumnIndexRange;

// AXRowHeaderUIElements
- (NSArray* _Nullable)moxRowHeaderUIElements;

// AXColumnHeaderUIElements
- (NSArray* _Nullable)moxColumnHeaderUIElements;

// AXIdentifier
- (NSString* _Nullable)moxIdentifier;

// AXVisibleChildren
- (NSArray* _Nullable)moxVisibleChildren;

// Outline Attributes

// AXDisclosing
- (NSNumber* _Nullable)moxDisclosing;

// AXDisclosedByRow
- (id _Nullable)moxDisclosedByRow;

// AXDisclosureLevel
- (NSNumber* _Nullable)moxDisclosureLevel;

// AXDisclosedRows
- (NSArray* _Nullable)moxDisclosedRows;

// AXSelectedRows
- (NSArray* _Nullable)moxSelectedRows;

// Math Attributes

// AXMathRootRadicand
- (id _Nullable)moxMathRootRadicand;

// AXMathRootIndex
- (id _Nullable)moxMathRootIndex;

// AXMathFractionNumerator
- (id _Nullable)moxMathFractionNumerator;

// AXMathFractionDenominator
- (id _Nullable)moxMathFractionDenominator;

// AXMathLineThickness
- (NSNumber* _Nullable)moxMathLineThickness;

// AXMathBase
- (id _Nullable)moxMathBase;

// AXMathSubscript
- (id _Nullable)moxMathSubscript;

// AXMathSuperscript
- (id _Nullable)moxMathSuperscript;

// AXMathUnder
- (id _Nullable)moxMathUnder;

// AXMathOver
- (id _Nullable)moxMathOver;

// AXInvalid
- (NSString* _Nullable)moxInvalid;

// AXSelectedText
- (NSString* _Nullable)moxSelectedText;

// AXSelectedTextRange
- (NSValue* _Nullable)moxSelectedTextRange;

// AXNumberOfCharacters
- (NSNumber* _Nullable)moxNumberOfCharacters;

// AXVisibleCharacterRange
- (NSValue* _Nullable)moxVisibleCharacterRange;

// AXInsertionPointLineNumber
- (NSNumber* _Nullable)moxInsertionPointLineNumber;

// AXEditableAncestor
- (id _Nullable)moxEditableAncestor;

// AXHighestEditableAncestor
- (id _Nullable)moxHighestEditableAncestor;

// AXFocusableAncestor
- (id _Nullable)moxFocusableAncestor;

// AXARIAAtomic
- (NSNumber* _Nullable)moxARIAAtomic;

// AXARIALive
- (NSString* _Nullable)moxARIALive;

// AXARIARelevant
- (NSString* _Nullable)moxARIARelevant;

// AXMozDebugDescription
- (NSString* _Nullable)moxMozDebugDescription;

#pragma mark - AttributeSetters

// AXDisclosing
- (void)moxSetDisclosing:(NSNumber* _Nullable)disclosing;

// AXValue
- (void)moxSetValue:(id _Nullable)value;

// AXFocused
- (void)moxSetFocused:(NSNumber* _Nullable)focused;

// AXSelected
- (void)moxSetSelected:(NSNumber* _Nullable)selected;

// AXSelectedChildren
- (void)moxSetSelectedChildren:(NSArray* _Nullable)selectedChildren;

// AXSelectedText
- (void)moxSetSelectedText:(NSString* _Nullable)selectedText;

// AXSelectedTextRange
- (void)moxSetSelectedTextRange:(NSValue* _Nullable)selectedTextRange;

// AXVisibleCharacterRange
- (void)moxSetVisibleCharacterRange:(NSValue* _Nullable)visibleCharacterRange;

#pragma mark - Actions

// AXPress
- (void)moxPerformPress;

// AXShowMenu
- (void)moxPerformShowMenu;

// AXScrollToVisible
- (void)moxPerformScrollToVisible;

// AXIncrement
- (void)moxPerformIncrement;

// AXDecrement
- (void)moxPerformDecrement;

#pragma mark - ParameterizedAttributeGetters

// AXLineForIndex
- (NSNumber* _Nullable)moxLineForIndex:(NSNumber* _Nonnull)index;

// AXRangeForLine
- (NSValue* _Nullable)moxRangeForLine:(NSNumber* _Nonnull)line;

// AXStringForRange
- (NSString* _Nullable)moxStringForRange:(NSValue* _Nonnull)range;

// AXRangeForPosition
- (NSValue* _Nullable)moxRangeForPosition:(NSValue* _Nonnull)position;

// AXRangeForIndex
- (NSValue* _Nullable)moxRangeForIndex:(NSNumber* _Nonnull)index;

// AXBoundsForRange
- (NSValue* _Nullable)moxBoundsForRange:(NSValue* _Nonnull)range;

// AXRTFForRange
- (NSData* _Nullable)moxRTFForRange:(NSValue* _Nonnull)range;

// AXStyleRangeForIndex
- (NSValue* _Nullable)moxStyleRangeForIndex:(NSNumber* _Nonnull)index;

// AXAttributedStringForRange
- (NSAttributedString* _Nullable)moxAttributedStringForRange:
    (NSValue* _Nonnull)range;

// AXUIElementsForSearchPredicate
- (NSArray* _Nullable)moxUIElementsForSearchPredicate:
    (NSDictionary* _Nonnull)searchPredicate;

// AXUIElementCountForSearchPredicate
- (NSNumber* _Nullable)moxUIElementCountForSearchPredicate:
    (NSDictionary* _Nonnull)searchPredicate;

// AXCellForColumnAndRow
- (id _Nullable)moxCellForColumnAndRow:(NSArray* _Nonnull)columnAndRow;

// AXIndexForChildUIElement
- (NSNumber* _Nullable)moxIndexForChildUIElement:(id _Nonnull)child;

@end

// This protocol maps text marker and text marker range parameters to
// methods. It is implemented by a delegate of a MOXAccessible.
@protocol MOXTextMarkerSupport

#pragma mark - TextAttributeGetters

// AXStartTextMarker
- (AXTextMarkerRef _Nullable)moxStartTextMarker;

// AXEndTextMarker
- (AXTextMarkerRef _Nullable)moxEndTextMarker;

// AXSelectedTextMarkerRange
- (AXTextMarkerRangeRef _Nullable)moxSelectedTextMarkerRange;

#pragma mark - ParameterizedTextAttributeGetters

// AXLengthForTextMarkerRange
- (NSNumber* _Nullable)moxLengthForTextMarkerRange:
    (AXTextMarkerRangeRef _Nonnull)textMarkerRange;

// AXStringForTextMarkerRange
- (NSString* _Nullable)moxStringForTextMarkerRange:
    (AXTextMarkerRangeRef _Nonnull)textMarkerRange;

// AXTextMarkerRangeForUnorderedTextMarkers
- (AXTextMarkerRangeRef _Nullable)moxTextMarkerRangeForUnorderedTextMarkers:
    (NSArray* _Nonnull)textMarkers;

// AXLeftWordTextMarkerRangeForTextMarker
- (AXTextMarkerRangeRef _Nullable)moxLeftWordTextMarkerRangeForTextMarker:
    (AXTextMarkerRef _Nonnull)textMarker;

// AXRightWordTextMarkerRangeForTextMarker
- (AXTextMarkerRangeRef _Nullable)moxRightWordTextMarkerRangeForTextMarker:
    (AXTextMarkerRef _Nonnull)textMarker;

// AXStartTextMarkerForTextMarkerRange
- (AXTextMarkerRef _Nullable)moxStartTextMarkerForTextMarkerRange:
    (AXTextMarkerRangeRef _Nonnull)textMarkerRange;

// AXEndTextMarkerForTextMarkerRange
- (AXTextMarkerRef _Nullable)moxEndTextMarkerForTextMarkerRange:
    (AXTextMarkerRangeRef _Nonnull)textMarkerRange;

// AXNextTextMarkerForTextMarker
- (AXTextMarkerRef _Nullable)moxNextTextMarkerForTextMarker:
    (AXTextMarkerRef _Nonnull)textMarker;

// AXPreviousTextMarkerForTextMarker
- (AXTextMarkerRef _Nullable)moxPreviousTextMarkerForTextMarker:
    (AXTextMarkerRef _Nonnull)textMarker;

// AXAttributedStringForTextMarkerRange
- (NSAttributedString* _Nullable)moxAttributedStringForTextMarkerRange:
    (AXTextMarkerRangeRef _Nonnull)textMarkerRange;

// AXBoundsForTextMarkerRange
- (NSValue* _Nullable)moxBoundsForTextMarkerRange:
    (AXTextMarkerRangeRef _Nonnull)textMarkerRange;

// AXIndexForTextMarker
- (NSNumber* _Nullable)moxIndexForTextMarker:
    (AXTextMarkerRef _Nonnull)textMarker;

// AXTextMarkerForIndex
- (AXTextMarkerRef _Nullable)moxTextMarkerForIndex:(NSNumber* _Nonnull)index;

// AXUIElementForTextMarker
- (id _Nullable)moxUIElementForTextMarker:(AXTextMarkerRef _Nonnull)textMarker;

// AXTextMarkerRangeForUIElement
- (AXTextMarkerRangeRef _Nullable)moxTextMarkerRangeForUIElement:
    (id _Nonnull)element;

// AXLineTextMarkerRangeForTextMarker
- (AXTextMarkerRangeRef _Nullable)moxLineTextMarkerRangeForTextMarker:
    (AXTextMarkerRef _Nonnull)textMarker;

// AXLeftLineTextMarkerRangeForTextMarker
- (AXTextMarkerRangeRef _Nullable)moxLeftLineTextMarkerRangeForTextMarker:
    (AXTextMarkerRef _Nonnull)textMarker;

// AXRightLineTextMarkerRangeForTextMarker
- (AXTextMarkerRangeRef _Nullable)moxRightLineTextMarkerRangeForTextMarker:
    (AXTextMarkerRef _Nonnull)textMarker;

// AXParagraphTextMarkerRangeForTextMarker
- (AXTextMarkerRangeRef _Nullable)moxParagraphTextMarkerRangeForTextMarker:
    (AXTextMarkerRef _Nonnull)textMarker;

// AXStyleTextMarkerRangeForTextMarker
- (AXTextMarkerRangeRef _Nullable)moxStyleTextMarkerRangeForTextMarker:
    (AXTextMarkerRef _Nonnull)textMarker;

// AXMozDebugDescriptionForTextMarker
- (NSString* _Nullable)moxMozDebugDescriptionForTextMarker:
    (AXTextMarkerRef _Nonnull)textMarker;

// AXMozDebugDescriptionForTextMarkerRange
- (NSString* _Nullable)moxMozDebugDescriptionForTextMarkerRange:
    (AXTextMarkerRangeRef _Nonnull)textMarkerRange;

#pragma mark - TextAttributeSetters

// AXSelectedTextMarkerRange
- (void)moxSetSelectedTextMarkerRange:(id _Nullable)textMarkerRange;

@end
