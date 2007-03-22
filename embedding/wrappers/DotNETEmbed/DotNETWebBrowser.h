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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roy Yokoyama <yokoyama@netscape.com> (original author)
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

#pragma once

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIBaseWindow.h"
#include "nsIWebNavigation.h"
#include "nsIWindowWatcher.h"
#include "nsIInputStream.h"
#include "nsIURI.h"
#include "nsEmbedAPI.h"
#include "DotNetNetworking.h"
#include "DotNetXPCOM_IO.h"

using namespace System;
using namespace Mozilla::Embedding::Networking;
using namespace Mozilla::Embedding::XPCOM_IO;

namespace Mozilla
{
  namespace Embedding
  {
    namespace WebBrowser
    {
      public __gc class EmbeddingSiteWindow
      {
        private:
          Boolean mVisibility;
          String* mTitle;

        public:
          static const UInt32 DIM_FLAGS_POSITION = 1;
          static const UInt32 DIM_FLAGS_SIZE_INNER = 2;
          static const UInt32 DIM_FLAGS_SIZE_OUTER = 4;

          __property Boolean get_Visibility() { return visibility; };
          __property void set_Visibility(Boolean v) { visibility = v; };
          __property String* get_Title() { return title; };
          __property void set_Title(String * t) { title = t; };

          void setDimensions(UInt32 aFlags, Int32 x, Int32 y, Int32 cx, Int32 cy) {};
          void getDimensions(UInt32 aFlags, Int32 *x, Int32 *y, Int32 *cx, Int32 *cy) {};
          void setFocus() {};
      }; // class EmbeddingSiteWindow

      public __gc class WebBrowserChrome
      {
        public:
          bool setStatus() { return true; };
          bool destroyBrowserWindow() { return true; };
          bool sizeBrowserTo() { return true; };
          bool showAsModal() { return true; };
          bool isWindowModal() { return true; };
          bool exitModalEventLoop() { return true; };
      }; // class WebBrowserChrome

      public __gc class WebNavigation
      {
        public:
          static const UInt32 LOAD_FLAGS_MASK = 65535;
          static const UInt32 LOAD_FLAGS_NONE = 0;
          static const UInt32 LOAD_FLAGS_IS_REFRESH = 16;
          static const UInt32 LOAD_FLAGS_IS_LINK = 32;
          static const UInt32 LOAD_FLAGS_BYPASS_HISTORY = 64;
          static const UInt32 LOAD_FLAGS_REPLACE_HISTORY = 128;
          static const UInt32 LOAD_FLAGS_BYPASS_CACHE = 256;
          static const UInt32 LOAD_FLAGS_BYPASS_PROXY = 512;
          static const UInt32 LOAD_FLAGS_CHARSET_CHANGE = 1024;
          static const UInt32 STOP_NETWORK = 1;
          static const UInt32 STOP_CONTENT = 2;
          static const UInt32 STOP_ALL = 3;

          void goBack() {};
          void goForward() {};
          void gotoIndex(Int32 arg0) {};
          void loadURI(String *arg0, UInt32 arg1, URI *arg2, InputStream *arg3, InputStream *arg4) {}; 
          void reload(UInt32 arg0) {};
          void stop(UInt32 arg0) {};
      }; // class WebNavigation

      public __gc class WebBrowser
      {
        public:
          bool addWebBrowserListener() { return true; };
          bool removeWebBrowserListener() { return true; };
          bool SetContainerWindow(WebBrowserChrome *aChrome) { return true; };
      }; // class WebBrowser

      public __gc class WindowWatcher
      {
        public:
          bool openWindow() { return true; };
          bool registerNotification() { return true; };
          bool unregisterNotification() { return true; };
          bool getWindowEnumerator() { return true; };
          bool getNewPrompter() { return true; };
          bool getNewAuthPrompter() { return true; };
          bool setWindowCreator() { return true; };
      }; //class WindowWatcher

      public __gc class WindowCreator
      {
        public:
          bool createChromeWindow() { return true; };
      }; //class WindowCreator

    } // namespace WebBrowser

  } // namespace Embedding
}
