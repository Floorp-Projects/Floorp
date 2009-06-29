// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A wrapper class for working with custom XP/Vista themes provided in
// uxtheme.dll.  This is a singleton class that can be grabbed using
// NativeTheme::instance().
// For more information on visual style parts and states, see:
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/commctls/userex/topics/partsandstates.asp

#ifndef BASE_GFX_NATIVE_THEME_H__
#define BASE_GFX_NATIVE_THEME_H__

#include <windows.h>
#include <uxtheme.h>
#include "base/basictypes.h"
#include "base/gfx/size.h"
#include "skia/include/SkColor.h"

namespace skia {
class PlatformCanvasWin;
}  // namespace skia

namespace gfx {

// TODO: Define class member enums to replace part_id and state_id parameters
// that are currently defined in <vssym32.h>. Afterward, classic_state should
// be removed and class users wouldn't need to include <vssym32.h> anymore.
// This would enable HOT state on non-themed UI (like when RDP'ing) and would
// simplify usage.
// TODO: This class should probably be changed to be platform independent at
// the same time.
class NativeTheme {
 public:
  enum ThemeName {
    BUTTON,
    LIST,
    MENU,
    MENULIST,
    SCROLLBAR,
    STATUS,
    TAB,
    TEXTFIELD,
    TRACKBAR,
    WINDOW,
    LAST
  };

  // This enumeration is used within PaintMenuArrow in order to indicate the
  // direction the menu arrow should point to.
  enum MenuArrowDirection {
    LEFT_POINTING_ARROW,
    RIGHT_POINTING_ARROW
  };

  typedef HRESULT (WINAPI* DrawThemeBackgroundPtr)(HANDLE theme,
                                                   HDC hdc,
                                                   int part_id,
                                                   int state_id,
                                                   const RECT* rect,
                                                   const RECT* clip_rect);
  typedef HRESULT (WINAPI* DrawThemeBackgroundExPtr)(HANDLE theme,
                                                     HDC hdc,
                                                     int part_id,
                                                     int state_id,
                                                     const RECT* rect,
                                                     const DTBGOPTS* opts);
  typedef HRESULT (WINAPI* GetThemeColorPtr)(HANDLE hTheme,
                                             int part_id,
                                             int state_id,
                                             int prop_id,
                                             COLORREF* color);
  typedef HRESULT (WINAPI* GetThemeContentRectPtr)(HANDLE hTheme,
                                                   HDC hdc,
                                                   int part_id,
                                                   int state_id,
                                                   const RECT* rect,
                                                   RECT* content_rect);
  typedef HRESULT (WINAPI* GetThemePartSizePtr)(HANDLE hTheme,
                                                HDC hdc,
                                                int part_id,
                                                int state_id,
                                                RECT* rect,
                                                int ts,
                                                SIZE* size);
  typedef HANDLE (WINAPI* OpenThemeDataPtr)(HWND window,
                                            LPCWSTR class_list);
  typedef HRESULT (WINAPI* CloseThemeDataPtr)(HANDLE theme);

  typedef void (WINAPI* SetThemeAppPropertiesPtr) (DWORD flags);
  typedef BOOL (WINAPI* IsThemeActivePtr)();
  typedef HRESULT (WINAPI* GetThemeIntPtr)(HANDLE hTheme,
                                           int part_id,
                                           int state_id,
                                           int prop_id,
                                           int *value);

  HRESULT PaintButton(HDC hdc,
                      int part_id,
                      int state_id,
                      int classic_state,
                      RECT* rect) const;

  HRESULT PaintDialogBackground(HDC dc, bool active, RECT* rect) const;

  HRESULT PaintListBackground(HDC dc, bool enabled, RECT* rect) const;

  // |arrow_direction| determines whether the arrow is pointing to the left or
  // to the right. In RTL locales, sub-menus open from right to left and
  // therefore the menu arrow should point to the left and not to the right.
  HRESULT PaintMenuArrow(ThemeName theme,
                         HDC hdc,
                         int part_id,
                         int state_id,
                         RECT* rect,
                         MenuArrowDirection arrow_direction,
                         bool is_highlighted) const;

  HRESULT PaintMenuBackground(ThemeName theme,
                              HDC hdc,
                              int part_id,
                              int state_id,
                              RECT* rect) const;

  HRESULT PaintMenuCheck(ThemeName theme,
                         HDC hdc,
                         int part_id,
                         int state_id,
                         RECT* rect,
                         bool is_highlighted) const;

  HRESULT PaintMenuCheckBackground(ThemeName theme,
                                   HDC hdc,
                                   int part_id,
                                   int state_id,
                                   RECT* rect) const;

  HRESULT PaintMenuGutter(HDC hdc,
                          int part_id,
                          int state_id,
                          RECT* rect) const;

