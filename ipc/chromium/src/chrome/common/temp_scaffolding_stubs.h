// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_TEMP_SCAFFOLDING_STUBS_H_
#define CHROME_COMMON_TEMP_SCAFFOLDING_STUBS_H_

// This file provides declarations and stub definitions for classes we encouter
// during the porting effort. It is not meant to be permanent, and classes will
// be removed from here as they are fleshed out more completely.

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/string16.h"
#include "base/gfx/native_widget_types.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "googleurl/src/gurl.h"
#include "skia/include/SkBitmap.h"

class BookmarkContextMenu;
class BookmarkNode;
class Browser;
class CommandLine;
class DownloadItem;
class MessageLoop;
class NavigationController;
class ProcessSingleton;
class Profile;
class RenderViewHostDelegate;
class SiteInstance;
class URLRequest;
class TabContents;
struct ViewHostMsg_DidPrintPage_Params;

namespace gfx {
class Rect;
class Widget;
}

namespace IPC {
class Message;
}

//---------------------------------------------------------------------------
// These stubs are for Browser_main()

class GoogleUpdateSettings {
 public:
  static bool GetCollectStatsConsent() {
    NOTIMPLEMENTED();
    return false;
  }
  static bool SetCollectStatsConsent(bool consented) {
    NOTIMPLEMENTED();
    return false;
  }
  static bool GetBrowser(std::wstring* browser) {
    NOTIMPLEMENTED();
    return false;
  }
  static bool GetLanguage(std::wstring* language) {
    NOTIMPLEMENTED();
    return false;
  }
  static bool GetBrand(std::wstring* brand) {
    NOTIMPLEMENTED();
    return false;
  }
  static bool GetReferral(std::wstring* referral) {
    NOTIMPLEMENTED();
    return false;
  }
  static bool ClearReferral() {
    NOTIMPLEMENTED();
    return false;
  }
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(GoogleUpdateSettings);
};

void OpenFirstRunDialog(Profile* profile, ProcessSingleton* process_singleton);

void InstallJankometer(const CommandLine&);

//---------------------------------------------------------------------------
// These stubs are for BrowserProcessImpl

class CancelableTask;
class ViewMsg_Print_Params;

// Printing is all (obviously) not implemented.
// http://code.google.com/p/chromium/issues/detail?id=9847
namespace printing {

class PrintViewManager {
 public:
  PrintViewManager(TabContents&) { }
  void Stop() { NOTIMPLEMENTED(); }
  void Destroy() { }
  bool OnRenderViewGone(RenderViewHost*) {
    NOTIMPLEMENTED();
    return true;  // Assume for now that all renderer crashes are important.
  }
  void DidGetPrintedPagesCount(int, int) { NOTIMPLEMENTED(); }
  void DidPrintPage(const ViewHostMsg_DidPrintPage_Params&) {
    NOTIMPLEMENTED();
  }
};

class PrintingContext {
 public:
  enum Result { OK, CANCEL, FAILED };
};

class PrintSettings {
 public:
  void RenderParams(ViewMsg_Print_Params* params) const { NOTIMPLEMENTED(); }
  int dpi() const { NOTIMPLEMENTED(); return 92; }
};

class PrinterQuery : public base::RefCountedThreadSafe<PrinterQuery> {
 public:
  enum GetSettingsAskParam {
    DEFAULTS,
    ASK_USER,
  };

  void GetSettings(GetSettingsAskParam ask_user_for_settings,
                   int parent_window,
                   int expected_page_count,
                   CancelableTask* callback) { NOTIMPLEMENTED(); }
  PrintingContext::Result last_status() { return PrintingContext::FAILED; }
  const PrintSettings& settings() { NOTIMPLEMENTED(); return settings_; }
  int cookie() { NOTIMPLEMENTED(); return 0; }
  void StopWorker() { NOTIMPLEMENTED(); }

