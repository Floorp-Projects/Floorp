/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPluginTags_h_
#define nsPluginTags_h_

#include "nscore.h"
#include "prtypes.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIPluginTag.h"
#include "nsNPAPIPluginInstance.h"
#include "nsISupportsArray.h"
#include "nsITimer.h"

class nsPluginHost;
struct PRLibrary;
struct nsPluginInfo;

// Remember that flags are written out to pluginreg.dat, be careful
// changing their meaning.
#define NS_PLUGIN_FLAG_ENABLED      0x0001    // is this plugin enabled?
// no longer used                   0x0002    // reuse only if regenerating pluginreg.dat
#define NS_PLUGIN_FLAG_FROMCACHE    0x0004    // this plugintag info was loaded from cache
// no longer used                   0x0008    // reuse only if regenerating pluginreg.dat
#define NS_PLUGIN_FLAG_BLOCKLISTED  0x0010    // this is a blocklisted plugin
#define NS_PLUGIN_FLAG_CLICKTOPLAY  0x0020    // this is a click-to-play plugin

// A linked-list of plugin information that is used for instantiating plugins
// and reflecting plugin information into JavaScript.
class nsPluginTag : public nsIPluginTag
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPLUGINTAG
  
  nsPluginTag(nsPluginTag* aPluginTag);
  nsPluginTag(nsPluginInfo* aPluginInfo);
  nsPluginTag(const char* aName,
              const char* aDescription,
              const char* aFileName,
              const char* aFullPath,
              const char* aVersion,
              const char* const* aMimeTypes,
              const char* const* aMimeDescriptions,
              const char* const* aExtensions,
              PRInt32 aVariants,
              PRInt64 aLastModifiedTime = 0,
              bool aArgsAreUTF8 = false);
  virtual ~nsPluginTag();
  
  void SetHost(nsPluginHost * aHost);
  void TryUnloadPlugin(bool inShutdown);
  void Mark(PRUint32 mask);
  void UnMark(PRUint32 mask);
  bool HasFlag(PRUint32 flag);
  PRUint32 Flags();
  bool HasSameNameAndMimes(const nsPluginTag *aPluginTag) const;
  bool IsEnabled();
  
  nsRefPtr<nsPluginTag> mNext;
  nsPluginHost *mPluginHost;
  nsCString     mName; // UTF-8
  nsCString     mDescription; // UTF-8
  nsTArray<nsCString> mMimeTypes; // UTF-8
  nsTArray<nsCString> mMimeDescriptions; // UTF-8
  nsTArray<nsCString> mExtensions; // UTF-8
  PRLibrary     *mLibrary;
  nsRefPtr<nsNPAPIPlugin> mPlugin;
  bool          mIsJavaPlugin;
  bool          mIsFlashPlugin;
  nsCString     mFileName; // UTF-8
  nsCString     mFullPath; // UTF-8
  nsCString     mVersion;  // UTF-8
  PRInt64       mLastModifiedTime;
  nsCOMPtr<nsITimer> mUnloadTimer;
private:
  PRUint32      mFlags;

  void InitMime(const char* const* aMimeTypes,
                const char* const* aMimeDescriptions,
                const char* const* aExtensions,
                PRUint32 aVariantCount);
  nsresult EnsureMembersAreUTF8();
};

#endif // nsPluginTags_h_
