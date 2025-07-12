# Solid-XUL Integration Package

## Overview

The Solid-XUL package (`src/packages/solid-xul/`) is a crucial bridge that seamlessly connects SolidJS reactive components with Firefox's XUL (XML User Interface Language) system. This integration allows Floorp to maintain complete compatibility with Firefox's existing interface framework while providing a modern, reactive UI experience.

## Architecture

### Core Concept

SolidJS-XUL integration works by creating a bidirectional bridge between two different UI paradigms:

- **SolidJS**: Modern reactive framework with fine-grained reactivity
- **XUL**: Mozilla's XML-based UI system used in Firefox

```
┌─────────────────────────────────────────────────────────────┐
│                  Solid-XUL Architecture                      │
├─────────────────────────────────────────────────────────────┤
│  SolidJS Components (TypeScript/JSX)                        │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │ Reactive    │ │ Custom      │ │ Event Handling      │   │
│  │ Components  │ │ Hooks       │ │ & State Management  │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  Bridge Layer (solid-xul package)                           │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │ JSX         │ │ XUL         │ │ Event Bridge        │   │
│  │ Runtime     │ │ Elements    │ │ & API Wrappers      │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  XUL System (Firefox Native)                                │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │ XUL         │ │ XPCOM       │ │ Firefox APIs        │   │
│  │ Elements    │ │ Services    │ │ & Components        │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## Package Structure

```
src/packages/solid-xul/
├── index.ts               # Main API exports
├── jsx-runtime.ts         # Custom JSX runtime for XUL
├── elements/              # XUL element wrappers
│   ├── browser.ts         # Browser-specific elements
│   ├── toolbar.ts         # Toolbar elements
│   ├── menu.ts            # Menu and popup elements
│   └── common.ts          # Common XUL elements
├── hooks/                 # XUL/Firefox SolidJS hooks
│   ├── use-xul-element.ts # XUL element management
│   ├── use-firefox-api.ts # Firefox API integration
│   └── use-preferences.ts # Preferences management
├── utils/                 # Utility functions
│   ├── xul-helpers.ts     # XUL operation helpers
│   ├── event-bridge.ts    # Event system bridge
│   └── type-guards.ts     # Type checking utilities
├── types/                 # TypeScript type definitions
│   ├── xul.d.ts           # XUL element types
│   ├── firefox.d.ts       # Firefox API types
│   └── solid-xul.d.ts     # Package-specific types
├── package.json           # Package configuration
└── tsconfig.json          # TypeScript configuration
```

## Main API File

### 1. index.ts - Main API

The main entry point exports all public APIs and utilities.

```typescript
// Solid-XUL integration main exports
export * from "./jsx-runtime";
export * from "./elements";
export * from "./hooks";
export * from "./utils";

// Core integration functions
export { setupSolidXULIntegration } from "./core/setup";
export { createXULComponent } from "./core/component-factory";
export { XULProvider } from "./core/provider";

// Type exports
export type {
  XULElement,
  XULElementProps,
  SolidXULComponent,
  XULEventHandler,
} from "./types";

// Version information
export const VERSION = "1.0.0";

// Integration initialization
export async function initializeSolidXUL(): Promise<void> {
  // Setup XUL JSX runtime
  await setupJSXRuntime();

  // Register custom elements
  await registerXULElements();

  // Setup event bridge
  await setupEventBridge();

  console.log("Solid-XUL integration initialized");
}

// JSX runtime setup
async function setupJSXRuntime(): Promise<void> {
  // Configure SolidJS to work with XUL elements
  const { configureJSXRuntime } = await import("./jsx-runtime");
  configureJSXRuntime();
}

// Register XUL element wrappers
async function registerXULElements(): Promise<void> {
  const elements = await import("./elements");

  // Register all XUL element components
  Object.entries(elements).forEach(([name, component]) => {
    if (typeof component === "function") {
      // Register as custom element if needed
      const tagName = `solid-${name.toLowerCase()}`;
      if (!customElements.get(tagName)) {
        customElements.define(tagName, component);
      }
    }
  });
}

