/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * This file contains the list of event names that are exposed via IDL
 * on various objects.  It is designed to be used as inline input to
 * various consumers through the magic of C preprocessing.
 *
 * Each entry is the name of an event.
 *
 * Event names that are exposed as content attributes on HTML elements
 * and as IDL attributes on Elements, Documents and Windows and have
 * no forwarding behavior should be enclosed in the EVENT macro.
 *
 * Event names that are exposed as content attributes on HTML elements
 * and as IDL attributes on Elements, Documents and Windows and are
 * forwarded from <body> and <frameset> to the Window should be
 * enclosed in the FORWARDED_EVENT macro.  If this macro is not
 * defined, it will be defined to be equivalent to EVENT.
 *
 * Event names that are exposed as IDL attributes on Windows only
 * should be enclosed in the WINDOW_ONLY_EVENT macro.  If this macro
 * is not defined, it will be defined to the empty string.
 *
 * Event names that are exposed as content and IDL attributes on
 * <body> and <frameset>, which forward them to the Window, and are
 * exposed as IDL attributes on the Window should be enclosed in the
 * WINDOW_EVENT macro.  If this macro is not defined, it will be
 * defined to be equivalent to WINDOW_ONLY_EVENT.
 *
 * Touch-specific event names should be enclosed in TOUCH_EVENT.  They
 * are otherwise equivalent to those enclosed in EVENT.  If
 * TOUCH_EVENT is not defined, it will be defined to the empty string.
 */

#ifdef DEFINED_FORWARDED_EVENT
#error "Don't define DEFINED_FORWARDED_EVENT"
#endif /* DEFINED_FORWARDED_EVENT */

#ifndef FORWARDED_EVENT
#define FORWARDED_EVENT(_name) EVENT(_name)
#define DEFINED_FORWARDED_EVENT
#endif /* FORWARDED_EVENT */

#ifdef DEFINED_WINDOW_ONLY_EVENT
#error "Don't define DEFINED_WINDOW_ONLY_EVENT"
#endif /* DEFINED_WINDOW_ONLY_EVENT */

#ifndef WINDOW_ONLY_EVENT
#define WINDOW_ONLY_EVENT(_name)
#define DEFINED_WINDOW_ONLY_EVENT
#endif /* WINDOW_ONLY_EVENT */

#ifdef DEFINED_WINDOW_EVENT
#error "Don't define DEFINED_WINDOW_EVENT"
#endif /* DEFINED_WINDOW_EVENT */

#ifndef WINDOW_EVENT
#define WINDOW_EVENT(_name) WINDOW_ONLY_EVENT(_name)
#define DEFINED_WINDOW_EVENT
#endif /* WINDOW_EVENT */

#ifdef DEFINED_TOUCH_EVENT
#error "Don't define DEFINED_TOUCH_EVENT"
#endif /* DEFINED_TOUCH_EVENT */

#ifndef TOUCH_EVENT
#define TOUCH_EVENT(_name)
#define DEFINED_TOUCH_EVENT
#endif /* TOUCH_EVENT */

EVENT(abort)
EVENT(canplay)
EVENT(canplaythrough)
EVENT(change)
EVENT(click)
EVENT(contextmenu)
EVENT(cuechange)
EVENT(dblclick)
EVENT(drag)
EVENT(dragend)
EVENT(dragenter)
EVENT(dragleave)
EVENT(dragover)
EVENT(dragstart)
EVENT(drop)
EVENT(durationchange)
EVENT(emptied)
EVENT(ended)
EVENT(input)
EVENT(invalid)
EVENT(keydown)
EVENT(keypress)
EVENT(keyup)
EVENT(loadeddata)
EVENT(loadedmetadata)
EVENT(loadstart)
EVENT(mousedown)
EVENT(mousemove)
EVENT(mouseout)
EVENT(mouseover)
EVENT(mouseup)
EVENT(mousewheel)
EVENT(pause)
EVENT(play)
EVENT(playing)
EVENT(progress)
EVENT(ratechange)
EVENT(readystatechange)
EVENT(reset)
EVENT(seeked)
EVENT(seeking)
EVENT(select)
EVENT(show)
EVENT(stalled)
EVENT(submit)
EVENT(suspend)
EVENT(timeupdate)
EVENT(volumechange)
EVENT(waiting)
// Gecko-specific extensions that apply to elements
EVENT(copy)
EVENT(cut)
EVENT(paste)
EVENT(beforescriptexecute)
EVENT(afterscriptexecute)

FORWARDED_EVENT(blur)
FORWARDED_EVENT(error)
FORWARDED_EVENT(focus)
FORWARDED_EVENT(load)
FORWARDED_EVENT(scroll)

WINDOW_EVENT(afterprint)
WINDOW_EVENT(beforeprint)
WINDOW_EVENT(beforeunload)
WINDOW_EVENT(hashchange)
WINDOW_EVENT(message)
WINDOW_EVENT(offline)
WINDOW_EVENT(online)
WINDOW_EVENT(pagehide)
WINDOW_EVENT(pageshow)
WINDOW_EVENT(popstate)
WINDOW_EVENT(redo)
WINDOW_EVENT(resize)
WINDOW_EVENT(storage)
WINDOW_EVENT(undo)
WINDOW_EVENT(unload)

WINDOW_ONLY_EVENT(devicemotion)
WINDOW_ONLY_EVENT(deviceorientation)

TOUCH_EVENT(touchstart)
TOUCH_EVENT(touchend)
TOUCH_EVENT(touchmove)
TOUCH_EVENT(touchenter)
TOUCH_EVENT(touchleave)
TOUCH_EVENT(touchcancel)

#ifdef DEFINED_FORWARDED_EVENT
#undef DEFINED_FORWARDED_EVENT
#undef FORWARDED_EVENT
#endif /* DEFINED_FORWARDED_EVENT */

#ifdef DEFINED_WINDOW_EVENT
#undef DEFINED_WINDOW_EVENT
#undef WINDOW_EVENT
#endif /* DEFINED_WINDOW_EVENT */

#ifdef DEFINED_WINDOW_ONLY_EVENT
#undef DEFINED_WINDOW_ONLY_EVENT
#undef WINDOW_ONLY_EVENT
#endif /* DEFINED_WINDOW_ONLY_EVENT */

#ifdef DEFINED_TOUCH_EVENT
#undef DEFINED_TOUCH_EVENT
#undef TOUCH_EVENT
#endif /* DEFINED_TOUCH_EVENT */

