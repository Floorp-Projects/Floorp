/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-2000 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#ifndef nsGUIEvent_h__
#define nsGUIEvent_h__

/**
 * Events sent to an nsIGUIEventListener from an nsIWindow
 * 
 * @file nsGUIEvent.h
 * @version 1.1
 */

#include "nsRect2.h"
#include "nsPoint2.h"

// nsIDOMEvent contains a long enum which includes a member called ERROR,
// which conflicts with something that Windows defines somewhere.
// So, undefine it:
#ifdef WIN32
#undef ERROR
#endif
#include "nsIDOMKeyEvent.h"

class nsIDrawable;
class nsIRegion;
class nsIWindow;
class nsIMenuItem;

/**
 * Return status for event processors.
 * @enum nsEventStatus
 */
enum nsEventStatus {
  /// The event is ignored, do default processing
  nsEventStatus_eIgnore,           

  /// The event is consumed, don't do default processing
  nsEventStatus_eConsumeNoDefault,

  /// The event is consumed, but do default processing
  nsEventStatus_eConsumeDoDefault
};

/**
 * sizemode is an adjunct to widget size
 * @enum nsSizeMode
 */
enum nsSizeMode {
  nsSizeMode_Normal = 0,
  nsSizeMode_Minimized,
  nsSizeMode_Maximized
};

/**
 * different types of (top-level) window z-level positioning
 * @enum nsWindowZ
 */
enum nsWindowZ {
  /// on top
  nsWindowZTop = 0,

  /// on bottom
  nsWindowZBottom,

  /// just below some specified widget
  nsWindowZRelative
};

/**
 * General event
 * @struct nsEvent
 * @attention NEED DOCS
 */
struct nsEvent {
  /// @see event_struct_types
  PRUint8     eventStructType;

  /// @see messages,
  PRUint32    message;

  /// in widget relative coordinates, modified to be relative to current view in layout.
  nsPoint2     point;

  /// in widget relative coordinates, not modified by layout code.
  nsPoint2     refPoint;

  /**
   * elapsed time, in milliseconds, from the time the
   * system was started to the time the message was created
   */
  PRUint32    time;

  /// flags to hold event flow stage and capture/bubble cancellation status            
  PRUint32    flags;

  /// flags for indicating more event state for Mozilla applications.
  PRUint32    internalAppFlags;
};

/**
 * General graphic user interface event
 * @struct nsGUIEvent
 * @attention NEED DOCS
 */
struct nsGUIEvent : public nsEvent {
  /// Originator of the event
  nsIWindow  *window;
  /// Internal platform specific message.
  void       *nativeMsg;
};

/**
 * Window resize event
 * @struct nsSizeEvent
 * @attention NEED DOCS
 */
struct nsSizeEvent : public nsGUIEvent {
  /// x,y width, height in pixels (client area)
  nsRect2          *windowSize;

  /// width of entire window (in pixels)
  PRInt32         mWinWidth;

  /// height of entire window (in pixels)
  PRInt32         mWinHeight;
};

/**
 * Window size mode event
 * @struct nsSizeModeEvent
 * @attention NEED DOCS
 */
struct nsSizeModeEvent : public nsGUIEvent {

    nsSizeMode      mSizeMode;
};

/**
 * Window z-level event
 * @struct nsZLevelEvent
 * @attention NEED DOCS
 */
struct nsZLevelEvent : public nsGUIEvent {

  nsWindowZ  mPlacement;

  /**
   * widget we request being below, if any
   * widget to be below, returned by handler
   */
  nsIWindow *mReqBelow, *mActualBelow;

  /// handler should make changes immediately
  PRBool     mImmediate;
  /// handler changed placement
  PRBool     mAdjusted;
};

/**
 * Window repaint event
 * @struct nsPaintEvent
 * @attention NEED DOCS
 */
struct nsPaintEvent : public nsGUIEvent {
  /// Context to paint in.
  nsIDrawable *drawable;

  /// area to paint  (should be used instead of rect)
  nsIRegion   *region;

