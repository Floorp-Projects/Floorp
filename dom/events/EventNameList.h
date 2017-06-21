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
 * 2) The event message
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

#ifdef MESSAGE_TO_EVENT
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

#define EVENT MESSAGE_TO_EVENT
#define WINDOW_ONLY_EVENT MESSAGE_TO_EVENT
#define TOUCH_EVENT MESSAGE_TO_EVENT
#define DOCUMENT_ONLY_EVENT MESSAGE_TO_EVENT
#define NON_IDL_EVENT MESSAGE_TO_EVENT
#endif /* MESSAGE_TO_EVENT */

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
#define WINDOW_ONLY_EVENT(_name, _message, _type, _struct)
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
#define TOUCH_EVENT(_name, _message, _type, _struct)
#define DEFINED_TOUCH_EVENT
#endif /* TOUCH_EVENT */

#ifdef DEFINED_DOCUMENT_ONLY_EVENT
#error "Don't define DEFINED_DOCUMENT_ONLY_EVENT"
#endif /* DEFINED_DOCUMENT_ONLY_EVENT */

#ifndef DOCUMENT_ONLY_EVENT
#define DOCUMENT_ONLY_EVENT(_name, _message, _type, _struct)
#define DEFINED_DOCUMENT_ONLY_EVENT
#endif /* DOCUMENT_ONLY_EVENT */

#ifdef DEFINED_NON_IDL_EVENT
#error "Don't define DEFINED_NON_IDL_EVENT"
#endif /* DEFINED_NON_IDL_EVENT */

#ifndef NON_IDL_EVENT
#define NON_IDL_EVENT(_name, _message, _type, _struct)
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
      eImageAbort,
      EventNameType_All,
      eBasicEventClass)