// Setup event bridge between SolidJS and XUL
async function setupEventBridge(): Promise<void> {
  const { initializeEventBridge } = await import("./utils/event-bridge");
  initializeEventBridge();
}
```

### 2. jsx-runtime.ts - Custom JSX Runtime

Custom JSX runtime that handles XUL elements alongside regular DOM elements.

```typescript
import { template, createComponent, mergeProps } from "solid-js/web";
import { isXULElement, createXULElement } from "./utils/xul-helpers";

// Custom JSX factory for XUL elements
export function jsx(type: any, props: any, key?: string) {
  // Check if this is a XUL element
  if (typeof type === "string" && isXULElement(type)) {
    return createXULComponent(type, props, key);
  }

  // Handle regular SolidJS components
  if (typeof type === "function") {
    return createComponent(type, props);
  }

  // Handle regular DOM elements
  return template(`<${type}></${type}>`, 1, false)(props, key);
}

// JSX fragment
export function jsxs(type: any, props: any, key?: string) {
  return jsx(type, props, key);
}

// Create XUL component wrapper
function createXULComponent(tagName: string, props: any, key?: string) {
  return () => {
    // Create XUL element
    const element = createXULElement(tagName);

    // Apply props to XUL element
    applyXULProps(element, props);

    // Handle children
    if (props.children) {
      handleXULChildren(element, props.children);
    }

    return element;
  };
}

// Apply props to XUL element
function applyXULProps(element: XULElement, props: any): void {
  Object.entries(props).forEach(([key, value]) => {
    if (key === "children") return;

    if (key.startsWith("on") && typeof value === "function") {
      // Handle event listeners
      const eventName = key.slice(2).toLowerCase();
      element.addEventListener(eventName, value);
    } else if (key === "ref") {
      // Handle refs
      if (typeof value === "function") {
        value(element);
      } else if (value) {
        value.current = element;
      }
    } else {
      // Handle attributes
      if (typeof value === "boolean") {
        if (value) {
          element.setAttribute(key, key);
        }
      } else if (value != null) {
        element.setAttribute(key, String(value));
      }
    }
  });
}

// Handle XUL children
function handleXULChildren(element: XULElement, children: any): void {
  if (Array.isArray(children)) {
    children.forEach((child) => appendXULChild(element, child));
  } else {
    appendXULChild(element, children);
  }
}

// Append child to XUL element
function appendXULChild(parent: XULElement, child: any): void {
  if (typeof child === "string" || typeof child === "number") {
    const textNode = document.createTextNode(String(child));
    parent.appendChild(textNode);
  } else if (child && typeof child === "object") {
    if (child.nodeType) {
      // DOM node
      parent.appendChild(child);
    } else if (typeof child === "function") {
      // SolidJS component
      const result = child();
      if (result) {
        appendXULChild(parent, result);
      }
    }
  }
}

// Configure JSX runtime for XUL
export function configureJSXRuntime(): void {
  // Override default JSX runtime settings for XUL compatibility
  if (typeof window !== "undefined" && window.document) {
    // Ensure XUL namespace is available
    if (!document.documentElement.lookupNamespaceURI("xul")) {
      document.documentElement.setAttributeNS(
        "http://www.w3.org/2000/xmlns/",
        "xmlns:xul",
        "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
      );
    }
  }
}
```

## XUL Element Components

### 1. Browser Elements (elements/browser.ts)

```typescript
import { Component, JSX, splitProps } from "solid-js";
import { XULElementProps } from "../types";

// Browser element wrapper
export interface BrowserProps extends XULElementProps {
  src?: string;
  type?: string;
  remote?: boolean;
  disablehistory?: boolean;
  disableglobalhistory?: boolean;
  messagemanagergroup?: string;
  onLocationChange?: (event: Event) => void;
  onStateChange?: (event: Event) => void;
  onSecurityChange?: (event: Event) => void;
}

