// No appearance at all.
#define NS_THEME_NONE                                      0

// A typical dialog button.
#define NS_THEME_BUTTON                                    1

// A radio element within a radio group.
#define NS_THEME_RADIO                                     2

// A checkbox element. 
#define NS_THEME_CHECKBOX                                  3

// A small radio button, for HTML forms
#define NS_THEME_RADIO_SMALL                               4

// A small checkbox, for HTML forms
#define NS_THEME_CHECKBOX_SMALL                            5

// A small button, for HTML forms
#define NS_THEME_BUTTON_SMALL                              6

// A rectangular button that contains complex content
// like images (e.g. HTML <button> elements)
#define NS_THEME_BUTTON_BEVEL                              7

// The toolbox that contains the toolbars.
#define NS_THEME_TOOLBOX                                   11

// A toolbar in an application window.
#define NS_THEME_TOOLBAR                                   12

// A single toolbar button (with no associated dropdown)
#define NS_THEME_TOOLBAR_BUTTON                            13

// A dual toolbar button (e.g., a Back button with a dropdown)
#define NS_THEME_TOOLBAR_DUAL_BUTTON                       14

// The dropdown portion of a dual toolbar button
#define NS_THEME_TOOLBAR_DUAL_BUTTON_DROPDOWN              15

// A separator.  Can be horizontal or vertical.
#define NS_THEME_TOOLBAR_SEPARATOR                         16

// The gripper for a toolbar.
#define NS_THEME_TOOLBAR_GRIPPER                           17

// A status bar in a main application window.
#define NS_THEME_STATUSBAR                                 21

// A single pane of a status bar.
#define NS_THEME_STATUSBAR_PANEL                           22

// The resizer background area in a status bar 
// for the resizer widget in the corner of a window.
#define NS_THEME_STATUSBAR_RESIZER_PANEL                   23

// The resizer itself.
#define NS_THEME_RESIZER                                   24

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
#define NS_THEME_TREEVIEW_TWISTY_OPEN                    48

// A horizontal progress bar.
#define NS_THEME_PROGRESSBAR                               51

// The progress bar's progress indicator
#define NS_THEME_PROGRESSBAR_CHUNK                         52

// A vertical progress bar.
#define NS_THEME_PROGRESSBAR_VERTICAL                      53

// A vertical progress chunk
#define NS_THEME_PROGRESSBAR_CHUNK_VERTICAL                54

// A single tab in a tab widget.
#define NS_THEME_TAB                                       61

// A single pane (inside the tabpanels container)
#define NS_THEME_TAB_PANEL                                 62

// The tab just before the selection
#define NS_THEME_TAB_LEFT_EDGE                             63

// The tab just after the selection
#define NS_THEME_TAB_RIGHT_EDGE                            64

// The tab panels container.
#define NS_THEME_TAB_PANELS                                65

// A tooltip
#define NS_THEME_TOOLTIP                                   71

// A spin control (up/down control for time/date pickers)
#define NS_THEME_SPINNER                                   72

// The up button of a spin control
#define NS_THEME_SPINNER_UP_BUTTON                         73

// The down button of a spin control
#define NS_THEME_SPINNER_DOWN_BUTTON                       74

// A scrollbar.
#define NS_THEME_SCROLLBAR                                 81

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

// The gripper that goes on the thumb
#define NS_THEME_SCROLLBAR_GRIPPER_HORIZONTAL              90
#define NS_THEME_SCROLLBAR_GRIPPER_VERTICAL                91

// A textfield or text area
#define NS_THEME_TEXTFIELD                                 95

// The caret of a text area
#define NS_THEME_TEXTFIELD_CARET                           96

// A dropdown list.
#define NS_THEME_DROPDOWN                                  101

// The dropdown button(s) that open up a dropdown list.
#define NS_THEME_DROPDOWN_BUTTON                           102

// The text part of a dropdown list, to left of button
#define NS_THEME_DROPDOWN_TEXT                             103

// An editable textfield with a dropdown list (a combobox)
#define NS_THEME_DROPDOWN_TEXTFIELD                        104

// A slider
#define NS_THEME_SLIDER                                    111

// A slider's thumb
#define NS_THEME_SLIDER_THUMB                              112

// If the platform supports it, the left/right chunks
// of the slider thumb
#define NS_THEME_SLIDER_THUMB_START                        113
#define NS_THEME_SLIDER_THUMB_END                          114

// The ticks for a slider.
#define NS_THEME_SLIDER_TICK                               115

// A generic container that always repaints on state
// changes.  This is a hack to make checkboxes and
// radio buttons work.
#define NS_THEME_CHECKBOX_CONTAINER                        150
#define NS_THEME_RADIO_CONTAINER                           151

// The label part of a checkbox or radio button, used for painting
// a focus outline.
#define NS_THEME_CHECKBOX_LABEL                            152
#define NS_THEME_RADIO_LABEL                               153

// Window and dialog backgrounds
#define NS_THEME_WINDOW                                    200
#define NS_THEME_DIALOG                                    201

// Menu Bar background
#define NS_THEME_MENUBAR                                   210
// Menu Popup background
#define NS_THEME_MENUPOPUP                                 211
// <menu> and <menuitem> appearances
#define NS_THEME_MENUITEM                                  212