  /**
   * x,y, width, height in pixels of area to paint
   * @deprecated use region
   */
  nsRect2      *rect;
};

/**
 * Scrollbar event
 * @struct nsScrollPortEvent
 * @note what is this used for???
 * @attention NEED DOCS
 */
struct nsScrollPortEvent : public nsGUIEvent {
  enum orientType {
    vertical   = 0,
    horizontal = 1,
    both       = 2
  };

  orientType orient;
};

/**
 * Input event
 * @struct nsInputEvent
 * @attention NEED DOCS
 * @note couldn't this be a bitfield?
 */
struct nsInputEvent : public nsGUIEvent {
  /// PR_TRUE indicates the shift key is down
  PRBool  isShift;

  /// PR_TRUE indicates the control key is down
  PRBool  isControl;

  /// PR_TRUE indicates the alt key is down
  PRBool  isAlt;

  /// PR_TRUE indicates the meta key is down
  /// (or, on Mac, the Command key)
  PRBool  isMeta;

};

/**
 * Mouse event
 * @struct nsMouseEvent
 * @attention NEED DOCS
 */
struct nsMouseEvent : public nsInputEvent {
  /// area to paint  (should be used instead of rect)
  PRUint32  clickCount;

  /**
   * Special return code for MOUSE_ACTIVATE to signal
   * if the target accepts activation (1), or denies it (0)
   */
  PRBool    acceptActivation;  

};

/**
 * Keyboard event
 * @struct nsKeyEvent
 * @attention NEED DOCS
 */
struct nsKeyEvent : public nsInputEvent {
  /// @see nsIDOMKeyEvent
  PRUint32  keyCode;

  /// OS translated Unicode char
  PRUint32  charCode;

  /// indicates whether the event signifies a printable character
  PRBool    isChar;      
};


/**
 * @struct nsMouseScrollEvent
 *
 * @attention NEED DOCS
 */
struct nsMouseScrollEvent : public nsInputEvent {
  PRInt32                 deltaLines;
};

/**
 * Scrollbar event
 * @struct nsScrollbarEvent
 * @deprecated no more real native scrollbars
 */
struct nsScrollbarEvent : public nsGUIEvent {
  /**
   * ranges between scrollbar 0 and (maxRange - thumbSize).
   * @see nsIScrollbar
   */
  PRUint32        position; 
};



/**
 * IME related events
 * @defgroup ime_events
 */

/**
 * @struct nsTextRange
 * @ingroup ime_events
 *
 * @attention NEED DOCS
 */
struct nsTextRange {
  PRUint32  mStartOffset;
  PRUint32  mEndOffset;
  PRUint32  mRangeType;
};

/**
 * nsTextRange typedef
 * @var typedef struct nsTextRange nsTextRange
 * @ingroup ime_events
 * @attention do we need this?  Isn't a struct in c++ treated like a class (ignoring private/etc)?
 */
typedef struct nsTextRange nsTextRange;

/**
 * nsTextRangeArray typedef
 * @var typedef nsTextRange* nsTextRangeArray
 * @ingroup ime_events
 * @attention why do we need this?????
 */
typedef nsTextRange* nsTextRangeArray;

/**
 * nsTextRangeArray typedef
 * @struct nsTextEventReply
 * @ingroup ime_events
 * @attention why do we need this?????
 */
struct nsTextEventReply {
  nsRect2    mCursorPosition;
  PRBool    mCursorIsCollapsed;
};

/**
 * nsTextEventReply typedef
 * @var typedef struct nsTextEventReply nsTextEventReply
 * @ingroup ime_events
 * @attention do we need this?  Isn't a struct in c++ treated like a class (ignoring private/etc)?
 */
typedef struct nsTextEventReply nsTextEventReply;

/**
 * @struct nsTextEvent
 * @ingroup ime_events
 *
 * @attention NEED DOCS
 */
struct nsTextEvent : public nsInputEvent {
  PRUnichar          *theText;
  nsTextEventReply   theReply;