export const Browser: Component<BrowserProps> = (props) => {
  const [xulProps, otherProps] = splitProps(props, [
    "src",
    "type",
    "remote",
    "disablehistory",
    "disableglobalhistory",
    "messagemanagergroup",
  ]);

  return (
    <browser {...xulProps} {...otherProps}>
      {props.children}
    </browser>
  );
};

// Tab browser wrapper
export interface TabBrowserProps extends XULElementProps {
  onTabOpen?: (event: Event) => void;
  onTabClose?: (event: Event) => void;
  onTabSelect?: (event: Event) => void;
  onTabMove?: (event: Event) => void;
}

export const TabBrowser: Component<TabBrowserProps> = (props) => {
  return (
    <tabbrowser
      onTabOpen={props.onTabOpen}
      onTabClose={props.onTabClose}
      onTabSelect={props.onTabSelect}
      onTabMove={props.onTabMove}
      {...props}
    >
      {props.children}
    </tabbrowser>
  );
};

// Tab element wrapper
export interface TabProps extends XULElementProps {
  label?: string;
  image?: string;
  pinned?: boolean;
  selected?: boolean;
  busy?: boolean;
  fadein?: boolean;
  onTabSelect?: (event: Event) => void;
  onTabClose?: (event: Event) => void;
}

export const Tab: Component<TabProps> = (props) => {
  return (
    <tab
      label={props.label}
      image={props.image}
      pinned={props.pinned}
      selected={props.selected}
      busy={props.busy}
      fadein={props.fadein}
      onTabSelect={props.onTabSelect}
      onTabClose={props.onTabClose}
      {...props}
    >
      {props.children}
    </tab>
  );
};
```

### 2. Toolbar Elements (elements/toolbar.ts)

```typescript
import { Component, JSX } from "solid-js";
import { XULElementProps } from "../types";

// Toolbar wrapper
export interface ToolbarProps extends XULElementProps {
  mode?: "icons" | "text" | "full";
  iconsize?: "small" | "large";
  defaultset?: string;
  currentset?: string;
  customizable?: boolean;
}

export const Toolbar: Component<ToolbarProps> = (props) => {
  return (
    <toolbar
      mode={props.mode}
      iconsize={props.iconsize}
      defaultset={props.defaultset}
      currentset={props.currentset}
      customizable={props.customizable}
      {...props}
    >
      {props.children}
    </toolbar>
  );
};

// Toolbarbutton wrapper
export interface ToolbarButtonProps extends XULElementProps {
  label?: string;
  image?: string;
  command?: string;
  type?: "button" | "menu" | "menu-button";
  disabled?: boolean;
  checked?: boolean;
  onCommand?: (event: Event) => void;
}

export const ToolbarButton: Component<ToolbarButtonProps> = (props) => {
  return (
    <toolbarbutton
      label={props.label}
      image={props.image}
      command={props.command}
      type={props.type}
      disabled={props.disabled}
      checked={props.checked}
      onCommand={props.onCommand}
      {...props}
    >
      {props.children}
    </toolbarbutton>
  );
};

// Toolbox wrapper
export interface ToolboxProps extends XULElementProps {
  mode?: "icons" | "text" | "full";
  iconsize?: "small" | "large";
}

export const Toolbox: Component<ToolboxProps> = (props) => {
  return (
    <toolbox mode={props.mode} iconsize={props.iconsize} {...props}>
      {props.children}
    </toolbox>
  );
};

// Spacer element
export interface SpacerProps extends XULElementProps {
  flex?: number;
}

export const Spacer: Component<SpacerProps> = (props) => {
  return <spacer flex={props.flex} {...props} />;
};
```

### 3. Menu Elements (elements/menu.ts)

```typescript
import { Component } from "solid-js";
import { XULElementProps } from "../types";

// Menubar wrapper
export interface MenubarProps extends XULElementProps {
  grippyhidden?: boolean;
}

export const Menubar: Component<MenubarProps> = (props) => {
  return (
    <menubar grippyhidden={props.grippyhidden} {...props}>
      {props.children}
    </menubar>
  );
};