 private:
  PrintSettings settings_;
};

class PrintJobManager {
 public:
  void OnQuit() { }
  void PopPrinterQuery(int document_cookie, scoped_refptr<PrinterQuery>* job) {
    NOTIMPLEMENTED();
  }
  void QueuePrinterQuery(PrinterQuery* job) { NOTIMPLEMENTED(); }
};

}  // namespace printing

namespace sandbox {

enum ResultCode {
  SBOX_ALL_OK = 0,
  SBOX_ERROR_GENERIC = 1,
  SBOX_ERROR_BAD_PARAMS = 2,
  SBOX_ERROR_UNSUPPORTED = 3,
  SBOX_ERROR_NO_SPACE = 4,
  SBOX_ERROR_INVALID_IPC = 5,
  SBOX_ERROR_FAILED_IPC = 6,
  SBOX_ERROR_NO_HANDLE = 7,
  SBOX_ERROR_UNEXPECTED_CALL = 8,
  SBOX_ERROR_WAIT_ALREADY_CALLED = 9,
  SBOX_ERROR_CHANNEL_ERROR = 10,
  SBOX_ERROR_LAST
};

class BrokerServices {
 public:
  void Init() { NOTIMPLEMENTED(); }
};

}  // namespace sandbox

struct ViewHostMsg_DidPrintPage_Params;

namespace views {

class AcceleratorHandler {
};

class TableModelObserver {
 public:
  virtual void OnModelChanged() = 0;
  virtual void OnItemsChanged(int, int) = 0;
  virtual void OnItemsAdded(int, int) = 0;
  virtual void OnItemsRemoved(int, int) = 0;
};

class TableModel {
 public:
  int CompareValues(int row1, int row2, int column_id) {
    NOTIMPLEMENTED();
    return 0;
  }
  virtual int RowCount() = 0;
};

class MenuItemView {
 public:
  enum Type {
    NORMAL,
    SUBMENU,
    CHECKBOX,
    RADIO,
    SEPARATOR
  };
  enum AnchorPosition {
    TOPLEFT,
    TOPRIGHT
  };
  MenuItemView(BookmarkContextMenu*) { NOTIMPLEMENTED(); }
  void RunMenuAt(gfx::NativeWindow parent, const gfx::Rect& bounds,
                 AnchorPosition anchor, bool has_mnemonics) {
    NOTIMPLEMENTED();
  }
  void Cancel() { NOTIMPLEMENTED(); }
  void AppendMenuItem(int item_id, const std::wstring& label, Type type) {
    NOTIMPLEMENTED();
  }
  void AppendMenuItemWithLabel(int item_id, const std::wstring& label) {
    NOTIMPLEMENTED();
  }
  void AppendSeparator() { NOTIMPLEMENTED(); }
};

class MenuDelegate {
};

class Window {
 public:
  void Show() { NOTIMPLEMENTED(); }
  virtual void Close() { NOTIMPLEMENTED(); }
};

}  // namespace views

class InputWindowDelegate {
};

class Menu {
 public:
  enum AnchorPoint {
    TOPLEFT,
    TOPRIGHT
  };
  enum MenuItemType {
    NORMAL,
    CHECKBOX,
    RADIO,
    SEPARATOR
  };
  class Delegate {
  };
  Menu(Delegate* delegate, AnchorPoint anchor, gfx::NativeWindow owner) {
    NOTIMPLEMENTED();
  }
  void AppendMenuItem(int item_id, const std::wstring& label,
                      MenuItemType type) {
    NOTIMPLEMENTED();
  }
  void AppendMenuItemWithLabel(int item_id, const std::wstring& label) {
    NOTIMPLEMENTED();
  }
  Menu* AppendSubMenu(int item_id, const std::wstring& label) {
    NOTIMPLEMENTED();
    return NULL;
  }
  void AppendSeparator() { NOTIMPLEMENTED(); }
  void AppendDelegateMenuItem(int item_id) { NOTIMPLEMENTED(); }
};