  PRUint32           rangeCount;
  nsTextRangeArray   rangeArray;

  PRBool             isChar;
};

/**
 * @struct nsCompositionEvent
 * @ingroup ime_events
 *
 * @attention NEED DOCS
 */
struct nsCompositionEvent : public nsInputEvent {
  PRUint32      compositionMessage;
  nsTextEventReply  theReply;
};

/**
 * @struct nsReconversionEventReply
 * @ingroup ime_events
 *
 * @attention NEED DOCS
 */
struct nsReconversionEventReply {
  PRUnichar *mReconversionString;
};

/**
 * @struct nsReconversionEvent
 * @ingroup ime_events
 *
 * @attention NEED DOCS
 */
struct nsReconversionEvent : public nsInputEvent {
  nsReconversionEventReply  theReply;
};

/**
 * MenuItem event
 * 
 * When this event occurs the widget field in nsGUIEvent holds the "target"
 * for the event
 *
 * @struct nsMenuEvent
 *
 * @attention NEED DOCS
 */
struct nsMenuEvent : public nsGUIEvent {
  nsIMenuItem * mMenuItem;
  PRUint32      mCommand;           
};

/**
 * Event status for D&D Event
 * @enum nsDragDropEventStatus
 */
enum nsDragDropEventStatus {  
  // The event is a enter
  nsDragDropEventStatus_eDragEntered,
  // The event is exit
  nsDragDropEventStatus_eDragExited,
  // The event is drop
  nsDragDropEventStatus_eDrop
};

/**
 * Event Struct Types
 * @name event_struct_types
 */
//@{
#define NS_EVENT               1
#define NS_GUI_EVENT           2
#define NS_SIZE_EVENT          3
#define NS_SIZEMODE_EVENT      4
#define NS_ZLEVEL_EVENT        5
#define NS_PAINT_EVENT         6
#define NS_SCROLLBAR_EVENT     7
#define NS_INPUT_EVENT         8
#define NS_KEY_EVENT           9
#define NS_MOUSE_EVENT        10
#define NS_MENU_EVENT         11
#define NS_DRAGDROP_EVENT     12
#define NS_TEXT_EVENT         13
#define NS_COMPOSITION_START  14
#define NS_COMPOSITION_END    15
#define NS_MOUSE_SCROLL_EVENT 16
#define NS_COMPOSITION_QUERY  17
#define NS_SCROLLPORT_EVENT   18
#define NS_RECONVERSION_QUERY 19
//@}


/**
 * GUI MESSAGES
 * @name messages
 */
//@{

#define NS_WINDOW_START                 100

/**
 * A window is being created.
 * @note This message should be sent at the end of nsITopLevelWindow::init(), nsIChildWindow::init() or nsIPopupWindow::init().
 * @see NS_DESTROY
 * @see nsGUIEvent
 */
#define NS_CREATE                       (NS_WINDOW_START)

/**
 * A window is being destroyed (has a refcount of 0).
 * @note This message should be sent when any object implimenting an nsIWindow gets a refcount of 0.
 * @see NS_CREATE
 * @see nsGUIEvent
 */
#define NS_DESTROY                      (NS_WINDOW_START + 2)


/**
 * The OS wants to destroy this window (due to a user clicking close, etc)
 * @attention can we renamed this NS_DELETE or something?
 */
#define NS_XUL_CLOSE                    (NS_WINDOW_START + 1)



/**
 * A window was resized.
 * @note This message should be sent when a window is resized by the OS.
 * @note (This message should not come directly from nsIWindow::resize() or nsIWindow::moveResize(), but when the OS actually resizes the window).
 * @see nsGUIEvent
 */
#define NS_SIZE                         (NS_WINDOW_START + 3)

/**
 * NS_RESIZE_EVENT
 * @attention WTF IS THIS?
 */ 
#define NS_RESIZE_EVENT                 (NS_WINDOW_START + 60)

/**
 * Window size mode was changed
 * @attention wtf is this?
 */