// Menu wrapper
export interface MenuProps extends XULElementProps {
  label?: string;
  accesskey?: string;
  disabled?: boolean;
}

export const Menu: Component<MenuProps> = (props) => {
  return (
    <menu
      label={props.label}
      accesskey={props.accesskey}
      disabled={props.disabled}
      {...props}
    >
      {props.children}
    </menu>
  );
};

// Menupopup wrapper
export interface MenupopupProps extends XULElementProps {
  onPopupShowing?: (event: Event) => void;
  onPopupShown?: (event: Event) => void;
  onPopupHiding?: (event: Event) => void;
  onPopupHidden?: (event: Event) => void;
}

export const Menupopup: Component<MenupopupProps> = (props) => {
  return (
    <menupopup
      onPopupShowing={props.onPopupShowing}
      onPopupShown={props.onPopupShown}
      onPopupHiding={props.onPopupHiding}
      onPopupHidden={props.onPopupHidden}
      {...props}
    >
      {props.children}
    </menupopup>
  );
};

// Menuitem wrapper
export interface MenuitemProps extends XULElementProps {
  label?: string;
  accesskey?: string;
  key?: string;
  command?: string;
  type?: "checkbox" | "radio";
  checked?: boolean;
  disabled?: boolean;
  onCommand?: (event: Event) => void;
}

export const Menuitem: Component<MenuitemProps> = (props) => {
  return (
    <menuitem
      label={props.label}
      accesskey={props.accesskey}
      key={props.key}
      command={props.command}
      type={props.type}
      checked={props.checked}
      disabled={props.disabled}
      onCommand={props.onCommand}
      {...props}
    />
  );
};

// Menuseparator wrapper
export const Menuseparator: Component<XULElementProps> = (props) => {
  return <menuseparator {...props} />;
};
```

## Hook System

### 1. XUL Element Hook (hooks/use-xul-element.ts)

```typescript
import { createSignal, onMount, onCleanup, Accessor } from "solid-js";

// Hook for managing XUL elements
export function useXULElement<T extends XULElement>(
  selector: string
): [Accessor<T | null>, (element: T) => void] {
  const [element, setElement] = createSignal<T | null>(null);

  onMount(() => {
    // Find existing XUL element
    const xulElement = document.querySelector(selector) as T;
    if (xulElement) {
      setElement(xulElement);
    }

    // Setup mutation observer to watch for element changes
    const observer = new MutationObserver((mutations) => {
      mutations.forEach((mutation) => {
        if (mutation.type === "childList") {
          const newElement = document.querySelector(selector) as T;
          if (newElement !== element()) {
            setElement(newElement);
          }
        }
      });
    });

    observer.observe(document.body, {
      childList: true,
      subtree: true,
    });

    onCleanup(() => {
      observer.disconnect();
    });
  });

  return [element, setElement];
}

// Hook for XUL element attributes
export function useXULAttribute(
  element: Accessor<XULElement | null>,
  attributeName: string
): [Accessor<string | null>, (value: string | null) => void] {
  const [value, setValue] = createSignal<string | null>(null);

  // Update value when element changes
  createEffect(() => {
    const el = element();
    if (el) {
      const attrValue = el.getAttribute(attributeName);
      setValue(attrValue);
    }
  });

  // Function to set attribute value
  const setAttributeValue = (newValue: string | null) => {
    const el = element();
    if (el) {
      if (newValue === null) {
        el.removeAttribute(attributeName);
      } else {
        el.setAttribute(attributeName, newValue);
      }
      setValue(newValue);
    }
  };

  return [value, setAttributeValue];
}

