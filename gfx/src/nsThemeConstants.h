/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// No appearance at all.
#define NS_THEME_NONE                                      0

// A typical dialog button.
#define NS_THEME_BUTTON                                    1

// A radio element within a radio group.
#define NS_THEME_RADIO                                     2

// A checkbox element. 
#define NS_THEME_CHECKBOX                                  3

// A rectangular button that contains complex content
// like images (e.g. HTML <button> elements)
#define NS_THEME_BUTTON_BEVEL                              7

// A themed focus outline (for outline:auto)
#define NS_THEME_FOCUS_OUTLINE                             8

// The toolbox that contains the toolbars.
#define NS_THEME_TOOLBOX                                   11

// A toolbar in an application window.
#define NS_THEME_TOOLBAR                                   12

// A single toolbar button (with no associated dropdown)
#define NS_THEME_TOOLBAR_BUTTON                            13

// A dual toolbar button (e.g., a Back button with a dropdown)
#define NS_THEME_TOOLBAR_DUAL_BUTTON                       14

// The dropdown portion of a toolbar button
#define NS_THEME_TOOLBAR_BUTTON_DROPDOWN                   15

// Various arrows that go in buttons
#define NS_THEME_BUTTON_ARROW_UP                           16
#define NS_THEME_BUTTON_ARROW_DOWN                         17
#define NS_THEME_BUTTON_ARROW_NEXT                         18
#define NS_THEME_BUTTON_ARROW_PREVIOUS                     19

// A separator.  Can be horizontal or vertical.
#define NS_THEME_TOOLBAR_SEPARATOR                         20

// The gripper for a toolbar.
#define NS_THEME_TOOLBAR_GRIPPER                           21

// A splitter.  Can be horizontal or vertical.
#define NS_THEME_SPLITTER                                  22

// A status bar in a main application window.
#define NS_THEME_STATUSBAR                                 23

// A single pane of a status bar.
#define NS_THEME_STATUSBAR_PANEL                           24

// The resizer background area in a status bar 
// for the resizer widget in the corner of a window.
#define NS_THEME_STATUSBAR_RESIZER_PANEL                   25

// The resizer itself.
#define NS_THEME_RESIZER                                   26

// List boxes
#define NS_THEME_LISTBOX                                   31

// A listbox item
#define NS_THEME_LISTBOX_LISTITEM                          32

// A tree widget
#define NS_THEME_TREEVIEW                                  41

// A tree item
#define NS_THEME_TREEVIEW_TREEITEM                         42

// A tree widget twisty
#define NS_THEME_TREEVIEW_TWISTY                           43

// A tree widget branch line
#define NS_THEME_TREEVIEW_LINE                             44

// A listbox or tree widget header
#define NS_THEME_TREEVIEW_HEADER                           45

// An individual header cell
#define NS_THEME_TREEVIEW_HEADER_CELL                      46

// The sort arrow for a header.
#define NS_THEME_TREEVIEW_HEADER_SORTARROW                 47

// Open tree widget twisty
#define NS_THEME_TREEVIEW_TWISTY_OPEN                      48

// A horizontal progress bar.
#define NS_THEME_PROGRESSBAR                               51

// The progress bar's progress indicator
#define NS_THEME_PROGRESSBAR_CHUNK                         52

// A vertical progress bar.
#define NS_THEME_PROGRESSBAR_VERTICAL                      53

// A vertical progress chunk
#define NS_THEME_PROGRESSBAR_CHUNK_VERTICAL                54

// A horizontal meter bar.
#define NS_THEME_METERBAR                                  55

// The meter bar's meter indicator
#define NS_THEME_METERBAR_CHUNK                            56

// A single tab in a tab widget.
#define NS_THEME_TAB                                       61

// A single pane (inside the tabpanels container)
#define NS_THEME_TAB_PANEL                                 62

// The tab panels container.
#define NS_THEME_TAB_PANELS                                65

// The tabs scroll arrows (left/right)
#define NS_THEME_TAB_SCROLLARROW_BACK                      66
#define NS_THEME_TAB_SCROLLARROW_FORWARD                   67

// A tooltip
#define NS_THEME_TOOLTIP                                   71

// A spin control (up/down control for time/date pickers)
#define NS_THEME_SPINNER                                   72

// The up button of a spin control
#define NS_THEME_SPINNER_UP_BUTTON                         73

// The down button of a spin control
#define NS_THEME_SPINNER_DOWN_BUTTON                       74

// The textfield of a spin control
#define NS_THEME_SPINNER_TEXTFIELD                         75

// For HTML's <input type=number>
#define NS_THEME_NUMBER_INPUT                              76

// A scrollbar.
#define NS_THEME_SCROLLBAR                                 80

// A small scrollbar.
#define NS_THEME_SCROLLBAR_SMALL                           81

// A scrollbar button (up/down/left/right)
#define NS_THEME_SCROLLBAR_BUTTON_UP                       82
#define NS_THEME_SCROLLBAR_BUTTON_DOWN                     83
#define NS_THEME_SCROLLBAR_BUTTON_LEFT                     84
#define NS_THEME_SCROLLBAR_BUTTON_RIGHT                    85

// The scrollbar track
#define NS_THEME_SCROLLBAR_TRACK_HORIZONTAL                86
#define NS_THEME_SCROLLBAR_TRACK_VERTICAL                  87

