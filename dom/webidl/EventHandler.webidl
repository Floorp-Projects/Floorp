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
callback OnBeforeUnloadEventHandlerNonNull = DOMString? (Event event);
typedef OnBeforeUnloadEventHandlerNonNull? OnBeforeUnloadEventHandler;

[TreatNonObjectAsNull]
callback OnErrorEventHandlerNonNull = any ((Event or DOMString) event, optional DOMString source, optional unsigned long lineno, optional unsigned long column, optional any error);
typedef OnErrorEventHandlerNonNull? OnErrorEventHandler;

interface mixin GlobalEventHandlers {
           attribute EventHandler onabort;
           attribute EventHandler onblur;
// We think the spec is wrong here. See OnErrorEventHandlerForNodes/Window
// below.
//         attribute OnErrorEventHandler onerror;
           attribute EventHandler onfocus;
           //(Not implemented)attribute EventHandler oncancel;
           attribute EventHandler onauxclick;
           [Pref="dom.input_events.beforeinput.enabled"]
           attribute EventHandler onbeforeinput;
           attribute EventHandler oncanplay;
           attribute EventHandler oncanplaythrough;
           attribute EventHandler onchange;
           attribute EventHandler onclick;
           attribute EventHandler onclose;
           attribute EventHandler oncontextmenu;
           attribute EventHandler oncuechange;
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
           attribute EventHandler onformdata;
           attribute EventHandler oninput;
           attribute EventHandler oninvalid;
           attribute EventHandler onkeydown;
           attribute EventHandler onkeypress;
           attribute EventHandler onkeyup;
           attribute EventHandler onload;
           attribute EventHandler onloadeddata;
           attribute EventHandler onloadedmetadata;
           attribute EventHandler onloadend;
           attribute EventHandler onloadstart;
           attribute EventHandler onmousedown;
  [LenientThis] attribute EventHandler onmouseenter;
  [LenientThis] attribute EventHandler onmouseleave;
           attribute EventHandler onmousemove;
           attribute EventHandler onmouseout;
           attribute EventHandler onmouseover;
           attribute EventHandler onmouseup;
           attribute EventHandler onwheel;
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
           [Pref="dom.menuitem.enabled"]
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

           attribute EventHandler ontoggle;

           // Pointer events handlers
           attribute EventHandler onpointercancel;
           attribute EventHandler onpointerdown;
           attribute EventHandler onpointerup;
           attribute EventHandler onpointermove;
           attribute EventHandler onpointerout;
           attribute EventHandler onpointerover;
           attribute EventHandler onpointerenter;
           attribute EventHandler onpointerleave;
           attribute EventHandler ongotpointercapture;
           attribute EventHandler onlostpointercapture;

           // Mozilla-specific handlers. Unprefixed handlers live in
           // Document rather than here.
           [Deprecated="MozfullscreenchangeDeprecatedPrefix"]
           attribute EventHandler onmozfullscreenchange;
           [Deprecated="MozfullscreenerrorDeprecatedPrefix"]
           attribute EventHandler onmozfullscreenerror;

           // CSS-Animation and CSS-Transition handlers.
           attribute EventHandler onanimationcancel;
           attribute EventHandler onanimationend;
           attribute EventHandler onanimationiteration;
           attribute EventHandler onanimationstart;
           attribute EventHandler ontransitioncancel;
           attribute EventHandler ontransitionend;
           attribute EventHandler ontransitionrun;
           attribute EventHandler ontransitionstart;

           // CSS-Animation and CSS-Transition legacy handlers.
           // This handler isn't standard.
           [BinaryName="onwebkitAnimationEnd"]
           attribute EventHandler onwebkitanimationend;
           [BinaryName="onwebkitAnimationIteration"]
           attribute EventHandler onwebkitanimationiteration;
           [BinaryName="onwebkitAnimationStart"]
           attribute EventHandler onwebkitanimationstart;
           [BinaryName="onwebkitTransitionEnd"]
           attribute EventHandler onwebkittransitionend;
};

interface mixin WindowEventHandlers {
           attribute EventHandler onafterprint;
           attribute EventHandler onbeforeprint;
           attribute OnBeforeUnloadEventHandler onbeforeunload;
           attribute EventHandler onhashchange;
           attribute EventHandler onlanguagechange;
           attribute EventHandler onmessage;
           attribute EventHandler onmessageerror;
           attribute EventHandler onoffline;
           attribute EventHandler ononline;
           attribute EventHandler onpagehide;
           attribute EventHandler onpageshow;
           attribute EventHandler onpopstate;
           attribute EventHandler onrejectionhandled;
           attribute EventHandler onstorage;
           attribute EventHandler onunhandledrejection;
           attribute EventHandler onunload;
};

// https://w3c.github.io/gamepad/#extensions-to-the-windoweventhandlers-interface-mixin
partial interface mixin WindowEventHandlers {
  attribute EventHandler ongamepadconnected;
  attribute EventHandler ongamepaddisconnected;
};

interface mixin DocumentAndElementEventHandlers {
  attribute EventHandler oncopy;
  attribute EventHandler oncut;
  attribute EventHandler onpaste;
};

// The spec has |attribute OnErrorEventHandler onerror;| on
// GlobalEventHandlers, and calls the handler differently depending on
// whether an ErrorEvent was fired. We don't do that, and until we do we'll
// need to distinguish between onerror on Window or on nodes.

interface mixin OnErrorEventHandlerForNodes {
           attribute EventHandler onerror;
};

interface mixin OnErrorEventHandlerForWindow {
           attribute OnErrorEventHandler onerror;
};
