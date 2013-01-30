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
[TreatNonCallableAsNull]
callback EventHandlerNonNull = any (Event event);
typedef EventHandlerNonNull? EventHandler;

[TreatNonCallableAsNull]
callback BeforeUnloadEventHandlerNonNull = DOMString? (Event event);
typedef BeforeUnloadEventHandlerNonNull? BeforeUnloadEventHandler;

[TreatNonCallableAsNull]
callback OnErrorEventHandlerNonNull = boolean ((Event or DOMString) event, optional DOMString source, optional unsigned long lineno, optional unsigned long column);
typedef OnErrorEventHandlerNonNull? OnErrorEventHandler;

[NoInterfaceObject]
interface GlobalEventHandlers {
           [SetterThrows]
           attribute EventHandler onabort;
           //(Not implemented)[SetterThrows]
           //(Not implemented)attribute EventHandler oncancel;
           [SetterThrows]
           attribute EventHandler oncanplay;
           [SetterThrows]
           attribute EventHandler oncanplaythrough;
           [SetterThrows]
           attribute EventHandler onchange;
           [SetterThrows]
           attribute EventHandler onclick;
           //(Not implemented)[SetterThrows]
           //(Not implemented)attribute EventHandler onclose;
           [SetterThrows]
           attribute EventHandler oncontextmenu;
           //(Not implemented)[SetterThrows]
           //(Not implemented)attribute EventHandler oncuechange;
           [SetterThrows]
           attribute EventHandler ondblclick;
           [SetterThrows]
           attribute EventHandler ondrag;
           [SetterThrows]
           attribute EventHandler ondragend;
           [SetterThrows]
           attribute EventHandler ondragenter;
           [SetterThrows]
           attribute EventHandler ondragleave;
           [SetterThrows]
           attribute EventHandler ondragover;
           [SetterThrows]
           attribute EventHandler ondragstart;
           [SetterThrows]
           attribute EventHandler ondrop;
           [SetterThrows]
           attribute EventHandler ondurationchange;
           [SetterThrows]
           attribute EventHandler onemptied;
           [SetterThrows]
           attribute EventHandler onended;
           [SetterThrows]
           attribute EventHandler oninput;
           [SetterThrows]
           attribute EventHandler oninvalid;
           [SetterThrows]
           attribute EventHandler onkeydown;
           [SetterThrows]
           attribute EventHandler onkeypress;
           [SetterThrows]
           attribute EventHandler onkeyup;
           [SetterThrows]
           attribute EventHandler onloadeddata;
           [SetterThrows]
           attribute EventHandler onloadedmetadata;
           [SetterThrows]
           attribute EventHandler onloadstart;
           [SetterThrows]
           attribute EventHandler onmousedown;
           [SetterThrows]
           attribute EventHandler onmousemove;
           [SetterThrows]
           attribute EventHandler onmouseout;
           [SetterThrows]
           attribute EventHandler onmouseover;
           [SetterThrows]
           attribute EventHandler onmouseup;
           //(Not implemented)[SetterThrows]
           //(Not implemented)attribute EventHandler onmousewheel;
           [SetterThrows]
           attribute EventHandler onpause;
           [SetterThrows]
           attribute EventHandler onplay;
           [SetterThrows]
           attribute EventHandler onplaying;
           [SetterThrows]
           attribute EventHandler onprogress;
           [SetterThrows]
           attribute EventHandler onratechange;
           [SetterThrows]
           attribute EventHandler onreset;
           [SetterThrows]
           attribute EventHandler onseeked;
           [SetterThrows]
           attribute EventHandler onseeking;
           [SetterThrows]
           attribute EventHandler onselect;
           [SetterThrows]
           attribute EventHandler onshow;
           //(Not implemented)[SetterThrows]
           //(Not implemented)attribute EventHandler onsort;
           [SetterThrows]
           attribute EventHandler onstalled;
           [SetterThrows]
           attribute EventHandler onsubmit;
           [SetterThrows]
           attribute EventHandler onsuspend;
           [SetterThrows]
           attribute EventHandler ontimeupdate;
           [SetterThrows]
           attribute EventHandler onvolumechange;
           [SetterThrows]
           attribute EventHandler onwaiting;

           // Mozilla-specific handlers
           [SetterThrows]
           attribute EventHandler onmozfullscreenchange;
           [SetterThrows]
           attribute EventHandler onmozfullscreenerror;
           [SetterThrows]
           attribute EventHandler onmozpointerlockchange;
           [SetterThrows]
           attribute EventHandler onmozpointerlockerror;
};

[NoInterfaceObject]
interface NodeEventHandlers {
           [SetterThrows]
           attribute EventHandler onblur;
  // We think the spec is wrong here.
  //         attribute OnErrorEventHandler onerror;
           [SetterThrows]
           attribute EventHandler onerror;
           [SetterThrows]
           attribute EventHandler onfocus;
           [SetterThrows]
           attribute EventHandler onload;
           [SetterThrows]
           attribute EventHandler onscroll;
};

[NoInterfaceObject]
interface WindowEventHandlers {
           [SetterThrows]
           attribute EventHandler onafterprint;
           [SetterThrows]
           attribute EventHandler onbeforeprint;
           [SetterThrows]
           attribute BeforeUnloadEventHandler onbeforeunload;
  //       For now, onerror comes from NodeEventHandlers
  //       When we convert Window to WebIDL this may need to change.
  //       [SetterThrows]
  //       attribute OnErrorEventHandler onerror;
           //(Not implemented)[SetterThrows]
           //(Not implemented)attribute EventHandler onfullscreenchange;
           //(Not implemented)[SetterThrows]
           //(Not implemented)attribute EventHandler onfullscreenerror;
           [SetterThrows]
           attribute EventHandler onhashchange;
           [SetterThrows]
           attribute EventHandler onmessage;
           [SetterThrows]
           attribute EventHandler onoffline;
           [SetterThrows]
           attribute EventHandler ononline;
           [SetterThrows]
           attribute EventHandler onpagehide;
           [SetterThrows]
           attribute EventHandler onpageshow;
           [SetterThrows]
           attribute EventHandler onpopstate;
           [SetterThrows]
           attribute EventHandler onresize;
           //(Not implemented)[SetterThrows]
           //(Not implemented)attribute EventHandler onstorage;
           [SetterThrows]
           attribute EventHandler onunload;
};