// The scrollbar thumb
#define NS_THEME_SCROLLBAR_THUMB_HORIZONTAL                88
#define NS_THEME_SCROLLBAR_THUMB_VERTICAL                  89

// A non-disappearing scrollbar.
#define NS_THEME_SCROLLBAR_NON_DISAPPEARING                90

// A textfield or text area
#define NS_THEME_TEXTFIELD                                 95

// The caret of a text area
#define NS_THEME_TEXTFIELD_CARET                           96

// A multiline text field
#define NS_THEME_TEXTFIELD_MULTILINE                       97

// A searchfield
#define NS_THEME_SEARCHFIELD                               98

// A dropdown list.
#define NS_THEME_DROPDOWN                                  101

// The dropdown button(s) that open up a dropdown list.
#define NS_THEME_DROPDOWN_BUTTON                           102

// The text part of a dropdown list, to left of button
#define NS_THEME_DROPDOWN_TEXT                             103

// An editable textfield with a dropdown list (a combobox)
#define NS_THEME_DROPDOWN_TEXTFIELD                        104

// A slider
#define NS_THEME_SCALE_HORIZONTAL                         111
#define NS_THEME_SCALE_VERTICAL                           112

// A slider's thumb
#define NS_THEME_SCALE_THUMB_HORIZONTAL                   113
#define NS_THEME_SCALE_THUMB_VERTICAL                     114

// If the platform supports it, the left/right chunks
// of the slider thumb
#define NS_THEME_SCALE_THUMB_START                        115
#define NS_THEME_SCALE_THUMB_END                          116

// The ticks for a slider.
#define NS_THEME_SCALE_TICK                               117

// nsRangeFrame and its subparts
#define NS_THEME_RANGE                                    120
#define NS_THEME_RANGE_THUMB                              121

// A groupbox
#define NS_THEME_GROUPBOX                                  149

// A generic container that always repaints on state
// changes.  This is a hack to make checkboxes and
// radio buttons work.
#define NS_THEME_CHECKBOX_CONTAINER                        150
#define NS_THEME_RADIO_CONTAINER                           151

// The label part of a checkbox or radio button, used for painting
// a focus outline.
#define NS_THEME_CHECKBOX_LABEL                            152
#define NS_THEME_RADIO_LABEL                               153

// The focus outline box inside of a button
#define NS_THEME_BUTTON_FOCUS                              154

// Window and dialog backgrounds
#define NS_THEME_WINDOW                                    200
#define NS_THEME_DIALOG                                    201

// Menu Bar background
#define NS_THEME_MENUBAR                                   210
// Menu Popup background
#define NS_THEME_MENUPOPUP                                 211
// <menu> and <menuitem> appearances
#define NS_THEME_MENUITEM                                  212
#define NS_THEME_CHECKMENUITEM                             213
#define NS_THEME_RADIOMENUITEM                             214

// menu checkbox/radio appearances
#define NS_THEME_MENUCHECKBOX                              215
#define NS_THEME_MENURADIO                                 216
#define NS_THEME_MENUSEPARATOR                             217
#define NS_THEME_MENUARROW                                 218
// An image in the menu gutter, like in bookmarks or history
#define NS_THEME_MENUIMAGE                                 219
// For text on non-iconic menuitems only
#define NS_THEME_MENUITEMTEXT                              220

// Vista Rebars
#define NS_THEME_WIN_COMMUNICATIONS_TOOLBOX                221
#define NS_THEME_WIN_MEDIA_TOOLBOX                         222
#define NS_THEME_WIN_BROWSER_TAB_BAR_TOOLBOX               223

// Unified toolbar and titlebar elements on the Mac
#define NS_THEME_MOZ_MAC_UNIFIED_TOOLBAR                   224
#define NS_THEME_MOZ_MAC_FULLSCREEN_BUTTON                 226

// Mac help button
#define NS_THEME_MOZ_MAC_HELP_BUTTON                       227

// Vista glass
#define NS_THEME_WIN_BORDERLESS_GLASS                      229
#define NS_THEME_WIN_GLASS                                 230

// Windows themed window frame elements
#define NS_THEME_WINDOW_TITLEBAR                           231
#define NS_THEME_WINDOW_TITLEBAR_MAXIMIZED                 232
#define NS_THEME_WINDOW_FRAME_LEFT                         233
#define NS_THEME_WINDOW_FRAME_RIGHT                        234
#define NS_THEME_WINDOW_FRAME_BOTTOM                       235
#define NS_THEME_WINDOW_BUTTON_CLOSE                       236
#define NS_THEME_WINDOW_BUTTON_MINIMIZE                    237
#define NS_THEME_WINDOW_BUTTON_MAXIMIZE                    238
#define NS_THEME_WINDOW_BUTTON_RESTORE                     239
#define NS_THEME_WINDOW_BUTTON_BOX                         240
#define NS_THEME_WINDOW_BUTTON_BOX_MAXIMIZED               241

// moz-apperance style used in setting proper glass margins
#define NS_THEME_WIN_EXCLUDE_GLASS                         242

#define NS_THEME_MAC_VIBRANCY_LIGHT                        243
#define NS_THEME_MAC_VIBRANCY_DARK                         244