#define NS_SIZEMODE                     (NS_WINDOW_START + 4)

/** 
 * A window has been moved to a new location.
 * The events point contains the x, y location in parent relative coordinates, or screen coordinates if there is no parent.
 * @attention this used to be screen relative coordinates
 * @see nsGUIEvent
 */
#define NS_MOVE                         (NS_WINDOW_START + 34) 


/**
 * Window gained focus
 * @note This message should be sent when the OS gets focus gets focus.
 * @note This message should come AFTER another window sent a NS_LOSTFOCUS message.
 * @see NS_LOSTFOCUS
 * @see NS_ACTIVATE
 * @see nsGUIEvent
 */
#define NS_GOTFOCUS                     (NS_WINDOW_START + 5)

/**
 * Window lost focus
 * @note This message should be sent when the OS gets focus gets focus.
 * @note This message should come BEFORE another window sends a NS_GOTFOCUS message.
 * @see NS_GOTFOCUS
 * @see NS_DEACTIVATE
 * @see nsGUIEvent
 */
#define NS_LOSTFOCUS                    (NS_WINDOW_START + 6)

/**
 * A top-level window has gained focus of the keyboard and mouse.
 * Got activated
 * @attention is this description correct?
 * @note This message should be sent when the OS gives a top-level window focus.
 * @note You must also send an NS_GOTFOCUS message. (seperatly)
 * @see NS_DEACTIVATE
 * @see NS_GOTFOCUS
 * @see nsGUIEvent
 */
#define NS_ACTIVATE                     (NS_WINDOW_START + 7)

/**
 * A top-level window has lost focus of the keyboard and mouse.
 * Got deactivated
 * @attention is this description correct?
 * @note This message should be sent when a top-level window loses focus.
 * @note This message should come BEFORE another window sends a NS_ACTIVATE message
 * @note You must also send an NS_LOSTFOCUS message. (seperatly)
 * @see NS_ACTIVATE
 * @see NS_LOSTFOCUS
 * @see nsGUIEvent
 */
#define NS_DEACTIVATE                   (NS_WINDOW_START + 8)


/**
 * top-level window z-level change request
 * @attention wtf is this?
 */
#define NS_SETZLEVEL                    (NS_WINDOW_START + 9)

/**
 * Widget needs to be repainted
 * @note This message should be sent when the OS wants an area of a window to be repainted.
 *       The OS usually generates these due to an expose event or an invalidation.
 * @see nsPaintEvent
 */
#define NS_PAINT                        (NS_WINDOW_START + 30)


/**
 * Key events
 * @defgroup key_event_types Key events
 * @see nsKeyEvent
 */

/**
 * Key is pressed within a window
 * @ingroup key_event_types
 * @attention NEED DOCS
 * @see nsKeyEvent
 */
#define NS_KEY_PRESS                    (NS_WINDOW_START + 31)

/**
 * Key is released within a window
 * @ingroup key_event_types
 * @attention NEED DOCS
 * @see nsKeyEvent
 */
#define NS_KEY_UP                       (NS_WINDOW_START + 32)

/**
 * Key is pressed within a window
 * @ingroup key_event_types
 * @attention NEED DOCS
 * @see nsKeyEvent
 */
#define NS_KEY_DOWN                     (NS_WINDOW_START + 33)




/// Tab control's selected tab has changed
#define NS_TABCHANGE                    (NS_WINDOW_START + 35)


/**
 * Form control changed: currently == combo box selection changed
 * but could be expanded to mean textbox, checkbox changed, etc.
 * This is a GUI specific event that does not necessarily correspond
 * directly to a mouse click or a key press.
 */
#define NS_CONTROL_CHANGE                (NS_WINDOW_START + 39)

/**
 * Indicates the display has changed depth
 * @note This event should be sent when the output device's bitdepth has changed
 * @see nsGUIEvent
 */
#define NS_DISPLAYCHANGED                (NS_WINDOW_START + 40)

/**
 * Indicates a script error has occurred
 * @attention WTF IS THIS?
 */