views::Window* CreateInputWindow(gfx::NativeWindow parent_hwnd,
                                 InputWindowDelegate* delegate);

class BookmarkManagerView {
 public:
   static BookmarkManagerView* current() {
     NOTIMPLEMENTED();
     return NULL;
   }
   static void Show(Profile* profile) { NOTIMPLEMENTED(); }
   void SelectInTree(BookmarkNode* node) { NOTIMPLEMENTED(); }
   Profile* profile() const {
    NOTIMPLEMENTED();
    return NULL;
  }
};

class BookmarkEditorView {
 public:
  class Handler {
  };
  enum Configuration {
    SHOW_TREE,
    NO_TREE
  };
  static void Show(gfx::NativeWindow parent_window, Profile* profile,
                   BookmarkNode* parent, BookmarkNode* node,
                   Configuration configuration, Handler* handler) {
    NOTIMPLEMENTED();
  }
};

//---------------------------------------------------------------------------
// These stubs are for Browser

namespace download_util {
void DragDownload(const DownloadItem* download, SkBitmap* icon);
}  // namespace download_util

class IconLoader {
 public:
  enum IconSize {
    SMALL = 0,  // 16x16
    NORMAL,     // 32x32
    LARGE
  };
};

class IconManager : public CancelableRequestProvider {
 public:
  typedef CancelableRequestProvider::Handle Handle;
  typedef Callback2<Handle, SkBitmap*>::Type IconRequestCallback;
  SkBitmap* LookupIcon(const FilePath&, IconLoader::IconSize)
      { NOTIMPLEMENTED(); return NULL; }
  Handle LoadIcon(const FilePath&, IconLoader::IconSize,
                  CancelableRequestConsumerBase*, IconRequestCallback*)
      { NOTIMPLEMENTED(); return NULL; }
};

class DebuggerWindow : public base::RefCountedThreadSafe<DebuggerWindow> {
 public:
};

class FaviconStatus {
 public:
  const GURL& url() const { return url_; }
 private:
  GURL url_;
};

class DockInfo {
 public:
  bool GetNewWindowBounds(gfx::Rect*, bool*) const {
    NOTIMPLEMENTED();
    return false;
  }
  void AdjustOtherWindowBounds() const { NOTIMPLEMENTED(); }
};

class WindowSizer {
 public:
  static void GetBrowserWindowBounds(const std::wstring& app_name,
                                     const gfx::Rect& specified_bounds,
                                     Browser* browser,
                                     gfx::Rect* window_bounds,
                                     bool* maximized);
};

//---------------------------------------------------------------------------
// These stubs are for Profile

class WebAppLauncher {
 public:
  static void Launch(Profile* profile, const GURL& url) {
    NOTIMPLEMENTED();
  }
};

//---------------------------------------------------------------------------
// These stubs are for TabContents

class WebApp : public base::RefCountedThreadSafe<WebApp> {
 public:
  class Observer {
   public:
  };
  void AddObserver(Observer* obs) { NOTIMPLEMENTED(); }
  void RemoveObserver(Observer* obs) { NOTIMPLEMENTED(); }
  void SetTabContents(TabContents*) { NOTIMPLEMENTED(); }
  SkBitmap GetFavIcon() {
    NOTIMPLEMENTED();
    return SkBitmap();
  }
};

class ModalHtmlDialogDelegate : public HtmlDialogUIDelegate {
 public:
  ModalHtmlDialogDelegate(const GURL&, int, int, const std::string&,
                          IPC::Message*, TabContents*) { }

   virtual bool IsDialogModal() const { return true; }
   virtual std::wstring GetDialogTitle() const { return std::wstring(); }
   virtual GURL GetDialogContentURL() const { return GURL(); }
   virtual void GetDialogSize(gfx::Size* size) const {}
   virtual std::string GetDialogArgs() const { return std::string(); }
   virtual void OnDialogClosed(const std::string& json_retval) {}
};

