// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_PLUGIN_LIB_H_
#define CHROME_COMMON_CHROME_PLUGIN_LIB_H_

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/ref_counted.h"
#include "chrome/common/chrome_plugin_api.h"

class MessageLoop;

// A ChromePluginLib is a single Chrome Plugin Library.
// This class is used in the browser process (IO thread), and the plugin process
// (plugin thread).  It should not be accessed on other threads, because it
// issues a NOTIFY_CHROME_PLUGIN_UNLOADED notification.
class ChromePluginLib : public base::RefCounted<ChromePluginLib>  {
 public:
  static bool IsInitialized();
  static ChromePluginLib* Create(const FilePath& filename,
                                 const CPBrowserFuncs* bfuncs);
  static ChromePluginLib* Find(const FilePath& filename);
  static void Destroy(const FilePath& filename);
  static bool IsPluginThread();
  static MessageLoop* GetPluginThreadLoop();

  static ChromePluginLib* FromCPID(CPID id) {
    return reinterpret_cast<ChromePluginLib*>(id);
  }

  // Adds Chrome plugins to the NPAPI plugin list.
  static void RegisterPluginsWithNPAPI();

  // Loads all the plugins that are marked as "LoadOnStartup" in the
  // registry. This should only be called in the browser process.
  static void LoadChromePlugins(const CPBrowserFuncs* bfuncs);

  // Unloads all the loaded plugins and cleans up the plugin map.
  static void UnloadAllPlugins();

  // Returns true if the plugin is currently loaded.
  const bool is_loaded() const { return initialized_; }

  // Get the Plugin's function pointer table.
  const CPPluginFuncs& functions() const;

  CPID cpid() { return reinterpret_cast<CPID>(this); }

  const FilePath& filename() { return filename_; }

  // Plugin API functions

  // Method to call a test function in the plugin, used for unit tests.
  int CP_Test(void* param);

#if defined(OS_WIN)
  // The registry path to search for Chrome Plugins/
  static const TCHAR kRegistryChromePlugins[];
#endif  // defined(OS_WIN)

 private:
  friend class base::RefCounted<ChromePluginLib>;

  ChromePluginLib(const FilePath& filename);
  ~ChromePluginLib();

  // Method to initialize a Plugin.
  // Initialize can be safely called multiple times.
  bool CP_Initialize(const CPBrowserFuncs* bfuncs);

  // Method to shutdown a Plugin.
  void CP_Shutdown();

  // Attempts to load the plugin.
  // Returns true if it is a legitimate plugin, false otherwise
  bool Load();

  // Unloads the plugin.
  void Unload();

  FilePath filename_;  // the path to the plugin
#if defined(OS_WIN)
  // TODO(port): Remove ifdefs when we have portable replacement for HMODULE.
  HMODULE module_;  // the opened plugin handle
#endif  // defined(OS_WIN)
  bool initialized_;  // is the plugin initialized

  // Exported symbols from the plugin, looked up by name.
  CP_VersionNegotiateFunc CP_VersionNegotiate_;
  CP_InitializeFunc CP_Initialize_;

  // Additional function pointers provided by the plugin.
  CPPluginFuncs plugin_funcs_;

  // Used for unit tests.
  typedef int (STDCALL *CP_TestFunc)(void*);
  CP_TestFunc CP_Test_;

  DISALLOW_COPY_AND_ASSIGN(ChromePluginLib);
};

#endif  // CHROME_COMMON_CHROME_PLUGIN_LIB_H_
