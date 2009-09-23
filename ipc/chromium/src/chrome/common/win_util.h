// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_WIN_UTIL_H_
#define CHROME_COMMON_WIN_UTIL_H_

#include <objbase.h>

#include <string>
#include <vector>

#include "app/gfx/chrome_font.h"
#include "base/fix_wp64.h"
#include "base/gfx/rect.h"
#include "base/scoped_handle.h"

class FilePath;

namespace win_util {

// Import ScopedHandle and friends into this namespace for backwards
// compatibility.  TODO(darin): clean this up!
using ::ScopedHandle;
using ::ScopedFindFileHandle;
using ::ScopedHDC;
using ::ScopedBitmap;
using ::ScopedHRGN;

// Simple scoped memory releaser class for COM allocated memory.
// Example:
//   CoMemReleaser<ITEMIDLIST> file_item;
//   SHGetSomeInfo(&file_item, ...);
//   ...
//   return;  <-- memory released
template<typename T>
class CoMemReleaser {
 public:
  explicit CoMemReleaser() : mem_ptr_(NULL) {}

  ~CoMemReleaser() {
    if (mem_ptr_)
      CoTaskMemFree(mem_ptr_);
  }

  T** operator&() {
    return &mem_ptr_;
  }

  operator T*() {
    return mem_ptr_;
  }

 private:
  T* mem_ptr_;

  DISALLOW_COPY_AND_ASSIGN(CoMemReleaser);
};

// Initializes COM in the constructor (STA), and uninitializes COM in the
// destructor.
class ScopedCOMInitializer {
 public:
  ScopedCOMInitializer() : hr_(CoInitialize(NULL)) {
  }

  ScopedCOMInitializer::~ScopedCOMInitializer() {
    if (SUCCEEDED(hr_))
      CoUninitialize();
  }

  // Returns the error code from CoInitialize(NULL)
  // (called in constructor)
  inline HRESULT error_code() const {
    return hr_;
  }

 protected:
  HRESULT hr_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedCOMInitializer);
};

// Creates a string interpretation of the time of day represented by the given
// SYSTEMTIME that's appropriate for the user's default locale.
// Format can be an empty string (for the default format), or a "format picture"
// as specified in the Windows documentation for GetTimeFormat().
std::wstring FormatSystemTime(const SYSTEMTIME& time,
                              const std::wstring& format);

// Creates a string interpretation of the date represented by the given
// SYSTEMTIME that's appropriate for the user's default locale.
// Format can be an empty string (for the default format), or a "format picture"
// as specified in the Windows documentation for GetDateFormat().
std::wstring FormatSystemDate(const SYSTEMTIME& date,
                              const std::wstring& format);

// Returns the long path name given a short path name. A short path name
// is a path that follows the 8.3 convention and has ~x in it. If the
// path is already a long path name, the function returns the current
// path without modification.
bool ConvertToLongPath(const std::wstring& short_path, std::wstring* long_path);

// Returns true if the current point is close enough to the origin point in
// space and time that it would be considered a double click.
bool IsDoubleClick(const POINT& origin,
                   const POINT& current,
                   DWORD elapsed_time);

// Returns true if the current point is far enough from the origin that it
// would be considered a drag.
bool IsDrag(const POINT& origin, const POINT& current);

// Returns true if we are on Windows Vista and composition is enabled
bool ShouldUseVistaFrame();

// Open or run a file via the Windows shell. In the event that there is no
// default application registered for the file specified by 'full_path',
// ask the user, via the Windows "Open With" dialog, for an application to use
// if 'ask_for_app' is true.
// Returns 'true' on successful open, 'false' otherwise.
bool OpenItemViaShell(const FilePath& full_path, bool ask_for_app);

// The download manager now writes the alternate data stream with the
// zone on all downloads. This function is equivalent to OpenItemViaShell
// without showing the zone warning dialog.
bool OpenItemViaShellNoZoneCheck(const FilePath& full_path,
                                 bool ask_for_app);

