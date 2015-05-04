/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file contains the list of event names that are exposed via IDL
 * on various objects.  It is designed to be used as inline input to
 * various consumers through the magic of C preprocessing.
 *
 * Each entry consists of 4 pieces of information:
 * 1) The name of the event
 * 2) The event ID (see BasicEvents.h)
 * 3) The event type (see the EventNameType enum in nsContentUtils.h)
 * 4) The event struct type for this event.
 * Items 2-4 might be empty strings for events for which they don't make sense.
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
 *
 * Event names that are only exposed as IDL attributes on Documents
 * should be enclosed in the DOCUMENT_ONLY_EVENT macro.  If this macro is
 * not defined, it will be defined to the empty string.
 *
 * Event names that are not exposed as IDL attributes at all should be
 * enclosed in NON_IDL_EVENT.  If NON_IDL_EVENT is not defined, it
 * will be defined to the empty string.
 *
 * If you change which macros event names are enclosed in, please
 * update the tests for bug 689564 and bug 659350 as needed.
 */

#ifdef ID_TO_EVENT
#ifdef EVENT
#error "Don't define EVENT"
#endif /* EVENT */
#ifdef WINDOW_ONLY_EVENT
#error "Don't define WINDOW_ONLY_EVENT"
#endif /* WINDOW_ONLY_EVENT */
#ifdef TOUCH_EVENT
#error "Don't define TOUCH_EVENT"
#endif /* TOUCH_EVENT */
#ifdef DOCUMENT_ONLY_EVENT
#error "Don't define DOCUMENT_ONLY_EVENT"
#endif /* DOCUMENT_ONLY_EVENT */
#ifdef NON_IDL_EVENT
#error "Don't define NON_IDL_EVENT"
#endif /* NON_IDL_EVENT */

#define EVENT ID_TO_EVENT
#define WINDOW_ONLY_EVENT ID_TO_EVENT
#define TOUCH_EVENT ID_TO_EVENT
#define DOCUMENT_ONLY_EVENT ID_TO_EVENT
#define NON_IDL_EVENT ID_TO_EVENT
#endif

#ifdef DEFINED_FORWARDED_EVENT
#error "Don't define DEFINED_FORWARDED_EVENT"
#endif /* DEFINED_FORWARDED_EVENT */

#ifndef FORWARDED_EVENT
#define FORWARDED_EVENT EVENT
#define DEFINED_FORWARDED_EVENT
#endif /* FORWARDED_EVENT */

#ifdef DEFINED_WINDOW_ONLY_EVENT
#error "Don't define DEFINED_WINDOW_ONLY_EVENT"
#endif /* DEFINED_WINDOW_ONLY_EVENT */

#ifndef WINDOW_ONLY_EVENT
#define WINDOW_ONLY_EVENT(_name, _id, _type, _struct)
#define DEFINED_WINDOW_ONLY_EVENT
#endif /* WINDOW_ONLY_EVENT */

#ifdef DEFINED_WINDOW_EVENT
#error "Don't define DEFINED_WINDOW_EVENT"
#endif /* DEFINED_WINDOW_EVENT */

#ifndef WINDOW_EVENT
#define WINDOW_EVENT WINDOW_ONLY_EVENT
#define DEFINED_WINDOW_EVENT
#endif /* WINDOW_EVENT */

#ifdef DEFINED_TOUCH_EVENT
#error "Don't define DEFINED_TOUCH_EVENT"
#endif /* DEFINED_TOUCH_EVENT */

#ifndef TOUCH_EVENT
#define TOUCH_EVENT(_name, _id, _type, _struct)
#define DEFINED_TOUCH_EVENT
#endif /* TOUCH_EVENT */

#ifdef DEFINED_DOCUMENT_ONLY_EVENT
#error "Don't define DEFINED_DOCUMENT_ONLY_EVENT"
#endif /* DEFINED_DOCUMENT_ONLY_EVENT */