#define NS_SCRIPT_ERROR                 (NS_WINDOW_START + 50)

/**
 * ...
 * @attention WTF IS THIS?
 */
#define NS_PLUGIN_ACTIVATE               (NS_WINDOW_START + 62)




#define NS_MOUSE_MESSAGE_START          300

/**
 * The mouse location has changed
 * @attention NEED DOCS
 * @see nsMouseEvent
 */
#define NS_MOUSE_MOVE                   (NS_MOUSE_MESSAGE_START)


/**
 * The mouse button has been pressed down
 * @defgroup mouse_down_event_types Mouse down events
 * @note This event should be sent when the user presses a mouse button down.
 * @note This event should be sent BEFORE a mouse up event
 * @see mouse_up_events
 * @see nsMouseEvent
 */

/**
 * The left button was pushed
 * @ingroup mouse_down_event_types
 */
#define NS_MOUSE_LEFT_BUTTON_DOWN       (NS_MOUSE_MESSAGE_START + 2)

/**
 * The left button was pushed
 * @ingroup mouse_down_event_types
 */
#define NS_MOUSE_MIDDLE_BUTTON_DOWN     (NS_MOUSE_MESSAGE_START + 11)

/**
 * The left button was pushed
 * @ingroup mouse_down_event_types
 */
#define NS_MOUSE_RIGHT_BUTTON_DOWN      (NS_MOUSE_MESSAGE_START + 21)


/**
 * The mouse button has been released
 * @defgroup mouse_up_event_types Mouse up events
 * @note This event should be sent when the user release a mouse button.
 * @note This event should be sent AFTER a mouse down event
 * @see mouse_down_events
 * @see nsMouseEvent
 */

/**
 * The left button was released
 * @ingroup mouse_up_event_types
 */
#define NS_MOUSE_LEFT_BUTTON_UP         (NS_MOUSE_MESSAGE_START + 1)

/**
 * The middle button was released
 * @ingroup mouse_up_event_types
 */
#define NS_MOUSE_MIDDLE_BUTTON_UP       (NS_MOUSE_MESSAGE_START + 10)

/**
 * The right button was released
 * @ingroup mouse_up_event_types
 */
#define NS_MOUSE_RIGHT_BUTTON_UP        (NS_MOUSE_MESSAGE_START + 20)


#define NS_MOUSE_ENTER                  (NS_MOUSE_MESSAGE_START + 22)
#define NS_MOUSE_EXIT                   (NS_MOUSE_MESSAGE_START + 23)
#define NS_MOUSE_LEFT_DOUBLECLICK       (NS_MOUSE_MESSAGE_START + 24)
#define NS_MOUSE_MIDDLE_DOUBLECLICK     (NS_MOUSE_MESSAGE_START + 25)
#define NS_MOUSE_RIGHT_DOUBLECLICK      (NS_MOUSE_MESSAGE_START + 26)
#define NS_MOUSE_LEFT_CLICK             (NS_MOUSE_MESSAGE_START + 27)
#define NS_MOUSE_MIDDLE_CLICK           (NS_MOUSE_MESSAGE_START + 28)
#define NS_MOUSE_RIGHT_CLICK            (NS_MOUSE_MESSAGE_START + 29)
#define NS_MOUSE_ACTIVATE               (NS_MOUSE_MESSAGE_START + 30)
#define NS_MOUSE_ENTER_SYNTH            (NS_MOUSE_MESSAGE_START + 31)
#define NS_MOUSE_EXIT_SYNTH             (NS_MOUSE_MESSAGE_START + 32)




#define NS_SCROLL_EVENT                 (NS_WINDOW_START + 61)


