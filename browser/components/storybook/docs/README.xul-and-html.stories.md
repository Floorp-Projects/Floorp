# XUL and HTML

This document gives a quick overview of XUL and HTML, especially as it pertains to desktop front-end developers.
As we migrate away from XUL elements to HTML elements where possible, it is important to note the differences between these two technologies.
Additionally it is helpful to know how to use both where needed, as some elements will still need to use XUL.

## What is XUL

XUL is an XML-based language for building cross-platform user interfaces and applications, so all the features of XML are available in XUL as well.
This is in contrast to HTML which is intended for developing web pages.
Because of this XUL is oriented towards application artifacts such as windows, scrollbars, and menus instead of pages, headings, links, etc.
These XUL elements are provided to an HTML page without the page having any control over them.

XUL was and is used to create various UI elements, for example:
- Input controls such as textboxes and checkboxes
- Toolbars with buttons or other content
- Menus on a menu bar or pop up menus
- Tabbed dialogs
- Trees for hierarchical or tabular information

XUL is a Mozilla-specific technology with many similarities but also many differences to HTML.
These include a different box model (although it is now synthesized in the HTML box model) and the ability to be backed by C++ code.

## What requires XUL

While many of the existing XUL elements that make up the browser can be migrated to HTML, there are some elements that require XUL.
These elements tend to be fundamental to the browser such as windows, popups, panels, etc.
Elements that need to emulate OS-specific styling also tend to be XUL elements.
While there are parts of these elements that must be XUL, that does not mean that the component must be entirely implmented in XUL.
For example, we require that a `panel` can be drawn outside of a window's bounds, but then we can have HTML inside of that `panel` element.

The following is not an exhaustive list of elements that require XUL:
- Browser Window
  - https://searchfox.org/mozilla-central/source/xpfe/appshell/nsIXULBrowserWindow.idl
- Popups
  - https://searchfox.org/mozilla-central/source/dom/webidl/XULPopupElement.webidl
  - https://searchfox.org/mozilla-central/source/layout/xul/nsMenuPopupFrame.cpp
  - https://searchfox.org/mozilla-central/source/toolkit/content/widgets/autocomplete-popup.js
  - https://searchfox.org/mozilla-central/source/toolkit/content/widgets/menupopup.js
- Panels
  - https://searchfox.org/mozilla-central/source/toolkit/content/widgets/panel.js

## When to use HTML or XUL

Now that HTML is powerful enough for us to create almost an entire application with it, the need for the features of XUL has diminished.
We now prefer to write HTML components over XUL components since that model is better understood by the web and front-end community.
This also allows us to gain new features of the web in the UI that we write without backporting them to XUL.

There are some cases where XUL may still be required for non-standard functionality.
Some XUL elements have more functions over similar HTML elements, such as the XUL `<browser>` element compared to the HTML `<iframe>` element.
The XUL `<panel>` can exceed the bounds of its parent document and is useful for dropdowns, menus, and similar situations.
In these situations, it would be appropriate to use XUL elements.
Otherwise, we want to [start replacing XUL elements with HTML elements](https://bugzilla.mozilla.org/show_bug.cgi?id=1563415).
Because of this, new code **should use HTML whenever possible**.

## Mixing HTML and XUL

There are a few things you must do in order to use HTML in a XUL file.
First, you must add the following attribute to the `window` tag of the XUL file, or to the outermost HTML element.
`xmlns:html="http://www.w3.org/1999/xhtml`.
Using this allows Firefox (or other applications using Gecko) to distinguish the HTML tags from the XUL ones.
Second, any HTML element used must be prefixed with `html:` otherwise the element will be parsed as a XUL element.
For example, to use a HTML anchor, you would declare it as `<html:a>`.
Third, any tags or attributes you use have to be in lowercase as XML is case-sensitive.

Please note [you cannot declare XUL in HTML markup](https://searchfox.org/mozilla-central/rev/7a4c08f2c3a895c9dc064734ada320f920250c1f/toolkit/content/tests/widgets/test_panel_list_in_xul_panel.html#35-36).
You can use `document.createXULElement()` to programmatically create a XUL element in chrome privileged documents.