class HtmlDialogContents {
 public:
  struct HtmlDialogParams {
    GURL url;
    int width;
    int height;
    std::string json_input;
  };
};

class LoginHandler {
 public:
  void SetAuth(const std::wstring& username,
               const std::wstring& password) {
    NOTIMPLEMENTED();
  }
  void CancelAuth() { NOTIMPLEMENTED(); }
  void OnRequestCancelled() { NOTIMPLEMENTED(); }
};

namespace net {
class AuthChallengeInfo;
}

LoginHandler* CreateLoginPrompt(net::AuthChallengeInfo* auth_info,
                                URLRequest* request,
                                MessageLoop* ui_loop);

class RepostFormWarningDialog {
 public:
  static void RunRepostFormWarningDialog(NavigationController*) { }
  virtual ~RepostFormWarningDialog() { }
};

class PageInfoWindow {
 public:
  enum TabID {
    GENERAL = 0,
    SECURITY,
  };
  static void CreatePageInfo(Profile* profile, NavigationEntry* nav_entry,
                             gfx::NativeView parent_hwnd, TabID tab) {
    NOTIMPLEMENTED();
  }
  static void CreateFrameInfo(Profile* profile, const GURL& url,
                              const NavigationEntry::SSLStatus& ssl,
                              gfx::NativeView parent_hwnd, TabID tab) {
    NOTIMPLEMENTED();
  }
};

class FontsLanguagesWindowView {
 public:
  explicit FontsLanguagesWindowView(Profile* profile) { NOTIMPLEMENTED(); }
  void SelectLanguagesTab() { NOTIMPLEMENTED(); }
};

class OSExchangeData {
 public:
  void SetString(const std::wstring& data) { NOTIMPLEMENTED(); }
  void SetURL(const GURL& url, const std::wstring& title) { NOTIMPLEMENTED(); }
};

class BaseDragSource {
};

//---------------------------------------------------------------------------
// These stubs are for extensions

namespace views {
class HWNDView {
 public:
  int width() { NOTIMPLEMENTED(); return 0; }
  int height() { NOTIMPLEMENTED(); return 0; }
  void InitHidden() { NOTIMPLEMENTED(); }
  void set_preferred_size(const gfx::Size& size) { NOTIMPLEMENTED(); }
  virtual void SetBackground(const SkBitmap&) { NOTIMPLEMENTED(); }
  virtual void SetVisible(bool flag) { NOTIMPLEMENTED(); }
  void SizeToPreferredSize() { NOTIMPLEMENTED(); }
  bool IsVisible() const { NOTIMPLEMENTED(); return false; }
  void Layout() { NOTIMPLEMENTED(); }
  void SchedulePaint() { NOTIMPLEMENTED(); }
  HWNDView* GetParent() const { NOTIMPLEMENTED(); return NULL; }
  virtual gfx::Size GetPreferredSize() { NOTIMPLEMENTED(); return gfx::Size(); }
  gfx::NativeWindow GetHWND() { NOTIMPLEMENTED(); return 0; }
  void Detach() { NOTIMPLEMENTED(); }
  gfx::Widget* GetWidget() { NOTIMPLEMENTED(); return NULL; }
};
}  // namespace views

class HWNDHtmlView : public views::HWNDView {
 public:
  HWNDHtmlView(const GURL& content_url, RenderViewHostDelegate* delegate,
               bool allow_dom_ui_bindings, SiteInstance* instance) {
    NOTIMPLEMENTED();
  }
  virtual ~HWNDHtmlView() {}

  RenderViewHost* render_view_host() { NOTIMPLEMENTED(); return NULL; }
  SiteInstance* site_instance() { NOTIMPLEMENTED(); return NULL; }
};


#endif  // CHROME_COMMON_TEMP_SCAFFOLDING_STUBS_H_