#define NS_SCROLLBAR_MESSAGE_START      1000
#define NS_SCROLLBAR_POS                (NS_SCROLLBAR_MESSAGE_START)
#define NS_SCROLLBAR_PAGE_NEXT          (NS_SCROLLBAR_MESSAGE_START + 1)
#define NS_SCROLLBAR_PAGE_PREV          (NS_SCROLLBAR_MESSAGE_START + 2)
#define NS_SCROLLBAR_LINE_NEXT          (NS_SCROLLBAR_MESSAGE_START + 3)
#define NS_SCROLLBAR_LINE_PREV          (NS_SCROLLBAR_MESSAGE_START + 4)


#define NS_STREAM_EVENT_START           1100
#define NS_PAGE_LOAD                    (NS_STREAM_EVENT_START)
#define NS_PAGE_UNLOAD                  (NS_STREAM_EVENT_START + 1)
#define NS_IMAGE_LOAD                   (NS_STREAM_EVENT_START + 2)
#define NS_IMAGE_ABORT                  (NS_STREAM_EVENT_START + 3)
#define NS_IMAGE_ERROR                  (NS_STREAM_EVENT_START + 4)

#define NS_FORM_EVENT_START             1200
#define NS_FORM_SUBMIT                  (NS_FORM_EVENT_START)
#define NS_FORM_RESET                   (NS_FORM_EVENT_START + 1)
#define NS_FORM_CHANGE                  (NS_FORM_EVENT_START + 2)
#define NS_FORM_SELECTED                (NS_FORM_EVENT_START + 3)
#define NS_FORM_INPUT                   (NS_FORM_EVENT_START + 4)

///Need separate focus/blur notifications for non-native widgets
#define NS_FOCUS_EVENT_START            1300
#define NS_FOCUS_CONTENT                (NS_FOCUS_EVENT_START)
#define NS_BLUR_CONTENT                 (NS_FOCUS_EVENT_START + 1)


#define NS_DRAGDROP_EVENT_START         1400
#define NS_DRAGDROP_ENTER               (NS_DRAGDROP_EVENT_START)
#define NS_DRAGDROP_OVER                (NS_DRAGDROP_EVENT_START + 1)
#define NS_DRAGDROP_EXIT                (NS_DRAGDROP_EVENT_START + 2)
#define NS_DRAGDROP_DROP                (NS_DRAGDROP_EVENT_START + 3)
#define NS_DRAGDROP_GESTURE             (NS_DRAGDROP_EVENT_START + 4)
#define NS_DRAGDROP_OVER_SYNTH          (NS_DRAGDROP_EVENT_START + 1)
#define NS_DRAGDROP_EXIT_SYNTH          (NS_DRAGDROP_EVENT_START + 2)




/// Menu item selected
#define NS_MENU_SELECTED                (NS_WINDOW_START + 38)



// Events for popups
#define NS_MENU_EVENT_START            1500
#define NS_MENU_CREATE                (NS_MENU_EVENT_START)
#define NS_MENU_DESTROY               (NS_MENU_EVENT_START+1)
#define NS_MENU_ACTION                (NS_MENU_EVENT_START+2)
#define NS_XUL_BROADCAST              (NS_MENU_EVENT_START+3)
#define NS_XUL_COMMAND_UPDATE         (NS_MENU_EVENT_START+4)
//@}

// Scroll events
#define NS_MOUSE_SCROLL_START         1600
#define NS_MOUSE_SCROLL               (NS_MOUSE_SCROLL_START)


#define NS_SCROLL_EVENT                 (NS_WINDOW_START + 61)


#define NS_SCROLLPORT_START           1700
#define NS_SCROLLPORT_UNDERFLOW       (NS_SCROLLPORT_START)
#define NS_SCROLLPORT_OVERFLOW        (NS_SCROLLPORT_START+1)
#define NS_SCROLLPORT_OVERFLOWCHANGED (NS_SCROLLPORT_START+2)

