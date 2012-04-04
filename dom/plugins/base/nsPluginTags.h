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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Josh Aas <josh@mozilla.com>
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
#define NS_PLUGIN_FLAG_UNWANTED     0x0008    // this is an unwanted plugin
#define NS_PLUGIN_FLAG_BLOCKLISTED  0x0010    // this is a blocklisted plugin

// A linked-list of plugin information that is used for instantiating plugins
// and reflecting plugin information into JavaScript.
class nsPluginTag : public nsIPluginTag
{
public:
  enum nsRegisterType {
    ePluginRegister,
    ePluginUnregister
  };
  
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
  bool Equals(nsPluginTag* aPluginTag);
  bool IsEnabled();
  void RegisterWithCategoryManager(bool aOverrideInternalTypes,
                                   nsRegisterType aType = ePluginRegister);
  
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
  bool          mIsNPRuntimeEnabledJavaPlugin;
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