// Hook for XUL element visibility
export function useXULVisibility(
  element: Accessor<XULElement | null>
): [Accessor<boolean>, (visible: boolean) => void] {
  const [isVisible, setIsVisible] = createSignal(true);

  createEffect(() => {
    const el = element();
    if (el) {
      const hidden = el.getAttribute("hidden") === "true";
      setIsVisible(!hidden);
    }
  });

  const setVisible = (visible: boolean) => {
    const el = element();
    if (el) {
      if (visible) {
        el.removeAttribute("hidden");
      } else {
        el.setAttribute("hidden", "true");
      }
      setIsVisible(visible);
    }
  };

  return [isVisible, setVisible];
}
```

### 2. Firefox API Hook (hooks/use-firefox-api.ts)

```typescript
import { createSignal, createEffect, onCleanup } from "solid-js";

// Hook for browser tabs
export function useBrowserTabs() {
  const [tabs, setTabs] = createSignal([]);
  const [selectedTab, setSelectedTab] = createSignal(null);

  const updateTabs = () => {
    if (window.gBrowser) {
      const allTabs = Array.from(window.gBrowser.tabs);
      setTabs(allTabs);
      setSelectedTab(window.gBrowser.selectedTab);
    }
  };

  onMount(() => {
    updateTabs();

    // Listen for tab events
    const tabContainer = window.gBrowser?.tabContainer;
    if (tabContainer) {
      const events = ["TabOpen", "TabClose", "TabSelect", "TabMove"];

      events.forEach((eventName) => {
        tabContainer.addEventListener(eventName, updateTabs);
      });

      onCleanup(() => {
        events.forEach((eventName) => {
          tabContainer.removeEventListener(eventName, updateTabs);
        });
      });
    }
  });

  const openTab = (url: string, options: { background?: boolean } = {}) => {
    if (window.gBrowser) {
      const tab = window.gBrowser.addTab(url, {
        inBackground: options.background,
      });

      if (!options.background) {
        window.gBrowser.selectedTab = tab;
      }

      return tab;
    }
  };

  const closeTab = (tab: any) => {
    if (window.gBrowser && tab) {
      window.gBrowser.removeTab(tab);
    }
  };

  return {
    tabs,
    selectedTab,
    openTab,
    closeTab,
    refresh: updateTabs,
  };
}

// Hook for bookmarks
export function useBookmarks() {
  const [bookmarks, setBookmarks] = createSignal([]);

  const loadBookmarks = async () => {
    try {
      if (window.PlacesUtils) {
        const bookmarkTree = await window.PlacesUtils.promiseBookmarksTree();
        setBookmarks(bookmarkTree.children || []);
      }
    } catch (error) {
      console.error("Failed to load bookmarks:", error);
    }
  };

  onMount(() => {
    loadBookmarks();

    // Listen for bookmark changes
    if (window.PlacesUtils?.bookmarks) {
      const observer = {
        onItemAdded: loadBookmarks,
        onItemRemoved: loadBookmarks,
        onItemChanged: loadBookmarks,
      };

      window.PlacesUtils.bookmarks.addObserver(observer);

      onCleanup(() => {
        window.PlacesUtils.bookmarks.removeObserver(observer);
      });
    }
  });

  const addBookmark = async (url: string, title: string, folder?: string) => {
    try {
      if (window.PlacesUtils?.bookmarks) {
        await window.PlacesUtils.bookmarks.insert({
          parentGuid: folder || window.PlacesUtils.bookmarks.unfiledGuid,
          url,
          title,
        });
        await loadBookmarks();
      }
    } catch (error) {
      console.error("Failed to add bookmark:", error);
    }
  };

  return {
    bookmarks,
    addBookmark,
    refresh: loadBookmarks,
  };
}

// Hook for downloads
export function useDownloads() {
  const [downloads, setDownloads] = createSignal([]);

  const loadDownloads = async () => {
    try {
      if (window.Downloads) {
        const downloadList = await window.Downloads.getList(
          window.Downloads.ALL
        );
        const allDownloads = await downloadList.getAll();
        setDownloads(allDownloads);
      }
    } catch (error) {
      console.error("Failed to load downloads:", error);
    }
  };

  onMount(() => {
    loadDownloads();
  });

  return {
    downloads,
    refresh: loadDownloads,
  };
}
```

## Utility Functions

### 1. XUL Helpers (utils/xul-helpers.ts)

```typescript
// XUL element detection and creation utilities

