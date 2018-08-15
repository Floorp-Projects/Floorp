/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Generated with cbindgen:0.6.1 */

/* DO NOT MODIFY THIS MANUALLY! This file was generated using cbindgen.
 * To generate this file:
 *   1. Get the latest cbindgen using `cargo install --force cbindgen`
 *      a. Alternatively, you can clone `https://github.com/eqrion/cbindgen` and use a tagged release
 *   2. Run `rustup run nightly cbindgen toolkit/library/rust/ --lockfile Cargo.lock --crate style -o layout/style/ServoStyleConsts.h`
 */

#include <cstdint>
#include <cstdlib>

namespace mozilla {

// The value for the `appearance` property.
//
// https://developer.mozilla.org/en-US/docs/Web/CSS/-moz-appearance
//
// NOTE(emilio): When changing this you may want to regenerate the C++ bindings
// (see components/style/cbindgen.toml)
enum class StyleAppearance : uint8_t {
  // No appearance at all.
  None,
  // A typical dialog button.
  Button,
  // Various arrows that go in buttons
  ButtonArrowDown,
  ButtonArrowNext,
  ButtonArrowPrevious,
  ButtonArrowUp,
  // A rectangular button that contains complex content
  // like images (e.g. HTML <button> elements)
  ButtonBevel,
  // The focus outline box inside of a button.
  ButtonFocus,
  // The caret of a text area
  Caret,
  // A dual toolbar button (e.g., a Back button with a dropdown)
  Dualbutton,
  // A groupbox.
  Groupbox,
  // A inner-spin button.
  InnerSpinButton,
  // List boxes.
  Listbox,
  // A listbox item.
  Listitem,
  // Menu Bar background
  Menubar,
  // <menu> and <menuitem> appearances
  Menuitem,
  Checkmenuitem,
  Radiomenuitem,
  // For text on non-iconic menuitems only
  Menuitemtext,
  // A dropdown list.
  Menulist,
  // The dropdown button(s) that open up a dropdown list.
  MenulistButton,
  // The text part of a dropdown list, to left of button.
  MenulistText,
  // An editable textfield with a dropdown list (a combobox).
  MenulistTextfield,
  // Menu Popup background.
  Menupopup,
  // menu checkbox/radio appearances
  Menucheckbox,
  Menuradio,
  Menuseparator,
  Menuarrow,
  // An image in the menu gutter, like in bookmarks or history.
  Menuimage,
  // A horizontal meter bar.
  Meterbar,
  // The meter bar's meter indicator.
  Meterchunk,
  // The "arrowed" part of the dropdown button that open up a dropdown list.
  MozMenulistButton,
  // For HTML's <input type=number>
  NumberInput,
  // A horizontal progress bar.
  Progressbar,
  // The progress bar's progress indicator
  Progresschunk,
  // A vertical progress bar.
  ProgressbarVertical,
  // A vertical progress chunk.
  ProgresschunkVertical,
  // A checkbox element.
  Checkbox,
  // A radio element within a radio group.
  Radio,
  // A generic container that always repaints on state changes. This is a
  // hack to make XUL checkboxes and radio buttons work.
  CheckboxContainer,
  RadioContainer,
  // The label part of a checkbox or radio button, used for painting a focus
  // outline.
  CheckboxLabel,
  RadioLabel,
  // nsRangeFrame and its subparts
  Range,
  RangeThumb,
  // The resizer background area in a status bar for the resizer widget in
  // the corner of a window.
  Resizerpanel,
  // The resizer itself.
  Resizer,
  // A slider.
  ScaleHorizontal,
  ScaleVertical,
  // A slider's thumb.
  ScalethumbHorizontal,
  ScalethumbVertical,
  // If the platform supports it, the left/right chunks of the slider thumb.
  Scalethumbstart,
  Scalethumbend,
  // The ticks for a slider.
  Scalethumbtick,
  // A scrollbar.
  Scrollbar,
  // A small scrollbar.
  ScrollbarSmall,
  // The scrollbar slider
  ScrollbarHorizontal,
  ScrollbarVertical,
  // A scrollbar button (up/down/left/right).
  // Keep these in order (some code casts these values to `int` in order to
  // compare them against each other).
  ScrollbarbuttonUp,
  ScrollbarbuttonDown,
  ScrollbarbuttonLeft,
  ScrollbarbuttonRight,
  // The scrollbar thumb.
  ScrollbarthumbHorizontal,
  ScrollbarthumbVertical,
  // The scrollbar track.
  ScrollbartrackHorizontal,
  ScrollbartrackVertical,
  // The scroll corner
  Scrollcorner,
  // A searchfield.
  Searchfield,
  // A separator.  Can be horizontal or vertical.
  Separator,
  // A spin control (up/down control for time/date pickers).
  Spinner,
  // The up button of a spin control.
  SpinnerUpbutton,
  // The down button of a spin control.
  SpinnerDownbutton,
  // The textfield of a spin control
  SpinnerTextfield,
  // A splitter.  Can be horizontal or vertical.
  Splitter,
  // A status bar in a main application window.
  Statusbar,
  // A single pane of a status bar.
  Statusbarpanel,
  // A single tab in a tab widget.
  Tab,
  // A single pane (inside the tabpanels container).
  Tabpanel,
  // The tab panels container.
  Tabpanels,
  // The tabs scroll arrows (left/right).
  TabScrollArrowBack,
  TabScrollArrowForward,
  // A textfield or text area.
  Textfield,
  // A multiline text field.
  TextfieldMultiline,
  // A toolbar in an application window.
  Toolbar,
  // A single toolbar button (with no associated dropdown).
  Toolbarbutton,
  // The dropdown portion of a toolbar button
  ToolbarbuttonDropdown,
  // The gripper for a toolbar.
  Toolbargripper,
  // The toolbox that contains the toolbars.
  Toolbox,
  // A tooltip.
  Tooltip,
  // A listbox or tree widget header
  Treeheader,
  // An individual header cell
  Treeheadercell,
  // The sort arrow for a header.
  Treeheadersortarrow,
  // A tree item.
  Treeitem,
  // A tree widget branch line
  Treeline,
  // A tree widget twisty.
  Treetwisty,
  // Open tree widget twisty.
  Treetwistyopen,
  // A tree widget.
  Treeview,
  // Window and dialog backgrounds.
  Window,
  Dialog,
  // Vista Rebars.
  MozWinCommunicationsToolbox,
  MozWinMediaToolbox,
  MozWinBrowsertabbarToolbox,
  // Vista glass.
  MozWinGlass,
  MozWinBorderlessGlass,
  // -moz-apperance style used in setting proper glass margins.
  MozWinExcludeGlass,
  // Titlebar elements on the Mac.
  MozMacFullscreenButton,
  // Mac help button.
  MozMacHelpButton,
  // Windows themed window frame elements.
  MozWindowButtonBox,
  MozWindowButtonBoxMaximized,
  MozWindowButtonClose,
  MozWindowButtonMaximize,
  MozWindowButtonMinimize,
  MozWindowButtonRestore,
  MozWindowFrameBottom,
  MozWindowFrameLeft,
  MozWindowFrameRight,
  MozWindowTitlebar,
  MozWindowTitlebarMaximized,
  MozGtkInfoBar,
  MozMacActiveSourceListSelection,
  MozMacDisclosureButtonClosed,
  MozMacDisclosureButtonOpen,
  MozMacSourceList,
  MozMacSourceListSelection,
  MozMacVibrancyDark,
  MozMacVibrancyLight,
  MozMacVibrantTitlebarDark,
  MozMacVibrantTitlebarLight,
  // A non-disappearing scrollbar.
  ScrollbarNonDisappearing,
  // A themed focus outline (for outline:auto).
  //
  // This isn't exposed to CSS at all, just here for convenience.
  FocusOutline,
  // A dummy variant that should be last to let the GTK widget do hackery.
  Count,
};

// Defines an element’s display type, which consists of
// the two basic qualities of how an element generates boxes
// <https://drafts.csswg.org/css-display/#propdef-display>
//
//
// NOTE(emilio): Order is important in Gecko!
//
// If you change it, make sure to take a look at the
// FrameConstructionDataByDisplay stuff (both the XUL and non-XUL version), and
// ensure it's still correct!
//
// Also, when you change this from Gecko you may need to regenerate the
// C++-side bindings (see components/style/cbindgen.toml).
enum class StyleDisplay : uint8_t {
  None = 0,
  Block,
  FlowRoot,
  Inline,
  InlineBlock,
  ListItem,
  Table,
  InlineTable,
  TableRowGroup,
  TableColumn,
  TableColumnGroup,
  TableHeaderGroup,
  TableFooterGroup,
  TableRow,
  TableCell,
  TableCaption,
  Flex,
  InlineFlex,
  Grid,
  InlineGrid,
  Ruby,
  RubyBase,
  RubyBaseContainer,
  RubyText,
  RubyTextContainer,
  Contents,
  WebkitBox,
  WebkitInlineBox,
  MozBox,
  MozInlineBox,
  MozGrid,
  MozInlineGrid,
  MozGridGroup,
  MozGridLine,
  MozStack,
  MozInlineStack,
  MozDeck,
  MozGroupbox,
  MozPopup,
};

// Values for the display-mode media feature.
enum class StyleDisplayMode : uint8_t {
  Browser = 0,
  MinimalUi,
  Standalone,
  Fullscreen,
};

} // namespace mozilla