// Ask the user, via the Windows "Open With" dialog, for an application to use
// to open the file specified by 'full_path'.
// Returns 'true' on successful open, 'false' otherwise.
bool OpenItemWithExternalApp(const std::wstring& full_path);

// Set up a filter for a Save/Open dialog, which will consist of |file_ext| file
// extensions (internally separated by semicolons), |ext_desc| as the text
// descriptions of the |file_ext| types (optional), and (optionally) the default
// 'All Files' view. The purpose of the filter is to show only files of a
// particular type in a Windows Save/Open dialog box. The resulting filter is
// returned. The filters created here are:
//   1. only files that have 'file_ext' as their extension
//   2. all files (only added if 'include_all_files' is true)
// Example:
//   file_ext: { "*.txt", "*.htm;*.html" }
//   ext_desc: { "Text Document" }
//   returned: "Text Document\0*.txt\0HTML Document\0*.htm;*.html\0"
//             "All Files\0*.*\0\0" (in one big string)
// If a description is not provided for a file extension, it will be retrieved
// from the registry. If the file extension does not exist in the registry, it
// will be omitted from the filter, as it is likely a bogus extension.
std::wstring FormatFilterForExtensions(
    const std::vector<std::wstring>& file_ext,
    const std::vector<std::wstring>& ext_desc,
    bool include_all_files);

// Prompt the user for location to save a file. 'suggested_name' is a full path
// that gives the dialog box a hint as to how to initialize itself.
// For example, a 'suggested_name' of:
//   "C:\Documents and Settings\jojo\My Documents\picture.png"
// will start the dialog in the "C:\Documents and Settings\jojo\My Documents\"
// directory, and filter for .png file types.
// 'owner' is the window to which the dialog box is modal, NULL for a modeless
// dialog box.
// On success,  returns true and 'final_name' contains the full path of the file
// that the user chose. On error, returns false, and 'final_name' is not
// modified.
// NOTE: DO NOT CALL THIS FUNCTION DIRECTLY! Instead use the helper objects in
//       browser/shell_dialogs.cc to do this asynchronously on a different
//       thread so that the app isn't jankified if the Windows shell dialog
//       takes a long time to display.
bool SaveFileAs(HWND owner,
                const std::wstring& suggested_name,
                std::wstring* final_name);

// Prompt the user for location to save a file.
// Callers should provide the filter string, and also a filter index.
// The parameter |index| indicates the initial index of filter description
// and filter pattern for the dialog box. If |index| is zero or greater than
// the number of total filter types, the system uses the first filter in the
// |filter| buffer. |index| is used to specify the initial selected extension,
// and when done contains the extension the user chose. The parameter
// |final_name| returns the file name which contains the drive designator,
// path, file name, and extension of the user selected file name. |def_ext| is
// the default extension to give to the file if the user did not enter an
// extension. If |ignore_suggested_ext| is true, any file extension contained in
// |suggested_name| will not be used to generate the file name. This is useful
// in the case of saving web pages, where we know the extension type already and
// where |suggested_name| may contain a '.' character as a valid part of the
// name, thus confusing our extension detection code.
bool SaveFileAsWithFilter(HWND owner,
                          const std::wstring& suggested_name,
                          const std::wstring& filter,
                          const std::wstring& def_ext,
                          bool ignore_suggested_ext,
                          unsigned* index,
                          std::wstring* final_name);

// This function takes the output of a SaveAs dialog: a filename, a filter and
// the extension originally suggested to the user (shown in the dialog box) and
// returns back the filename with the appropriate extension tacked on. For
// example, if you pass in 'foo' as filename with filter '*.jpg' this function
// will return 'foo.jpg'. It respects MIME types, so if you pass in 'foo.jpeg'
// with filer '*.jpg' it will return 'foo.jpeg' (will not append .jpg).
// |filename| should contain the filename selected in the SaveAs dialog box and
// may include the path, |filter_selected| should be '*.something', for example
// '*.*' or it can be blank (which is treated as *.*). |suggested_ext| should
// contain the extension without the dot (.) in front, for example 'jpg'.
std::wstring AppendExtensionIfNeeded(const std::wstring& filename,
                                     const std::wstring& filter_selected,
                                     const std::wstring& suggested_ext);