// List of XUL element tag names
const XUL_ELEMENTS = new Set([
  "browser",
  "tabbrowser",
  "tab",
  "tabs",
  "toolbar",
  "toolbarbutton",
  "toolbox",
  "spacer",
  "menubar",
  "menu",
  "menupopup",
  "menuitem",
  "menuseparator",
  "box",
  "hbox",
  "vbox",
  "deck",
  "stack",
  "splitter",
  "grippy",
  "statusbar",
  "statusbarpanel",
  "textbox",
  "checkbox",
  "radio",
  "button",
  "listbox",
  "listitem",
  "tree",
  "treecol",
  "treechildren",
  "richlistbox",
  "richlistitem",
  "description",
  "label",
  "progressmeter",
  "scale",
  "colorpicker",
  "datepicker",
  "wizard",
  "wizardpage",
  "prefwindow",
  "prefpane",
]);

// Check if a tag name is a XUL element
export function isXULElement(tagName: string): boolean {
  return XUL_ELEMENTS.has(tagName.toLowerCase());
}

// Create XUL element with proper namespace
export function createXULElement(tagName: string): XULElement {
  const XUL_NS =
    "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  return document.createElementNS(XUL_NS, tagName) as XULElement;
}

// Get XUL element by ID
export function getXULElement(id: string): XULElement | null {
  return document.getElementById(id) as XULElement | null;
}

// Find XUL elements by selector
export function findXULElements(selector: string): XULElement[] {
  return Array.from(document.querySelectorAll(selector)) as XULElement[];
}

// Set XUL attribute safely
export function setXULAttribute(
  element: XULElement,
  name: string,
  value: string | boolean | number
): void {
  if (typeof value === "boolean") {
    if (value) {
      element.setAttribute(name, name);
    } else {
      element.removeAttribute(name);
    }
  } else {
    element.setAttribute(name, String(value));
  }
}

// Get XUL attribute with type conversion
export function getXULAttribute(
  element: XULElement,
  name: string,
  type: "string"
): string | null;
export function getXULAttribute(
  element: XULElement,
  name: string,
  type: "boolean"
): boolean;
export function getXULAttribute(
  element: XULElement,
  name: string,
  type: "number"
): number | null;
export function getXULAttribute(
  element: XULElement,
  name: string,
  type: "string" | "boolean" | "number"
): any {
  const value = element.getAttribute(name);

  if (value === null) {
    return type === "boolean" ? false : null;
  }

  switch (type) {
    case "boolean":
      return value === "true" || value === name;
    case "number":
      const num = parseFloat(value);
      return isNaN(num) ? null : num;
    default:
      return value;
  }
}

// Insert SolidJS component into XUL element
export function insertSolidComponent(
  xulElement: XULElement,
  component: () => JSX.Element,
  position: "before" | "after" | "prepend" | "append" = "append"
): HTMLElement {
  const container = document.createElement("div");
  container.className = "solid-xul-container";

  // Position the container
  switch (position) {
    case "before":
      xulElement.parentNode?.insertBefore(container, xulElement);
      break;
    case "after":
      xulElement.parentNode?.insertBefore(container, xulElement.nextSibling);
      break;
    case "prepend":
      xulElement.insertBefore(container, xulElement.firstChild);
      break;
    case "append":
    default:
      xulElement.appendChild(container);
      break;
  }

  // Render SolidJS component
  render(component, container);

  return container;
}
```

### 2. Event Bridge (utils/event-bridge.ts)

```typescript
// Event system bridge between SolidJS and XUL

// Event mapping between XUL and standard events
const XUL_EVENT_MAP = new Map([
  ["command", "click"],
  ["select", "change"],
  ["input", "input"],
  ["focus", "focus"],
  ["blur", "blur"],
]);

// Initialize event bridge
export function initializeEventBridge(): void {
  // Setup global event delegation for XUL events
  document.addEventListener("command", handleXULCommand, true);
  document.addEventListener("select", handleXULSelect, true);

  console.log("Event bridge initialized");
}

