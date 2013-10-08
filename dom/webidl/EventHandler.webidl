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
callback OnBeforeUnloadEventHandlerNonNull = DOMString? (Event event);
typedef OnBeforeUnloadEventHandlerNonNull? OnBeforeUnloadEventHandler;

[TreatNonCallableAsNull]
callback OnErrorEventHandlerNonNull = boolean ((Event or DOMString) event, optional DOMString source, optional unsigned long lineno, optional unsigned long column);
typedef OnErrorEventHandlerNonNull? OnErrorEventHandler;

[NoInterfaceObject]
interface GlobalEventHandlers {
           attribute EventHandler onabort;
           //(Not implemented)[SetterThrows]
           //(Not implemented)attribute EventHandler oncancel;
           attribute EventHandler oncanplay;
           attribute EventHandler oncanplaythrough;
           attribute EventHandler onchange;
           attribute EventHandler onclick;
           //(Not implemented)[SetterThrows]
           //(Not implemented)attribute EventHandler onclose;
           attribute EventHandler oncontextmenu;
           //(Not implemented)[SetterThrows]
           //(Not implemented)attribute EventHandler oncuechange;
           attribute EventHandler ondblclick;
           attribute EventHandler ondrag;
           attribute EventHandler ondragend;
           attribute EventHandler ondragenter;
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
           attribute EventHandler onloadeddata;
           attribute EventHandler onloadedmetadata;
           attribute EventHandler onloadstart;
           attribute EventHandler onmousedown;
           attribute EventHandler onmousemove;
           attribute EventHandler onmouseout;
           attribute EventHandler onmouseover;
           attribute EventHandler onmouseup;
           //(Not implemented)[SetterThrows]
           //(Not implemented)attribute EventHandler onmousewheel;
           attribute EventHandler onpause;
           attribute EventHandler onplay;
           attribute EventHandler onplaying;
           attribute EventHandler onprogress;
           attribute EventHandler onratechange;
           attribute EventHandler onreset;
           attribute EventHandler onseeked;
           attribute EventHandler onseeking;
           attribute EventHandler onselect;
           attribute EventHandler onshow;
           //(Not implemented)[SetterThrows]
           //(Not implemented)attribute EventHandler onsort;
           attribute EventHandler onstalled;
           attribute EventHandler onsubmit;
           attribute EventHandler onsuspend;
           attribute EventHandler ontimeupdate;
           attribute EventHandler onvolumechange;
           attribute EventHandler onwaiting;

           // Mozilla-specific handlers
           attribute EventHandler onmozfullscreenchange;
           attribute EventHandler onmozfullscreenerror;
           attribute EventHandler onmozpointerlockchange;
           attribute EventHandler onmozpointerlockerror;
};

[NoInterfaceObject]
interface NodeEventHandlers {
           attribute EventHandler onblur;
  // We think the spec is wrong here.
  //         attribute OnErrorEventHandler onerror;
           attribute EventHandler onerror;
           attribute EventHandler onfocus;
           attribute EventHandler onload;
           attribute EventHandler onscroll;
};

[NoInterfaceObject]
interface WindowEventHandlers {
           attribute EventHandler onafterprint;
           attribute EventHandler onbeforeprint;
           attribute OnBeforeUnloadEventHandler onbeforeunload;
  //       For now, onerror comes from NodeEventHandlers
  //       When we convert Window to WebIDL this may need to change.
  //       [SetterThrows]
  //       attribute OnErrorEventHandler onerror;
           //(Not implemented)[SetterThrows]
           //(Not implemented)attribute EventHandler onfullscreenchange;
           //(Not implemented)[SetterThrows]
           //(Not implemented)attribute EventHandler onfullscreenerror;
           attribute EventHandler onhashchange;
           attribute EventHandler onmessage;
           attribute EventHandler onoffline;
           attribute EventHandler ononline;
           attribute EventHandler onpagehide;
           attribute EventHandler onpageshow;
           attribute EventHandler onpopstate;
           attribute EventHandler onresize;
           //(Not implemented)[SetterThrows]
           //(Not implemented)attribute EventHandler onstorage;
           attribute EventHandler onunload;
};