#define NS_IS_MOUSE_EVENT(evnt) \
       (((evnt)->message == NS_MOUSE_LEFT_BUTTON_DOWN) || \
        ((evnt)->message == NS_MOUSE_LEFT_BUTTON_UP) || \
        ((evnt)->message == NS_MOUSE_LEFT_CLICK) || \
        ((evnt)->message == NS_MOUSE_LEFT_DOUBLECLICK) || \
        ((evnt)->message == NS_MOUSE_MIDDLE_BUTTON_DOWN) || \
        ((evnt)->message == NS_MOUSE_MIDDLE_BUTTON_UP) || \
        ((evnt)->message == NS_MOUSE_MIDDLE_CLICK) || \
        ((evnt)->message == NS_MOUSE_MIDDLE_DOUBLECLICK) || \
        ((evnt)->message == NS_MOUSE_RIGHT_BUTTON_DOWN) || \
        ((evnt)->message == NS_MOUSE_RIGHT_BUTTON_UP) || \
        ((evnt)->message == NS_MOUSE_RIGHT_CLICK) || \
        ((evnt)->message == NS_MOUSE_RIGHT_DOUBLECLICK) || \
        ((evnt)->message == NS_MOUSE_ENTER) || \
        ((evnt)->message == NS_MOUSE_EXIT) || \
        ((evnt)->message == NS_MOUSE_ENTER) || \
        ((evnt)->message == NS_MOUSE_EXIT) || \
        ((evnt)->message == NS_MOUSE_ENTER_SYNTH) || \
        ((evnt)->message == NS_MOUSE_EXIT_SYNTH) || \
        ((evnt)->message == NS_MOUSE_MOVE))

#define NS_IS_DRAG_EVENT(evnt) \
       (((evnt)->message == NS_DRAGDROP_ENTER) || \
        ((evnt)->message == NS_DRAGDROP_OVER) || \
        ((evnt)->message == NS_DRAGDROP_EXIT) || \
        ((evnt)->message == NS_DRAGDROP_DROP) || \
        ((evnt)->message == NS_DRAGDROP_GESTURE) || \
        ((evnt)->message == NS_DRAGDROP_OVER_SYNTH) || \
        ((evnt)->message == NS_DRAGDROP_EXIT_SYNTH))

#define NS_IS_KEY_EVENT(evnt) \
       (((evnt)->message == NS_KEY_DOWN) ||  \
        ((evnt)->message == NS_KEY_PRESS) || \
        ((evnt)->message == NS_KEY_UP))

#define NS_IS_IME_EVENT(evnt) \
       (((evnt)->message == NS_TEXT_EVENT) ||  \
        ((evnt)->message == NS_COMPOSITION_START) ||  \
        ((evnt)->message == NS_COMPOSITION_END) || \
        ((evnt)->message == NS_RECONVERSION_QUERY) || \
        ((evnt)->message == NS_COMPOSITION_QUERY))

#define NS_EVENT_FLAG_NONE          0x0000
#define NS_EVENT_FLAG_INIT          0x0001
#define NS_EVENT_FLAG_BUBBLE        0x0002
#define NS_EVENT_FLAG_CAPTURE       0x0004
#define NS_EVENT_FLAG_STOP_DISPATCH 0x0008
#define NS_EVENT_FLAG_NO_DEFAULT    0x0010
#define NS_EVENT_FLAG_CANT_CANCEL   0x0020
#define NS_EVENT_FLAG_CANT_BUBBLE   0x0040

#define NS_APP_EVENT_FLAG_NONE      0x0000

/// Similar to NS_EVENT_FLAG_NO_DEFAULT, but it allows focus
#define NS_APP_EVENT_FLAG_HANDLED   0x0001 

/**
 * IME Constants
 * @name textrange_defines
 * @note keep in synch with nsIDOMTextRange.h
 * @note why don't we use the values that nsIDOMTextRange uses???
 */
//@{
#define NS_TEXTRANGE_CARETPOSITION				0x01
#define NS_TEXTRANGE_RAWINPUT					0X02
#define NS_TEXTRANGE_SELECTEDRAWTEXT			0x03
#define NS_TEXTRANGE_CONVERTEDTEXT				0x04
#define NS_TEXTRANGE_SELECTEDCONVERTEDTEXT		0x05
//@}

#endif // nsGUIEvent_h__