#ifndef DOCUMENT_ONLY_EVENT
#define DOCUMENT_ONLY_EVENT(_name, _id, _type, _struct)
#define DEFINED_DOCUMENT_ONLY_EVENT
#endif /* DOCUMENT_ONLY_EVENT */

#ifdef DEFINED_NON_IDL_EVENT
#error "Don't define DEFINED_NON_IDL_EVENT"
#endif /* DEFINED_NON_IDL_EVENT */

#ifndef NON_IDL_EVENT
#define NON_IDL_EVENT(_name, _id, _type, _struct)
#define DEFINED_NON_IDL_EVENT
#endif /* NON_IDL_EVENT */

#ifdef DEFINED_ERROR_EVENT
#error "Don't define DEFINED_ERROR_EVENT"
#endif /* DEFINED_ERROR_EVENT */

#ifndef ERROR_EVENT
#define ERROR_EVENT FORWARDED_EVENT
#define DEFINED_ERROR_EVENT
#endif /* ERROR_EVENT */

#ifdef DEFINED_BEFOREUNLOAD_EVENT
#error "Don't define DEFINED_BEFOREUNLOAD_EVENT"
#endif /* DEFINED_BEFOREUNLOAD_EVENT */

#ifndef BEFOREUNLOAD_EVENT
#define BEFOREUNLOAD_EVENT WINDOW_EVENT
#define DEFINED_BEFOREUNLOAD_EVENT
#endif /* BEFOREUNLOAD_EVENT */

EVENT(abort,
      NS_IMAGE_ABORT,
      EventNameType_All,
      eBasicEventClass)
