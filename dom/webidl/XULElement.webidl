/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface XULControllers;

[HTMLConstructor, Func="IsChromeOrXBL"]
interface XULElement : Element {
  // Layout properties
  [SetterThrows]
  attribute DOMString align;
  [SetterThrows]
  attribute DOMString dir;
  [SetterThrows]
  attribute DOMString flex;
  [SetterThrows]
  attribute DOMString ordinal;
  [SetterThrows]
  attribute DOMString orient;
  [SetterThrows]
  attribute DOMString pack;

  // Properties for hiding elements.
  attribute boolean hidden;
  attribute boolean collapsed;

  // Property for hooking up to broadcasters
  [SetterThrows]
  attribute DOMString observes;

  // Properties for hooking up to popups
  [SetterThrows]
  attribute DOMString menu;
  [SetterThrows]
  attribute DOMString contextMenu;
  [SetterThrows]
  attribute DOMString tooltip;

  // Width/height properties
  [SetterThrows]
  attribute DOMString width;
  [SetterThrows]
  attribute DOMString height;
  [SetterThrows]
  attribute DOMString minWidth;
  [SetterThrows]
  attribute DOMString minHeight;
  [SetterThrows]
  attribute DOMString maxWidth;
  [SetterThrows]
  attribute DOMString maxHeight;

  // Position properties for
  // * popups - these are screen coordinates
  // * other elements - these are client coordinates relative to parent stack.
  [SetterThrows]
  attribute DOMString left;
  [SetterThrows]
  attribute DOMString top;

  // Return the screen coordinates of the element.
  readonly attribute long screenX;
  readonly attribute long screenY;

  // Tooltip
  [SetterThrows]
  attribute DOMString tooltipText;

  // Properties for images
  [SetterThrows]
  attribute DOMString src;

  attribute boolean allowEvents;

  [Throws, ChromeOnly]
  readonly attribute XULControllers             controllers;
  [Throws]
  readonly attribute BoxObject?                 boxObject;

  [SetterThrows]
  attribute long tabIndex;
  [Throws]
  void                      blur();
  [NeedsCallerType]
  void                      click();
  void                      doCommand();

  [Constant]
  readonly attribute CSSStyleDeclaration style;

  // Returns true if this is a menu-type element that has a menu
  // frame associated with it.
  boolean hasMenu();

  // If this is a menu-type element, opens or closes the menu
  // depending on the argument passed.
  void openMenu(boolean open);
};

XULElement implements GlobalEventHandlers;
XULElement implements HTMLOrSVGOrXULElementMixin;
XULElement implements TouchEventHandlers;
XULElement implements OnErrorEventHandlerForNodes;
