/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.whatwg.org/specs/web-apps/current-work/#eventhandler
 *
 * © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and
 * Opera Software ASA. You are granted a license to use, reproduce
 * and create derivative works of this document.
 */
[TreatNonObjectAsNull]
callback EventHandlerNonNull = any (Event event);
typedef EventHandlerNonNull? EventHandler;

[TreatNonObjectAsNull]
// https://www.w3.org/Bugs/Public/show_bug.cgi?id=23489
//callback OnBeforeUnloadEventHandlerNonNull = DOMString (Event event);
callback OnBeforeUnloadEventHandlerNonNull = DOMString? (Event event);
typedef OnBeforeUnloadEventHandlerNonNull? OnBeforeUnloadEventHandler;

[TreatNonObjectAsNull]
callback OnErrorEventHandlerNonNull = boolean ((Event or DOMString) event, optional DOMString source, optional unsigned long lineno, optional unsigned long column, optional any error);
typedef OnErrorEventHandlerNonNull? OnErrorEventHandler;

[NoInterfaceObject]
interface GlobalEventHandlers {
           attribute EventHandler onabort;
           attribute EventHandler onblur;
// We think the spec is wrong here. See OnErrorEventHandlerForNodes/Window
// below.
//         attribute OnErrorEventHandler onerror;
           attribute EventHandler onfocus;
           //(Not implemented)attribute EventHandler oncancel;
           attribute EventHandler oncanplay;
           attribute EventHandler oncanplaythrough;
           attribute EventHandler onchange;
           attribute EventHandler onclick;
           //(Not implemented)attribute EventHandler onclose;
           attribute EventHandler oncontextmenu;
           //(Not implemented)attribute EventHandler oncuechange;
           attribute EventHandler ondblclick;
           attribute EventHandler ondrag;
           attribute EventHandler ondragend;
           attribute EventHandler ondragenter;
           attribute EventHandler ondragexit;
           attribute EventHandler ondragleave;
           attribute EventHandler ondragover;
           attribute EventHandler ondragstart;
           attribute EventHandler ondrop;
           attribute EventHandler ondurationchange;
           attribute EventHandler onemptied;
           attribute EventHandler onended;
           attribute EventHandler oninput;
           attribute EventHandler oninvalid;
           attribute EventHandler onkeydown;
           attribute EventHandler onkeypress;
           attribute EventHandler onkeyup;
           attribute EventHandler onload;
           attribute EventHandler onloadeddata;
           attribute EventHandler onloadedmetadata;
           attribute EventHandler onloadstart;
           attribute EventHandler onmousedown;
  [LenientThis] attribute EventHandler onmouseenter;
  [LenientThis] attribute EventHandler onmouseleave;
           attribute EventHandler onmousemove;
           attribute EventHandler onmouseout;
           attribute EventHandler onmouseover;
           attribute EventHandler onmouseup;
           //(Not implemented)attribute EventHandler onmousewheel;
           attribute EventHandler onpause;
           attribute EventHandler onplay;
           attribute EventHandler onplaying;
           attribute EventHandler onprogress;
           attribute EventHandler onratechange;
           attribute EventHandler onreset;
           attribute EventHandler onresize;
           attribute EventHandler onscroll;
           attribute EventHandler onseeked;
           attribute EventHandler onseeking;
           attribute EventHandler onselect;
           attribute EventHandler onshow;
           //(Not implemented)attribute EventHandler onsort;
           attribute EventHandler onstalled;
           attribute EventHandler onsubmit;
           attribute EventHandler onsuspend;
           attribute EventHandler ontimeupdate;
           attribute EventHandler onvolumechange;
           attribute EventHandler onwaiting;

           [Pref="dom.select_events.enabled"]
           attribute EventHandler onselectstart;

           [Pref="dom.details_element.enabled"]
           attribute EventHandler ontoggle;

           // Pointer events handlers
           [Pref="dom.w3c_pointer_events.enabled"]
           attribute EventHandler onpointercancel;
           [Pref="dom.w3c_pointer_events.enabled"]
           attribute EventHandler onpointerdown;
           [Pref="dom.w3c_pointer_events.enabled"]
           attribute EventHandler onpointerup;
           [Pref="dom.w3c_pointer_events.enabled"]
           attribute EventHandler onpointermove;
           [Pref="dom.w3c_pointer_events.enabled"]
           attribute EventHandler onpointerout;
           [Pref="dom.w3c_pointer_events.enabled"]
           attribute EventHandler onpointerover;
           [Pref="dom.w3c_pointer_events.enabled"]
           attribute EventHandler onpointerenter;
           [Pref="dom.w3c_pointer_events.enabled"]
           attribute EventHandler onpointerleave;
           [Pref="dom.w3c_pointer_events.enabled"]
           attribute EventHandler ongotpointercapture;
           [Pref="dom.w3c_pointer_events.enabled"]
           attribute EventHandler onlostpointercapture;

           // Mozilla-specific handlers. Unprefixed handlers live in
           // Document rather than here.
           attribute EventHandler onmozfullscreenchange;
           attribute EventHandler onmozfullscreenerror;
           [Pref="pointer-lock-api.prefixed.enabled"]
           attribute EventHandler onmozpointerlockchange;
           [Pref="pointer-lock-api.prefixed.enabled"]
           attribute EventHandler onmozpointerlockerror;
};

[NoInterfaceObject]
interface WindowEventHandlers {
           attribute EventHandler onafterprint;
           attribute EventHandler onbeforeprint;
           attribute OnBeforeUnloadEventHandler onbeforeunload;
           attribute EventHandler onhashchange;
           attribute EventHandler onlanguagechange;
           attribute EventHandler onmessage;
           attribute EventHandler onoffline;
           attribute EventHandler ononline;
           attribute EventHandler onpagehide;
           attribute EventHandler onpageshow;
           attribute EventHandler onpopstate;
           attribute EventHandler onstorage;
           attribute EventHandler onunload;
};

// The spec has |attribute OnErrorEventHandler onerror;| on
// GlobalEventHandlers, and calls the handler differently depending on
// whether an ErrorEvent was fired. We don't do that, and until we do we'll
// need to distinguish between onerror on Window or on nodes.

[NoInterfaceObject]
interface OnErrorEventHandlerForNodes {
           attribute EventHandler onerror;
};

[NoInterfaceObject]
interface OnErrorEventHandlerForWindow {
           attribute OnErrorEventHandler onerror;
};