EVENT(canplay,
      NS_CANPLAY,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(canplaythrough,
      NS_CANPLAYTHROUGH,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(change,
      NS_FORM_CHANGE,
      EventNameType_HTMLXUL,
      eBasicEventClass)
EVENT(click,
      NS_MOUSE_CLICK,
      EventNameType_All,
      eMouseEventClass)
EVENT(contextmenu,
      NS_CONTEXTMENU,
      EventNameType_HTMLXUL,
      eMouseEventClass)
// Not supported yet
// EVENT(cuechange)
EVENT(dblclick,
      NS_MOUSE_DOUBLECLICK,
      EventNameType_HTMLXUL,
      eMouseEventClass)
EVENT(drag,
      NS_DRAGDROP_DRAG,
      EventNameType_HTMLXUL,
      eDragEventClass)
EVENT(dragend,
      NS_DRAGDROP_END,
      EventNameType_HTMLXUL,
      eDragEventClass)
EVENT(dragenter,
      NS_DRAGDROP_ENTER,
      EventNameType_HTMLXUL,
      eDragEventClass)
EVENT(dragleave,
      NS_DRAGDROP_LEAVE,
      EventNameType_HTMLXUL,
      eDragEventClass)
EVENT(dragover,
      NS_DRAGDROP_OVER,
      EventNameType_HTMLXUL,
      eDragEventClass)
EVENT(dragstart,
      NS_DRAGDROP_START,
      EventNameType_HTMLXUL,
      eDragEventClass)
EVENT(drop,
      NS_DRAGDROP_DROP,
      EventNameType_HTMLXUL,
      eDragEventClass)
EVENT(durationchange,
      NS_DURATIONCHANGE,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(emptied,
      NS_EMPTIED,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(ended,
      NS_ENDED,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(input,
      NS_EDITOR_INPUT,
      EventNameType_HTMLXUL,
      eEditorInputEventClass)
EVENT(invalid,
      NS_FORM_INVALID,
      EventNameType_HTMLXUL,
      eBasicEventClass)
EVENT(keydown,
      NS_KEY_DOWN,
      EventNameType_HTMLXUL,
      eKeyboardEventClass)
EVENT(keypress,
      NS_KEY_PRESS,
      EventNameType_HTMLXUL,
      eKeyboardEventClass)
EVENT(keyup,
      NS_KEY_UP,
      EventNameType_HTMLXUL,
      eKeyboardEventClass)
NON_IDL_EVENT(mozbrowserbeforekeydown,
              NS_KEY_BEFORE_DOWN,
              EventNameType_None,
              eBeforeAfterKeyboardEventClass)
NON_IDL_EVENT(mozbrowserafterkeydown,
              NS_KEY_AFTER_DOWN,
              EventNameType_None,
              eBeforeAfterKeyboardEventClass)
NON_IDL_EVENT(mozbrowserbeforekeyup,
              NS_KEY_BEFORE_UP,
              EventNameType_None,
              eBeforeAfterKeyboardEventClass)
NON_IDL_EVENT(mozbrowserafterkeyup,
              NS_KEY_AFTER_UP,
              EventNameType_None,
              eBeforeAfterKeyboardEventClass)
EVENT(loadeddata,
      NS_LOADEDDATA,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(loadedmetadata,
      NS_LOADEDMETADATA,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(loadstart,
      NS_LOADSTART,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(mousedown,
      NS_MOUSE_BUTTON_DOWN,
      EventNameType_All,
      eMouseEventClass)
EVENT(mouseenter,
      NS_MOUSEENTER,
      EventNameType_All,
      eMouseEventClass)
EVENT(mouseleave,
      NS_MOUSELEAVE,
      EventNameType_All,
      eMouseEventClass)
EVENT(mousemove,
      NS_MOUSE_MOVE,
      EventNameType_All,
      eMouseEventClass)
EVENT(mouseout,
      NS_MOUSE_OUT,
      EventNameType_All,
      eMouseEventClass)
EVENT(mouseover,
      NS_MOUSE_OVER,
      EventNameType_All,
      eMouseEventClass)
EVENT(mouseup,
      NS_MOUSE_BUTTON_UP,
      EventNameType_All,
      eMouseEventClass)
EVENT(mozfullscreenchange,
      NS_FULLSCREENCHANGE,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(mozfullscreenerror,
      NS_FULLSCREENERROR,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(mozpointerlockchange,
      NS_POINTERLOCKCHANGE,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(mozpointerlockerror,
      NS_POINTERLOCKERROR,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(pointerdown,
      NS_POINTER_DOWN,
      EventNameType_All,
      ePointerEventClass)
EVENT(pointermove,
      NS_POINTER_MOVE,
      EventNameType_All,
      ePointerEventClass)
EVENT(pointerup,
      NS_POINTER_UP,
      EventNameType_All,
      ePointerEventClass)
EVENT(pointercancel,
      NS_POINTER_CANCEL,
      EventNameType_All,
      ePointerEventClass)
EVENT(pointerover,
      NS_POINTER_OVER,
      EventNameType_All,
      ePointerEventClass)
EVENT(pointerout,
      NS_POINTER_OUT,
      EventNameType_All,
      ePointerEventClass)
EVENT(pointerenter,
      NS_POINTER_ENTER,
      EventNameType_All,
      ePointerEventClass)
EVENT(pointerleave,
      NS_POINTER_LEAVE,
      EventNameType_All,
      ePointerEventClass)
EVENT(gotpointercapture,
      NS_POINTER_GOT_CAPTURE,
      EventNameType_All,
      ePointerEventClass)
EVENT(lostpointercapture,
      NS_POINTER_LOST_CAPTURE,
      EventNameType_All,
      ePointerEventClass)

// Not supported yet; probably never because "wheel" is a better idea.
// EVENT(mousewheel)
EVENT(pause,
      NS_PAUSE,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(play,
      NS_PLAY,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(playing,
      NS_PLAYING,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(progress,
      NS_PROGRESS,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(ratechange,
      NS_RATECHANGE,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(reset,
      NS_FORM_RESET,
      EventNameType_HTMLXUL,
      eBasicEventClass)
EVENT(seeked,
      NS_SEEKED,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(seeking,
      NS_SEEKING,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(select,
      NS_FORM_SELECTED,
      EventNameType_HTMLXUL,
      eBasicEventClass)
EVENT(show,
      NS_SHOW_EVENT,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(stalled,
      NS_STALLED,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(submit,
      NS_FORM_SUBMIT,
      EventNameType_HTMLXUL,
      eBasicEventClass)
EVENT(suspend,
      NS_SUSPEND,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(timeupdate,
      NS_TIMEUPDATE,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(volumechange,
      NS_VOLUMECHANGE,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(waiting,
      NS_WAITING,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(wheel,
      NS_WHEEL_WHEEL,
      EventNameType_All,
      eWheelEventClass)
EVENT(copy,
      NS_COPY,
      EventNameType_HTMLXUL,
      eClipboardEventClass)
EVENT(cut,
      NS_CUT,
      EventNameType_HTMLXUL,
      eClipboardEventClass)
EVENT(paste,
      NS_PASTE,
      EventNameType_HTMLXUL,
      eClipboardEventClass)
// Gecko-specific extensions that apply to elements
EVENT(beforescriptexecute,
      NS_BEFORE_SCRIPT_EXECUTE,
      EventNameType_HTMLXUL,
      eBasicEventClass)
EVENT(afterscriptexecute,
      NS_AFTER_SCRIPT_EXECUTE,
      EventNameType_HTMLXUL,
      eBasicEventClass)

FORWARDED_EVENT(blur,
                NS_BLUR_CONTENT,
                EventNameType_HTMLXUL,
                eFocusEventClass)
ERROR_EVENT(error,
            NS_LOAD_ERROR,
            EventNameType_All,
            eBasicEventClass)
FORWARDED_EVENT(focus,
                NS_FOCUS_CONTENT,
                EventNameType_HTMLXUL,
                eFocusEventClass)
FORWARDED_EVENT(load,
                NS_LOAD,
                EventNameType_All,
                eBasicEventClass)
FORWARDED_EVENT(resize,
                NS_RESIZE_EVENT,
                EventNameType_All,
                eBasicEventClass)
FORWARDED_EVENT(scroll,
                NS_SCROLL_EVENT,
                (EventNameType_HTMLXUL | EventNameType_SVGSVG),
                eBasicEventClass)

WINDOW_EVENT(afterprint,
             NS_AFTERPRINT,
             EventNameType_XUL | EventNameType_HTMLBodyOrFramesetOnly,
             eBasicEventClass)
WINDOW_EVENT(beforeprint,
             NS_BEFOREPRINT,
             EventNameType_XUL | EventNameType_HTMLBodyOrFramesetOnly,
             eBasicEventClass)
BEFOREUNLOAD_EVENT(beforeunload,
                   NS_BEFORE_PAGE_UNLOAD,
                   EventNameType_XUL | EventNameType_HTMLBodyOrFramesetOnly,
                   eBasicEventClass)
WINDOW_EVENT(hashchange,
             NS_HASHCHANGE,
             EventNameType_XUL | EventNameType_HTMLBodyOrFramesetOnly,
             eBasicEventClass)
WINDOW_EVENT(languagechange,
             NS_LANGUAGECHANGE,
             EventNameType_HTMLBodyOrFramesetOnly,
             eBasicEventClass)
// XXXbz Should the onmessage attribute on <body> really not work?  If so, do we
// need a different macro to flag things like that (IDL, but not content
// attributes on body/frameset), or is just using EventNameType_None enough?
WINDOW_EVENT(message,
             NS_MESSAGE,
             EventNameType_None,
             eBasicEventClass)
WINDOW_EVENT(offline,
             NS_OFFLINE,
             EventNameType_XUL | EventNameType_HTMLBodyOrFramesetOnly,
             eBasicEventClass)
WINDOW_EVENT(online,
             NS_ONLINE,
             EventNameType_XUL | EventNameType_HTMLBodyOrFramesetOnly,
             eBasicEventClass)
WINDOW_EVENT(pagehide,
             NS_PAGE_HIDE,
             EventNameType_HTMLBodyOrFramesetOnly,
             eBasicEventClass)
WINDOW_EVENT(pageshow,
             NS_PAGE_SHOW,
             EventNameType_HTMLBodyOrFramesetOnly,
             eBasicEventClass)
WINDOW_EVENT(popstate,
             NS_POPSTATE,
             EventNameType_XUL | EventNameType_HTMLBodyOrFramesetOnly,
             eBasicEventClass)
// Not supported yet
// WINDOW_EVENT(redo)
// Not supported yet
// WINDOW_EVENT(storage)
// Not supported yet
// WINDOW_EVENT(undo)
WINDOW_EVENT(unload,
             NS_PAGE_UNLOAD,
             (EventNameType_XUL | EventNameType_SVGSVG |
              EventNameType_HTMLBodyOrFramesetOnly),
             eBasicEventClass)

WINDOW_ONLY_EVENT(devicemotion,
                  NS_DEVICE_MOTION,
                  EventNameType_None,
                  eBasicEventClass)
WINDOW_ONLY_EVENT(deviceorientation,
                  NS_DEVICE_ORIENTATION,
                  EventNameType_None,
                  eBasicEventClass)
WINDOW_ONLY_EVENT(deviceproximity,
                  NS_DEVICE_PROXIMITY,
                  EventNameType_None,
                  eBasicEventClass)
WINDOW_ONLY_EVENT(userproximity,
                  NS_USER_PROXIMITY,
                  EventNameType_None,
                  eBasicEventClass)
WINDOW_ONLY_EVENT(devicelight,
                  NS_DEVICE_LIGHT,
                  EventNameType_None,
                  eBasicEventClass)

#ifdef MOZ_B2G
WINDOW_ONLY_EVENT(moztimechange,
                  NS_MOZ_TIME_CHANGE_EVENT,
                  EventNameType_None,
                  eBasicEventClass)
WINDOW_ONLY_EVENT(moznetworkupload,
                  NS_NETWORK_UPLOAD_EVENT,
                  EventNameType_None,
                  eBasicEventClass)
WINDOW_ONLY_EVENT(moznetworkdownload,
                  NS_NETWORK_DOWNLOAD_EVENT,
                  EventNameType_None,
                  eBasicEventClass)
#endif // MOZ_B2G

TOUCH_EVENT(touchstart,
            NS_TOUCH_START,
            EventNameType_All,
            eTouchEventClass)
TOUCH_EVENT(touchend,
            NS_TOUCH_END,
            EventNameType_All,
            eTouchEventClass)
TOUCH_EVENT(touchmove,
            NS_TOUCH_MOVE,
            EventNameType_All,
            eTouchEventClass )
TOUCH_EVENT(touchcancel,
            NS_TOUCH_CANCEL,
            EventNameType_All,
            eTouchEventClass)

DOCUMENT_ONLY_EVENT(readystatechange,
                    NS_READYSTATECHANGE,
                    EventNameType_HTMLXUL,
                    eBasicEventClass)

NON_IDL_EVENT(MozMouseHittest,
              NS_MOUSE_MOZHITTEST,
              EventNameType_None,
              eMouseEventClass)

NON_IDL_EVENT(DOMAttrModified,
              NS_MUTATION_ATTRMODIFIED,
              EventNameType_HTMLXUL,
              eMutationEventClass)
NON_IDL_EVENT(DOMCharacterDataModified,
              NS_MUTATION_CHARACTERDATAMODIFIED,
              EventNameType_HTMLXUL,
              eMutationEventClass)
NON_IDL_EVENT(DOMNodeInserted,
              NS_MUTATION_NODEINSERTED,
              EventNameType_HTMLXUL,
              eMutationEventClass)
NON_IDL_EVENT(DOMNodeRemoved,
              NS_MUTATION_NODEREMOVED,
              EventNameType_HTMLXUL,
              eMutationEventClass)
NON_IDL_EVENT(DOMNodeInsertedIntoDocument,
              NS_MUTATION_NODEINSERTEDINTODOCUMENT,
              EventNameType_HTMLXUL,
              eMutationEventClass)
NON_IDL_EVENT(DOMNodeRemovedFromDocument,
              NS_MUTATION_NODEREMOVEDFROMDOCUMENT,
              EventNameType_HTMLXUL,
              eMutationEventClass)
NON_IDL_EVENT(DOMSubtreeModified,
              NS_MUTATION_SUBTREEMODIFIED,
              EventNameType_HTMLXUL,
              eMutationEventClass)

NON_IDL_EVENT(DOMActivate,
              NS_UI_ACTIVATE,
              EventNameType_HTMLXUL,
              eUIEventClass)
NON_IDL_EVENT(DOMFocusIn,
              NS_UI_FOCUSIN,
              EventNameType_HTMLXUL,
              eUIEventClass)
NON_IDL_EVENT(DOMFocusOut,
              NS_UI_FOCUSOUT,
              EventNameType_HTMLXUL,
              eUIEventClass)
                                  
NON_IDL_EVENT(DOMMouseScroll,
              NS_MOUSE_SCROLL,
              EventNameType_HTMLXUL,
              eMouseScrollEventClass)
NON_IDL_EVENT(MozMousePixelScroll,
              NS_MOUSE_PIXEL_SCROLL,
              EventNameType_HTMLXUL,
              eMouseScrollEventClass)
                                                
NON_IDL_EVENT(open,
              NS_OPEN,
              EventNameType_None,
              eBasicEventClass)

NON_IDL_EVENT(dataavailable,
              NS_MEDIARECORDER_DATAAVAILABLE,
              EventNameType_None,
              eBasicEventClass)

NON_IDL_EVENT(stop,
              NS_MEDIARECORDER_STOP,
              EventNameType_None,
              eBasicEventClass)

NON_IDL_EVENT(warning,
              NS_MEDIARECORDER_WARNING,
              EventNameType_None,
              eBasicEventClass)

NON_IDL_EVENT(speakerforcedchange,
              NS_SPEAKERMANAGER_SPEAKERFORCEDCHANGE,
              EventNameType_None,
              eBasicEventClass)

// Events that only have on* attributes on XUL elements

 // "text" event is legacy event for modifying composition string in nsEditor.
 // This shouldn't be used by web/xul apps.  "compositionupdate" should be
 // used instead.
NON_IDL_EVENT(text,
              NS_COMPOSITION_CHANGE,
              EventNameType_XUL,
              eCompositionEventClass)
NON_IDL_EVENT(compositionstart,
              NS_COMPOSITION_START,
              EventNameType_XUL,
              eCompositionEventClass)
NON_IDL_EVENT(compositionupdate,
              NS_COMPOSITION_UPDATE,
              EventNameType_XUL,
              eCompositionEventClass)
NON_IDL_EVENT(compositionend,
              NS_COMPOSITION_END,
              EventNameType_XUL,
              eCompositionEventClass)
NON_IDL_EVENT(command,
              NS_XUL_COMMAND,
              EventNameType_XUL,
              eInputEventClass)
NON_IDL_EVENT(close,
              NS_XUL_CLOSE,
              EventNameType_XUL,
              eBasicEventClass)
NON_IDL_EVENT(popupshowing,
              NS_XUL_POPUP_SHOWING,
              EventNameType_XUL,
              eBasicEventClass)
NON_IDL_EVENT(popupshown,
              NS_XUL_POPUP_SHOWN,
              EventNameType_XUL,
              eBasicEventClass)
NON_IDL_EVENT(popuphiding,
              NS_XUL_POPUP_HIDING,
              EventNameType_XUL,
              eBasicEventClass)
NON_IDL_EVENT(popuphidden,
              NS_XUL_POPUP_HIDDEN,
              EventNameType_XUL,
              eBasicEventClass)
NON_IDL_EVENT(broadcast,
              NS_XUL_BROADCAST,
              EventNameType_XUL,
              eBasicEventClass)
NON_IDL_EVENT(commandupdate,
              NS_XUL_COMMAND_UPDATE,
              EventNameType_XUL,
              eBasicEventClass)
NON_IDL_EVENT(dragexit,
              NS_DRAGDROP_EXIT,
              EventNameType_XUL,
              eDragEventClass)
NON_IDL_EVENT(dragdrop,
              NS_DRAGDROP_DRAGDROP,
              EventNameType_XUL,
              eDragEventClass)
NON_IDL_EVENT(draggesture,
              NS_DRAGDROP_GESTURE,
              EventNameType_XUL,
              eDragEventClass)
NON_IDL_EVENT(overflow,
              NS_SCROLLPORT_OVERFLOW,
              EventNameType_XUL,
              eBasicEventClass)
NON_IDL_EVENT(underflow,
              NS_SCROLLPORT_UNDERFLOW,
              EventNameType_XUL,
              eBasicEventClass)

// Various SVG events
NON_IDL_EVENT(SVGLoad,
              NS_SVG_LOAD,
              EventNameType_None,
              eBasicEventClass)
NON_IDL_EVENT(SVGUnload,
              NS_SVG_UNLOAD,
              EventNameType_None,
              eBasicEventClass)
NON_IDL_EVENT(SVGResize,
              NS_SVG_RESIZE,
              EventNameType_None,
              eBasicEventClass)
NON_IDL_EVENT(SVGScroll,
              NS_SVG_SCROLL,
              EventNameType_None,
              eBasicEventClass)

NON_IDL_EVENT(SVGZoom,
              NS_SVG_ZOOM,
              EventNameType_None,
              eSVGZoomEventClass)

// Only map the ID to the real event name when ID_TO_EVENT is defined.
#ifndef ID_TO_EVENT
// This is a bit hackish, but SVG's event names are weird.
NON_IDL_EVENT(zoom,
              NS_SVG_ZOOM,
              EventNameType_SVGSVG,
              eBasicEventClass)
#endif
// Only map the ID to the real event name when ID_TO_EVENT is defined.
#ifndef ID_TO_EVENT
NON_IDL_EVENT(begin,
              NS_SMIL_BEGIN,
              EventNameType_SMIL,
              eBasicEventClass)
#endif
NON_IDL_EVENT(beginEvent,
              NS_SMIL_BEGIN,
              EventNameType_None,
              eSMILTimeEventClass)
// Only map the ID to the real event name when ID_TO_EVENT is defined.
#ifndef ID_TO_EVENT
NON_IDL_EVENT(end,
              NS_SMIL_END,
              EventNameType_SMIL,
              eBasicEventClass)
#endif
NON_IDL_EVENT(endEvent,
              NS_SMIL_END,
              EventNameType_None,
              eSMILTimeEventClass)
// Only map the ID to the real event name when ID_TO_EVENT is defined.
#ifndef ID_TO_EVENT
NON_IDL_EVENT(repeat,
              NS_SMIL_REPEAT,
              EventNameType_SMIL,
              eBasicEventClass)
#endif
NON_IDL_EVENT(repeatEvent,
              NS_SMIL_REPEAT,
              EventNameType_None,
              eSMILTimeEventClass)

NON_IDL_EVENT(MozAfterPaint,
              NS_AFTERPAINT,
              EventNameType_None,
              eBasicEventClass)

NON_IDL_EVENT(MozScrolledAreaChanged,
              NS_SCROLLEDAREACHANGED,
              EventNameType_None,
              eScrollAreaEventClass)

#ifdef MOZ_GAMEPAD
NON_IDL_EVENT(gamepadbuttondown,
              NS_GAMEPAD_BUTTONDOWN,
              EventNameType_None,
              eBasicEventClass)
NON_IDL_EVENT(gamepadbuttonup,
              NS_GAMEPAD_BUTTONUP,
              EventNameType_None,
              eBasicEventClass)
NON_IDL_EVENT(gamepadaxismove,
              NS_GAMEPAD_AXISMOVE,
              EventNameType_None,
              eBasicEventClass)
NON_IDL_EVENT(gamepadconnected,
              NS_GAMEPAD_CONNECTED,
              EventNameType_None,
              eBasicEventClass)
NON_IDL_EVENT(gamepaddisconnected,
              NS_GAMEPAD_DISCONNECTED,
              EventNameType_None,
              eBasicEventClass)
#endif

// Simple gesture events
NON_IDL_EVENT(MozSwipeGestureStart,
              NS_SIMPLE_GESTURE_SWIPE_START,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozSwipeGestureUpdate,
              NS_SIMPLE_GESTURE_SWIPE_UPDATE,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozSwipeGestureEnd,
              NS_SIMPLE_GESTURE_SWIPE_END,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozSwipeGesture,
              NS_SIMPLE_GESTURE_SWIPE,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozMagnifyGestureStart,
              NS_SIMPLE_GESTURE_MAGNIFY_START,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozMagnifyGestureUpdate,
              NS_SIMPLE_GESTURE_MAGNIFY_UPDATE,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozMagnifyGesture,
              NS_SIMPLE_GESTURE_MAGNIFY,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozRotateGestureStart,
              NS_SIMPLE_GESTURE_ROTATE_START,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozRotateGestureUpdate,
              NS_SIMPLE_GESTURE_ROTATE_UPDATE,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozRotateGesture,
              NS_SIMPLE_GESTURE_ROTATE,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozTapGesture,
              NS_SIMPLE_GESTURE_TAP,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozPressTapGesture,
              NS_SIMPLE_GESTURE_PRESSTAP,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozEdgeUIStarted,
              NS_SIMPLE_GESTURE_EDGE_STARTED,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozEdgeUICanceled,
              NS_SIMPLE_GESTURE_EDGE_CANCELED,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozEdgeUICompleted,
              NS_SIMPLE_GESTURE_EDGE_COMPLETED,
              EventNameType_None,
              eSimpleGestureEventClass)

NON_IDL_EVENT(transitionend,
              NS_TRANSITION_END,
              EventNameType_None,
              eTransitionEventClass)
NON_IDL_EVENT(animationstart,
              NS_ANIMATION_START,
              EventNameType_None,
              eAnimationEventClass)
NON_IDL_EVENT(animationend,
              NS_ANIMATION_END,
              EventNameType_None,
              eAnimationEventClass)
NON_IDL_EVENT(animationiteration,
              NS_ANIMATION_ITERATION,
              EventNameType_None,
              eAnimationEventClass)

NON_IDL_EVENT(audioprocess,
              NS_AUDIO_PROCESS,
              EventNameType_None,
              eBasicEventClass)

NON_IDL_EVENT(complete,
              NS_AUDIO_COMPLETE,
              EventNameType_None,
              eBasicEventClass)

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

#ifdef DEFINED_DOCUMENT_ONLY_EVENT
#undef DEFINED_DOCUMENT_ONLY_EVENT
#undef DOCUMENT_ONLY_EVENT
#endif /* DEFINED_DOCUMENT_ONLY_EVENT */

#ifdef DEFINED_NON_IDL_EVENT
#undef DEFINED_NON_IDL_EVENT
#undef NON_IDL_EVENT
#endif /* DEFINED_NON_IDL_EVENT */

#ifdef DEFINED_ERROR_EVENT
#undef DEFINED_ERROR_EVENT
#undef ERROR_EVENT
#endif /* DEFINED_ERROR_EVENT */

#ifdef DEFINED_BEFOREUNLOAD_EVENT
#undef DEFINED_BEFOREUNLOAD_EVENT
#undef BEFOREUNLOAD_EVENT
#endif /* BEFOREUNLOAD_EVENT */

#ifdef ID_TO_EVENT
#undef EVENT
#undef WINDOW_ONLY_EVENT
#undef TOUCH_EVENT
#undef DOCUMENT_ONLY_EVENT
#undef NON_IDL_EVENT
#endif /* ID_TO_EVENT */