// If the window does not fit on the default monitor, it is moved and possibly
// resized appropriately.
void AdjustWindowToFit(HWND hwnd);

// Sizes the window to have a client or window size (depending on the value of
// |pref_is_client|) of pref, then centers the window over parent, ensuring the
// window fits on screen.
void CenterAndSizeWindow(HWND parent, HWND window, const SIZE& pref,
                         bool pref_is_client);

// Returns true if edge |edge| (one of ABE_LEFT, TOP, RIGHT, or BOTTOM) of
// monitor |monitor| has an auto-hiding taskbar that's always-on-top.
bool EdgeHasTopmostAutoHideTaskbar(UINT edge, HMONITOR monitor);

// Duplicates a section handle from another process to the current process.
// Returns the new valid handle if the function succeed. NULL otherwise.
HANDLE GetSectionFromProcess(HANDLE section, HANDLE process, bool read_only);

// Returns true if the specified window is the current active top window or one
// of its children.
bool DoesWindowBelongToActiveWindow(HWND window);

// Adjusts the value of |child_rect| if necessary to ensure that it is
// completely visible within |parent_rect|.
void EnsureRectIsVisibleInRect(const gfx::Rect& parent_rect,
                               gfx::Rect* child_rect,
                               int padding);

// Ensures that the child window stays within the boundaries of the parent
// before setting its bounds. If |parent_window| is NULL, the bounds of the
// parent are assumed to be the bounds of the monitor that |child_window| is
// nearest to. If |child_window| isn't visible yet and |insert_after_window|
// is non-NULL and visible, the monitor |insert_after_window| is on is used
// as the parent bounds instead.
void SetChildBounds(HWND child_window, HWND parent_window,
                    HWND insert_after_window, const gfx::Rect& bounds,
                    int padding, unsigned long flags);

// Returns the bounds for the monitor that contains the largest area of
// intersection with the specified rectangle.
gfx::Rect GetMonitorBoundsForRect(const gfx::Rect& rect);

// Returns true if the virtual key code is a digit coming from the numeric
// keypad (with or without NumLock on).  |extended_key| should be set to the
// extended key flag specified in the WM_KEYDOWN/UP where the |key_code|
// originated.
bool IsNumPadDigit(int key_code, bool extended_key);

// Grabs a snapshot of the designated window and stores a PNG representation
// into a byte vector.
void GrabWindowSnapshot(HWND window_handle,
                        std::vector<unsigned char>* png_representation);

// Returns whether the specified window is the current active window.
bool IsWindowActive(HWND hwnd);

// Returns whether the specified file name is a reserved name on windows.
// This includes names like "com2.zip" (which correspond to devices) and
// desktop.ini and thumbs.db which have special meaning to the windows shell.
bool IsReservedName(const std::wstring& filename);

// Returns whether the specified extension is automatically integrated into the
// windows shell.
bool IsShellIntegratedExtension(const std::wstring& eextension);

// A wrapper around Windows' MessageBox function. Using a Chrome specific
// MessageBox function allows us to control certain RTL locale flags so that
// callers don't have to worry about adding these flags when running in a
// right-to-left locale.
int MessageBox(HWND hwnd,
               const std::wstring& text,
               const std::wstring& caption,
               UINT flags);

// Returns the system set window title font.
ChromeFont GetWindowTitleFont();

// The thickness of an auto-hide taskbar in pixels.
extern const int kAutoHideTaskbarThicknessPx;

}  // namespace win_util

#endif  // CHROME_COMMON_WIN_UTIL_H_