  HRESULT PaintMenuItemBackground(ThemeName theme,
                                  HDC hdc,
                                  int part_id,
                                  int state_id,
                                  bool selected,
                                  RECT* rect) const;

  HRESULT PaintMenuList(HDC hdc,
                        int part_id,
                        int state_id,
                        int classic_state,
                        RECT* rect) const;

  HRESULT PaintMenuSeparator(HDC hdc,
                             int part_id,
                             int state_id,
                             RECT* rect) const;

  // Paints a scrollbar arrow.  |classic_state| should have the appropriate
  // classic part number ORed in already.
  HRESULT PaintScrollbarArrow(HDC hdc,
                              int state_id,
                              int classic_state,
                              RECT* rect) const;

  // Paints a scrollbar track section.  |align_rect| is only used in classic
  // mode, and makes sure the checkerboard pattern in |target_rect| is aligned
  // with one presumed to be in |align_rect|.
  HRESULT PaintScrollbarTrack(HDC hdc,
                              int part_id,
                              int state_id,
                              int classic_state,
                              RECT* target_rect,
                              RECT* align_rect,
                              skia::PlatformCanvasWin* canvas) const;

  // Paints a scrollbar thumb or gripper.
  HRESULT PaintScrollbarThumb(HDC hdc,
                              int part_id,
                              int state_id,
                              int classic_state,
                              RECT* rect) const;

  HRESULT PaintStatusGripper(HDC hdc,
                             int part_id,
                             int state_id,
                             int classic_state,
                             RECT* rect) const;

  HRESULT PaintTabPanelBackground(HDC dc, RECT* rect) const;

  HRESULT PaintTextField(HDC hdc,
                         int part_id,
                         int state_id,
                         int classic_state,
                         RECT* rect,
                         COLORREF color,
                         bool fill_content_area,
                         bool draw_edges) const;

  HRESULT PaintTrackbar(HDC hdc,
                        int part_id,
                        int state_id,
                        int classic_state,
                        RECT* rect,
                        skia::PlatformCanvasWin* canvas) const;

  bool IsThemingActive() const;

  HRESULT GetThemePartSize(ThemeName themeName,
                           HDC hdc,
                           int part_id,
                           int state_id,
                           RECT* rect,
                           int ts,
                           SIZE* size) const;

  HRESULT GetThemeColor(ThemeName theme,
                        int part_id,
                        int state_id,
                        int prop_id,
                        SkColor* color) const;

  // Get the theme color if theming is enabled.  If theming is unsupported
  // for this part, use Win32's GetSysColor to find the color specified
  // by default_sys_color.
  SkColor GetThemeColorWithDefault(ThemeName theme,
                                   int part_id,
                                   int state_id,
                                   int prop_id,
                                   int default_sys_color) const;

  HRESULT GetThemeInt(ThemeName theme,
                      int part_id,
                      int state_id,
                      int prop_id,
                      int *result) const;

  // Get the thickness of the border associated with the specified theme,
  // defaulting to GetSystemMetrics edge size if themes are disabled.
  // In Classic Windows, borders are typically 2px; on XP+, they are 1px.
  Size GetThemeBorderSize(ThemeName theme) const;

  // Disables all theming for top-level windows in the entire process, from
  // when this method is called until the process exits.  All the other
  // methods in this class will continue to work, but their output will ignore
  // the user's theme. This is meant for use when running tests that require
  // consistent visual results.
  void DisableTheming() const;

  // Closes cached theme handles so we can unload the DLL or update our UI
  // for a theme change.
  void CloseHandles() const;

  // Gets our singleton instance.
  static const NativeTheme* instance();

 private:
  NativeTheme();
  ~NativeTheme();

  HRESULT PaintFrameControl(HDC hdc,
                            RECT* rect,
                            UINT type,
                            UINT state,
                            bool is_highlighted) const;

  // Returns a handle to the theme data.
  HANDLE GetThemeHandle(ThemeName theme_name) const;

  // Function pointers into uxtheme.dll.
  DrawThemeBackgroundPtr draw_theme_;
  DrawThemeBackgroundExPtr draw_theme_ex_;
  GetThemeColorPtr get_theme_color_;
  GetThemeContentRectPtr get_theme_content_rect_;
  GetThemePartSizePtr get_theme_part_size_;
  OpenThemeDataPtr open_theme_;
  CloseThemeDataPtr close_theme_;
  SetThemeAppPropertiesPtr set_theme_properties_;
  IsThemeActivePtr is_theme_active_;
  GetThemeIntPtr get_theme_int_;

  // Handle to uxtheme.dll.
  HMODULE theme_dll_;

  // A cache of open theme handles.
  mutable HANDLE theme_handles_[LAST];

  DISALLOW_EVIL_CONSTRUCTORS(NativeTheme);
};

}  // namespace gfx

#endif  // BASE_GFX_NATIVE_THEME_H__