EVENT(canplay,
      eCanPlay,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(canplaythrough,
      eCanPlayThrough,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(change,
      eFormChange,
      EventNameType_HTMLXUL,
      eBasicEventClass)
EVENT(CheckboxStateChange,
      eFormCheckboxStateChange,
      EventNameType_None,
      eBasicEventClass)
EVENT(RadioStateChange,
      eFormRadioStateChange,
      EventNameType_None,
      eBasicEventClass)
EVENT(auxclick,
      eMouseAuxClick,
      EventNameType_All,
      eMouseEventClass)
EVENT(click,
      eMouseClick,
      EventNameType_All,
      eMouseEventClass)
EVENT(close,
      eClose,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(contextmenu,
      eContextMenu,
      EventNameType_HTMLXUL,
      eMouseEventClass)
NON_IDL_EVENT(mouselongtap,
      eMouseLongTap,
      EventNameType_HTMLXUL,
      eMouseEventClass)
// Not supported yet
// EVENT(cuechange)
EVENT(dblclick,
      eMouseDoubleClick,
      EventNameType_HTMLXUL,
      eMouseEventClass)
EVENT(drag,
      eDrag,
      EventNameType_HTMLXUL,
      eDragEventClass)
EVENT(dragend,
      eDragEnd,
      EventNameType_HTMLXUL,
      eDragEventClass)
EVENT(dragenter,
      eDragEnter,
      EventNameType_HTMLXUL,
      eDragEventClass)
EVENT(dragexit,
      eDragExit,
      EventNameType_HTMLXUL,
      eDragEventClass)
EVENT(dragleave,
      eDragLeave,
      EventNameType_HTMLXUL,
      eDragEventClass)
EVENT(dragover,
      eDragOver,
      EventNameType_HTMLXUL,
      eDragEventClass)
EVENT(dragstart,
      eDragStart,
      EventNameType_HTMLXUL,
      eDragEventClass)
EVENT(drop,
      eDrop,
      EventNameType_HTMLXUL,
      eDragEventClass)
EVENT(durationchange,
      eDurationChange,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(emptied,
      eEmptied,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(ended,
      eEnded,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(fullscreenchange,
      eFullscreenChange,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(fullscreenerror,
      eFullscreenError,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(input,
      eEditorInput,
      EventNameType_HTMLXUL,
      eEditorInputEventClass)
EVENT(invalid,
      eFormInvalid,
      EventNameType_HTMLXUL,
      eBasicEventClass)
EVENT(keydown,
      eKeyDown,
      EventNameType_HTMLXUL,
      eKeyboardEventClass)
EVENT(keypress,
      eKeyPress,
      EventNameType_HTMLXUL,
      eKeyboardEventClass)
EVENT(keyup,
      eKeyUp,
      EventNameType_HTMLXUL,
      eKeyboardEventClass)
EVENT(mozkeydownonplugin,
      eKeyDownOnPlugin,
      EventNameType_None,
      eKeyboardEventClass)
EVENT(mozkeyuponplugin,
      eKeyUpOnPlugin,
      EventNameType_None,
      eKeyboardEventClass)
NON_IDL_EVENT(mozaccesskeynotfound,
              eAccessKeyNotFound,
              EventNameType_None,
              eKeyboardEventClass)
EVENT(loadeddata,
      eLoadedData,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(loadedmetadata,
      eLoadedMetaData,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(loadend,
      eLoadEnd,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(loadstart,
      eLoadStart,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(mousedown,
      eMouseDown,
      EventNameType_All,
      eMouseEventClass)
EVENT(mouseenter,
      eMouseEnter,
      EventNameType_All,
      eMouseEventClass)
EVENT(mouseleave,
      eMouseLeave,
      EventNameType_All,
      eMouseEventClass)
EVENT(mousemove,
      eMouseMove,
      EventNameType_All,
      eMouseEventClass)
EVENT(mouseout,
      eMouseOut,
      EventNameType_All,
      eMouseEventClass)
EVENT(mouseover,
      eMouseOver,
      EventNameType_All,
      eMouseEventClass)
EVENT(mouseup,
      eMouseUp,
      EventNameType_All,
      eMouseEventClass)
EVENT(mozfullscreenchange,
      eMozFullscreenChange,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(mozfullscreenerror,
      eMozFullscreenError,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(mozpointerlockchange,
      eMozPointerLockChange,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(mozpointerlockerror,
      eMozPointerLockError,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(pointerlockchange,
      ePointerLockChange,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(pointerlockerror,
      ePointerLockError,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(pointerdown,
      ePointerDown,
      EventNameType_All,
      ePointerEventClass)
EVENT(pointermove,
      ePointerMove,
      EventNameType_All,
      ePointerEventClass)
EVENT(pointerup,
      ePointerUp,
      EventNameType_All,
      ePointerEventClass)
EVENT(pointercancel,
      ePointerCancel,
      EventNameType_All,
      ePointerEventClass)
EVENT(pointerover,
      ePointerOver,
      EventNameType_All,
      ePointerEventClass)
EVENT(pointerout,
      ePointerOut,
      EventNameType_All,
      ePointerEventClass)
EVENT(pointerenter,
      ePointerEnter,
      EventNameType_All,
      ePointerEventClass)
EVENT(pointerleave,
      ePointerLeave,
      EventNameType_All,
      ePointerEventClass)
EVENT(gotpointercapture,
      ePointerGotCapture,
      EventNameType_All,
      ePointerEventClass)
EVENT(lostpointercapture,
      ePointerLostCapture,
      EventNameType_All,
      ePointerEventClass)
EVENT(selectstart,
      eSelectStart,
      EventNameType_HTMLXUL,
      eBasicEventClass)

// Not supported yet; probably never because "wheel" is a better idea.
// EVENT(mousewheel)
EVENT(pause,
      ePause,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(play,
      ePlay,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(playing,
      ePlaying,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(progress,
      eProgress,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(ratechange,
      eRateChange,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(reset,
      eFormReset,
      EventNameType_HTMLXUL,
      eBasicEventClass)
EVENT(seeked,
      eSeeked,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(seeking,
      eSeeking,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(select,
      eFormSelect,
      EventNameType_HTMLXUL,
      eBasicEventClass)
EVENT(show,
      eShow,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(stalled,
      eStalled,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(submit,
      eFormSubmit,
      EventNameType_HTMLXUL,
      eBasicEventClass)
EVENT(suspend,
      eSuspend,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(timeupdate,
      eTimeUpdate,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(toggle,
      eToggle,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(volumechange,
      eVolumeChange,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(waiting,
      eWaiting,
      EventNameType_HTML,
      eBasicEventClass)
EVENT(wheel,
      eWheel,
      EventNameType_All,
      eWheelEventClass)
EVENT(copy,
      eCopy,
      EventNameType_HTMLXUL,
      eClipboardEventClass)
EVENT(cut,
      eCut,
      EventNameType_HTMLXUL,
      eClipboardEventClass)
EVENT(paste,
      ePaste,
      EventNameType_HTMLXUL,
      eClipboardEventClass)
// Gecko-specific extensions that apply to elements
EVENT(beforescriptexecute,
      eBeforeScriptExecute,
      EventNameType_HTMLXUL,
      eBasicEventClass)
EVENT(afterscriptexecute,
      eAfterScriptExecute,
      EventNameType_HTMLXUL,
      eBasicEventClass)

FORWARDED_EVENT(blur,
                eBlur,
                EventNameType_HTMLXUL,
                eFocusEventClass)
ERROR_EVENT(error,
            eLoadError,
            EventNameType_All,
            eBasicEventClass)
FORWARDED_EVENT(focus,
                eFocus,
                EventNameType_HTMLXUL,
                eFocusEventClass)
FORWARDED_EVENT(focusin,
                eFocusIn,
                EventNameType_HTMLXUL,
                eFocusEventClass)
FORWARDED_EVENT(focusout,
                eFocusOut,
                EventNameType_HTMLXUL,
                eFocusEventClass)
FORWARDED_EVENT(load,
                eLoad,
                EventNameType_All,
                eBasicEventClass)
FORWARDED_EVENT(resize,
                eResize,
                EventNameType_All,
                eBasicEventClass)
FORWARDED_EVENT(scroll,
                eScroll,
                (EventNameType_HTMLXUL | EventNameType_SVGSVG),
                eBasicEventClass)

WINDOW_EVENT(afterprint,
             eAfterPrint,
             EventNameType_XUL | EventNameType_HTMLBodyOrFramesetOnly,
             eBasicEventClass)
WINDOW_EVENT(beforeprint,
             eBeforePrint,
             EventNameType_XUL | EventNameType_HTMLBodyOrFramesetOnly,
             eBasicEventClass)
BEFOREUNLOAD_EVENT(beforeunload,
                   eBeforeUnload,
                   EventNameType_XUL | EventNameType_HTMLBodyOrFramesetOnly,
                   eBasicEventClass)
WINDOW_EVENT(hashchange,
             eHashChange,
             EventNameType_XUL | EventNameType_HTMLBodyOrFramesetOnly,
             eBasicEventClass)
WINDOW_EVENT(languagechange,
             eLanguageChange,
             EventNameType_HTMLBodyOrFramesetOnly,
             eBasicEventClass)
// XXXbz Should the onmessage attribute on <body> really not work?  If so, do we
// need a different macro to flag things like that (IDL, but not content
// attributes on body/frameset), or is just using EventNameType_None enough?
WINDOW_EVENT(message,
             eMessage,
             EventNameType_None,
             eBasicEventClass)
WINDOW_EVENT(offline,
             eOffline,
             EventNameType_XUL | EventNameType_HTMLBodyOrFramesetOnly,
             eBasicEventClass)
WINDOW_EVENT(online,
             eOnline,
             EventNameType_XUL | EventNameType_HTMLBodyOrFramesetOnly,
             eBasicEventClass)
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
WINDOW_EVENT(orientationchange,
             eOrientationChange,
             EventNameType_HTMLBodyOrFramesetOnly,
             eBasicEventClass)
#endif
WINDOW_EVENT(pagehide,
             ePageHide,
             EventNameType_HTMLBodyOrFramesetOnly,
             eBasicEventClass)
WINDOW_EVENT(pageshow,
             ePageShow,
             EventNameType_HTMLBodyOrFramesetOnly,
             eBasicEventClass)
WINDOW_EVENT(popstate,
             ePopState,
             EventNameType_XUL | EventNameType_HTMLBodyOrFramesetOnly,
             eBasicEventClass)
// Not supported yet
// WINDOW_EVENT(redo)
WINDOW_EVENT(storage,
             eStorage,
             EventNameType_HTMLBodyOrFramesetOnly,
             eBasicEventClass)
// Not supported yet
// WINDOW_EVENT(undo)
WINDOW_EVENT(unload,
             eUnload,
             (EventNameType_XUL | EventNameType_SVGSVG |
              EventNameType_HTMLBodyOrFramesetOnly),
             eBasicEventClass)

WINDOW_ONLY_EVENT(devicemotion,
                  eDeviceMotion,
                  EventNameType_None,
                  eBasicEventClass)
WINDOW_ONLY_EVENT(deviceorientation,
                  eDeviceOrientation,
                  EventNameType_None,
                  eBasicEventClass)
WINDOW_ONLY_EVENT(absolutedeviceorientation,
                  eAbsoluteDeviceOrientation,
                  EventNameType_None,
                  eBasicEventClass)
WINDOW_ONLY_EVENT(deviceproximity,
                  eDeviceProximity,
                  EventNameType_None,
                  eBasicEventClass)
WINDOW_ONLY_EVENT(userproximity,
                  eUserProximity,
                  EventNameType_None,
                  eBasicEventClass)
WINDOW_ONLY_EVENT(devicelight,
                  eDeviceLight,
                  EventNameType_None,
                  eBasicEventClass)
WINDOW_ONLY_EVENT(vrdisplayactivate,
                  eVRDisplayActivate,
                  EventNameType_None,
                  eBasicEventClass)
WINDOW_ONLY_EVENT(vrdisplaydeactivate,
                  eVRDisplayDeactivate,
                  EventNameType_None,
                  eBasicEventClass)
WINDOW_ONLY_EVENT(vrdisplayconnect,
                  eVRDisplayConnect,
                  EventNameType_None,
                  eBasicEventClass)
WINDOW_ONLY_EVENT(vrdisplaydisconnect,
                  eVRDisplayDisconnect,
                  EventNameType_None,
                  eBasicEventClass)
WINDOW_ONLY_EVENT(vrdisplaypresentchange,
                  eVRDisplayPresentChange,
                  EventNameType_None,
                  eBasicEventClass)
// Install events as per W3C Manifest spec
WINDOW_ONLY_EVENT(appinstalled,
                  eAppInstalled,
                  EventNameType_None,
                  eBasicEventClass)


#ifdef MOZ_B2G
WINDOW_ONLY_EVENT(moztimechange,
                  eTimeChange,
                  EventNameType_None,
                  eBasicEventClass)
WINDOW_ONLY_EVENT(moznetworkupload,
                  eNetworkUpload,
                  EventNameType_None,
                  eBasicEventClass)
WINDOW_ONLY_EVENT(moznetworkdownload,
                  eNetworkDownload,
                  EventNameType_None,
                  eBasicEventClass)
#endif // MOZ_B2G

TOUCH_EVENT(touchstart,
            eTouchStart,
            EventNameType_All,
            eTouchEventClass)
TOUCH_EVENT(touchend,
            eTouchEnd,
            EventNameType_All,
            eTouchEventClass)
TOUCH_EVENT(touchmove,
            eTouchMove,
            EventNameType_All,
            eTouchEventClass )
TOUCH_EVENT(touchcancel,
            eTouchCancel,
            EventNameType_All,
            eTouchEventClass)

DOCUMENT_ONLY_EVENT(readystatechange,
                    eReadyStateChange,
                    EventNameType_HTMLXUL,
                    eBasicEventClass)
DOCUMENT_ONLY_EVENT(selectionchange,
                    eSelectionChange,
                    EventNameType_HTMLXUL,
                    eBasicEventClass)

NON_IDL_EVENT(MozMouseHittest,
              eMouseHitTest,
              EventNameType_None,
              eMouseEventClass)

NON_IDL_EVENT(DOMAttrModified,
              eLegacyAttrModified,
              EventNameType_HTMLXUL,
              eMutationEventClass)
NON_IDL_EVENT(DOMCharacterDataModified,
              eLegacyCharacterDataModified,
              EventNameType_HTMLXUL,
              eMutationEventClass)
NON_IDL_EVENT(DOMNodeInserted,
              eLegacyNodeInserted,
              EventNameType_HTMLXUL,
              eMutationEventClass)
NON_IDL_EVENT(DOMNodeRemoved,
              eLegacyNodeRemoved,
              EventNameType_HTMLXUL,
              eMutationEventClass)
NON_IDL_EVENT(DOMNodeInsertedIntoDocument,
              eLegacyNodeInsertedIntoDocument,
              EventNameType_HTMLXUL,
              eMutationEventClass)
NON_IDL_EVENT(DOMNodeRemovedFromDocument,
              eLegacyNodeRemovedFromDocument,
              EventNameType_HTMLXUL,
              eMutationEventClass)
NON_IDL_EVENT(DOMSubtreeModified,
              eLegacySubtreeModified,
              EventNameType_HTMLXUL,
              eMutationEventClass)

NON_IDL_EVENT(DOMActivate,
              eLegacyDOMActivate,
              EventNameType_HTMLXUL,
              eUIEventClass)
NON_IDL_EVENT(DOMFocusIn,
              eLegacyDOMFocusIn,
              EventNameType_HTMLXUL,
              eUIEventClass)
NON_IDL_EVENT(DOMFocusOut,
              eLegacyDOMFocusOut,
              EventNameType_HTMLXUL,
              eUIEventClass)

NON_IDL_EVENT(DOMMouseScroll,
              eLegacyMouseLineOrPageScroll,
              EventNameType_HTMLXUL,
              eMouseScrollEventClass)
NON_IDL_EVENT(MozMousePixelScroll,
              eLegacyMousePixelScroll,
              EventNameType_HTMLXUL,
              eMouseScrollEventClass)

NON_IDL_EVENT(open,
              eOpen,
              EventNameType_None,
              eBasicEventClass)

NON_IDL_EVENT(dataavailable,
              eMediaRecorderDataAvailable,
              EventNameType_None,
              eBasicEventClass)

NON_IDL_EVENT(stop,
              eMediaRecorderStop,
              EventNameType_None,
              eBasicEventClass)

NON_IDL_EVENT(warning,
              eMediaRecorderWarning,
              EventNameType_None,
              eBasicEventClass)

// Events that only have on* attributes on XUL elements

 // "text" event is legacy event for modifying composition string in EditorBase.
 // This shouldn't be used by web/xul apps.  "compositionupdate" should be
 // used instead.
NON_IDL_EVENT(text,
              eCompositionChange,
              EventNameType_XUL,
              eCompositionEventClass)
NON_IDL_EVENT(compositionstart,
              eCompositionStart,
              EventNameType_XUL,
              eCompositionEventClass)
NON_IDL_EVENT(compositionupdate,
              eCompositionUpdate,
              EventNameType_XUL,
              eCompositionEventClass)
NON_IDL_EVENT(compositionend,
              eCompositionEnd,
              EventNameType_XUL,
              eCompositionEventClass)
NON_IDL_EVENT(command,
              eXULCommand,
              EventNameType_XUL,
              eInputEventClass)
NON_IDL_EVENT(close,
              eWindowClose,
              EventNameType_XUL,
              eBasicEventClass)
NON_IDL_EVENT(popupshowing,
              eXULPopupShowing,
              EventNameType_XUL,
              eBasicEventClass)
NON_IDL_EVENT(popupshown,
              eXULPopupShown,
              EventNameType_XUL,
              eBasicEventClass)
NON_IDL_EVENT(popuppositioned,
              eXULPopupPositioned,
              EventNameType_XUL,
              eBasicEventClass)
NON_IDL_EVENT(popuphiding,
              eXULPopupHiding,
              EventNameType_XUL,
              eBasicEventClass)
NON_IDL_EVENT(popuphidden,
              eXULPopupHidden,
              EventNameType_XUL,
              eBasicEventClass)
NON_IDL_EVENT(broadcast,
              eXULBroadcast,
              EventNameType_XUL,
              eBasicEventClass)
NON_IDL_EVENT(commandupdate,
              eXULCommandUpdate,
              EventNameType_XUL,
              eBasicEventClass)
NON_IDL_EVENT(overflow,
              eScrollPortOverflow,
              EventNameType_XUL,
              eBasicEventClass)
NON_IDL_EVENT(underflow,
              eScrollPortUnderflow,
              EventNameType_XUL,
              eBasicEventClass)

// Various SVG events
NON_IDL_EVENT(SVGLoad,
              eSVGLoad,
              EventNameType_None,
              eBasicEventClass)
NON_IDL_EVENT(SVGUnload,
              eSVGUnload,
              EventNameType_None,
              eBasicEventClass)
NON_IDL_EVENT(SVGResize,
              eSVGResize,
              EventNameType_None,
              eBasicEventClass)
NON_IDL_EVENT(SVGScroll,
              eSVGScroll,
              EventNameType_None,
              eBasicEventClass)

// Only map the ID to the real event name when MESSAGE_TO_EVENT is defined.
#ifndef MESSAGE_TO_EVENT
NON_IDL_EVENT(begin,
              eSMILBeginEvent,
              EventNameType_SMIL,
              eBasicEventClass)
#endif
NON_IDL_EVENT(beginEvent,
              eSMILBeginEvent,
              EventNameType_None,
              eSMILTimeEventClass)
// Only map the ID to the real event name when MESSAGE_TO_EVENT is defined.
#ifndef MESSAGE_TO_EVENT
NON_IDL_EVENT(end,
              eSMILEndEvent,
              EventNameType_SMIL,
              eBasicEventClass)
#endif
NON_IDL_EVENT(endEvent,
              eSMILEndEvent,
              EventNameType_None,
              eSMILTimeEventClass)
// Only map the ID to the real event name when MESSAGE_TO_EVENT is defined.
#ifndef MESSAGE_TO_EVENT
NON_IDL_EVENT(repeat,
              eSMILRepeatEvent,
              EventNameType_SMIL,
              eBasicEventClass)
#endif
NON_IDL_EVENT(repeatEvent,
              eSMILRepeatEvent,
              EventNameType_None,
              eSMILTimeEventClass)

NON_IDL_EVENT(MozAfterPaint,
              eAfterPaint,
              EventNameType_None,
              eBasicEventClass)

NON_IDL_EVENT(MozScrolledAreaChanged,
              eScrolledAreaChanged,
              EventNameType_None,
              eScrollAreaEventClass)

NON_IDL_EVENT(gamepadbuttondown,
              eGamepadButtonDown,
              EventNameType_None,
              eBasicEventClass)
NON_IDL_EVENT(gamepadbuttonup,
              eGamepadButtonUp,
              EventNameType_None,
              eBasicEventClass)
NON_IDL_EVENT(gamepadaxismove,
              eGamepadAxisMove,
              EventNameType_None,
              eBasicEventClass)
NON_IDL_EVENT(gamepadconnected,
              eGamepadConnected,
              EventNameType_None,
              eBasicEventClass)
NON_IDL_EVENT(gamepaddisconnected,
              eGamepadDisconnected,
              EventNameType_None,
              eBasicEventClass)

// Simple gesture events
NON_IDL_EVENT(MozSwipeGestureMayStart,
              eSwipeGestureMayStart,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozSwipeGestureStart,
              eSwipeGestureStart,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozSwipeGestureUpdate,
              eSwipeGestureUpdate,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozSwipeGestureEnd,
              eSwipeGestureEnd,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozSwipeGesture,
              eSwipeGesture,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozMagnifyGestureStart,
              eMagnifyGestureStart,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozMagnifyGestureUpdate,
              eMagnifyGestureUpdate,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozMagnifyGesture,
              eMagnifyGesture,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozRotateGestureStart,
              eRotateGestureStart,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozRotateGestureUpdate,
              eRotateGestureUpdate,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozRotateGesture,
              eRotateGesture,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozTapGesture,
              eTapGesture,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozPressTapGesture,
              ePressTapGesture,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozEdgeUIStarted,
              eEdgeUIStarted,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozEdgeUICanceled,
              eEdgeUICanceled,
              EventNameType_None,
              eSimpleGestureEventClass)
NON_IDL_EVENT(MozEdgeUICompleted,
              eEdgeUICompleted,
              EventNameType_None,
              eSimpleGestureEventClass)

// CSS Transition & Animation events:
EVENT(transitionstart,
      eTransitionStart,
      EventNameType_All,
      eTransitionEventClass)
EVENT(transitionrun,
      eTransitionRun,
      EventNameType_All,
      eTransitionEventClass)
EVENT(transitionend,
      eTransitionEnd,
      EventNameType_All,
      eTransitionEventClass)
EVENT(transitioncancel,
      eTransitionCancel,
      EventNameType_All,
      eTransitionEventClass)
EVENT(animationstart,
      eAnimationStart,
      EventNameType_All,
      eAnimationEventClass)
EVENT(animationend,
      eAnimationEnd,
      EventNameType_All,
      eAnimationEventClass)
EVENT(animationiteration,
      eAnimationIteration,
      EventNameType_All,
      eAnimationEventClass)
EVENT(animationcancel,
      eAnimationCancel,
      EventNameType_All,
      eAnimationEventClass)

// Webkit-prefixed versions of Transition & Animation events, for web compat:
EVENT(webkitAnimationEnd,
      eWebkitAnimationEnd,
      EventNameType_All,
      eAnimationEventClass)
EVENT(webkitAnimationIteration,
      eWebkitAnimationIteration,
      EventNameType_All,
      eAnimationEventClass)
EVENT(webkitAnimationStart,
      eWebkitAnimationStart,
      EventNameType_All,
      eAnimationEventClass)
EVENT(webkitTransitionEnd,
      eWebkitTransitionEnd,
      EventNameType_All,
      eTransitionEventClass)
#ifndef MESSAGE_TO_EVENT
EVENT(webkitanimationend,
      eWebkitAnimationEnd,
      EventNameType_All,
      eAnimationEventClass)
EVENT(webkitanimationiteration,
      eWebkitAnimationIteration,
      EventNameType_All,
      eAnimationEventClass)
EVENT(webkitanimationstart,
      eWebkitAnimationStart,
      EventNameType_All,
      eAnimationEventClass)
EVENT(webkittransitionend,
      eWebkitTransitionEnd,
      EventNameType_All,
      eTransitionEventClass)
#endif

NON_IDL_EVENT(audioprocess,
              eAudioProcess,
              EventNameType_None,
              eBasicEventClass)

NON_IDL_EVENT(complete,
              eAudioComplete,
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

#ifdef MESSAGE_TO_EVENT
#undef EVENT
#undef WINDOW_ONLY_EVENT
#undef TOUCH_EVENT
#undef DOCUMENT_ONLY_EVENT
#undef NON_IDL_EVENT
#endif /* MESSAGE_TO_EVENT */