// Handle XUL command events
function handleXULCommand(event: Event): void {
  const target = event.target as XULElement;

  // Check if target has SolidJS event handlers
  if (target && target._solidEventHandlers) {
    const handler = target._solidEventHandlers.onCommand;
    if (typeof handler === "function") {
      handler(event);
    }
  }
}

// Handle XUL select events
function handleXULSelect(event: Event): void {
  const target = event.target as XULElement;

  if (target && target._solidEventHandlers) {
    const handler = target._solidEventHandlers.onSelect;
    if (typeof handler === "function") {
      handler(event);
    }
  }
}

// Attach SolidJS event handler to XUL element
export function attachXULEventHandler(
  element: XULElement,
  eventName: string,
  handler: (event: Event) => void
): void {
  // Store handler reference
  if (!element._solidEventHandlers) {
    element._solidEventHandlers = {};
  }
  element._solidEventHandlers[eventName] = handler;

  // Map XUL event to standard event if needed
  const mappedEvent = XUL_EVENT_MAP.get(eventName.toLowerCase()) || eventName;

  // Add native event listener
  element.addEventListener(mappedEvent, handler);
}

// Remove SolidJS event handler from XUL element
export function removeXULEventHandler(
  element: XULElement,
  eventName: string
): void {
  if (element._solidEventHandlers) {
    const handler = element._solidEventHandlers[eventName];
    if (handler) {
      const mappedEvent =
        XUL_EVENT_MAP.get(eventName.toLowerCase()) || eventName;
      element.removeEventListener(mappedEvent, handler);
      delete element._solidEventHandlers[eventName];
    }
  }
}

// Create custom XUL event
export function createXULEvent(
  type: string,
  detail?: any,
  bubbles: boolean = true,
  cancelable: boolean = true
): CustomEvent {
  return new CustomEvent(type, {
    detail,
    bubbles,
    cancelable,
  });
}

// Dispatch XUL event
export function dispatchXULEvent(
  element: XULElement,
  event: Event | string,
  detail?: any
): boolean {
  if (typeof event === "string") {
    event = createXULEvent(event, detail);
  }

  return element.dispatchEvent(event);
}
```

## Usage Examples

### 1. Creating Custom Toolbar

```typescript
import { Component, createSignal } from "solid-js";
import { Toolbar, ToolbarButton, Spacer } from "@floorp/solid-xul";

const CustomToolbar: Component = () => {
  const [buttonCount, setButtonCount] = createSignal(0);

  const handleButtonClick = () => {
    setButtonCount((prev) => prev + 1);
    console.log(`Button clicked ${buttonCount()} times`);
  };

  return (
    <Toolbar id="custom-toolbar" mode="icons">
      <ToolbarButton
        label="Custom Action"
        image="chrome://floorp/skin/icons/custom.png"
        onCommand={handleButtonClick}
      />

      <Spacer flex={1} />

      <ToolbarButton label={`Clicks: ${buttonCount()}`} disabled={true} />
    </Toolbar>
  );
};
```

### 2. Tab Management Component

```typescript
import { Component, For } from "solid-js";
import { useBrowserTabs } from "@floorp/solid-xul";

const TabManager: Component = () => {
  const { tabs, selectedTab, openTab, closeTab } = useBrowserTabs();

  return (
    <div class="tab-manager">
      <h3>Open Tabs ({tabs().length})</h3>

      <For each={tabs()}>
        {(tab) => (
          <div class={`tab-item ${tab === selectedTab() ? "selected" : ""}`}>
            <span class="tab-title">{tab.label}</span>
            <button onClick={() => closeTab(tab)}>×</button>
          </div>
        )}
      </For>

      <button onClick={() => openTab("about:newtab")}>New Tab</button>
    </div>
  );
};
```

The Solid-XUL integration package provides a powerful bridge between modern reactive UI development and Firefox's native XUL system, enabling Floorp to deliver advanced browser features while maintaining complete compatibility with the Firefox ecosystem.
