/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPluginTags_h_
#define nsPluginTags_h_

#include "mozilla/Attributes.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIPluginTag.h"
#include "nsITimer.h"
#include "nsString.h"

class nsIURI;

namespace mozilla {
namespace dom {
struct FakePluginTagInit;
}  // namespace dom
}  // namespace mozilla

// An interface representing plugin tags internally.
#define NS_IINTERNALPLUGINTAG_IID                    \
  {                                                  \
    0xe8fdd227, 0x27da, 0x46ee, {                    \
      0xbe, 0xf3, 0x1a, 0xef, 0x5a, 0x8f, 0xc5, 0xb4 \
    }                                                \
  }

class nsIInternalPluginTag : public nsIPluginTag {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IINTERNALPLUGINTAG_IID)

  nsIInternalPluginTag();
  nsIInternalPluginTag(const char* aName, const char* aDescription,
                       const char* aFileName, const char* aVersion);
  nsIInternalPluginTag(const char* aName, const char* aDescription,
                       const char* aFileName, const char* aVersion,
                       const nsTArray<nsCString>& aMimeTypes,
                       const nsTArray<nsCString>& aMimeDescriptions,
                       const nsTArray<nsCString>& aExtensions);

  virtual bool IsEnabled() = 0;
  virtual const nsCString& GetNiceFileName() = 0;

  const nsCString& Name() const { return mName; }
  const nsCString& Description() const { return mDescription; }

  const nsTArray<nsCString>& MimeTypes() const { return mMimeTypes; }

  const nsTArray<nsCString>& MimeDescriptions() const {
    return mMimeDescriptions;
  }

  const nsTArray<nsCString>& Extensions() const { return mExtensions; }

  const nsCString& FileName() const { return mFileName; }

  const nsCString& Version() const { return mVersion; }

  // Returns true if this plugin claims it supports this MIME type.  The
  // comparison is done ASCII-case-insensitively.
  bool HasMimeType(const nsACString& aMimeType) const;

  // Returns true if this plugin claims it supports the given extension.  In
  // that case, aMatchingType is set to the MIME type the plugin claims
  // corresponds to this extension.  The match on aExtension is done
  // ASCII-case-insensitively.
  bool HasExtension(const nsACString& aExtension,
                    /* out */ nsACString& aMatchingType) const;

  // These must match the STATE_* values in nsIPluginTag.idl
  enum PluginState {
    ePluginState_Disabled = 0,
    ePluginState_Clicktoplay = 1,
    ePluginState_Enabled = 2,
    ePluginState_MaxValue = 3,
  };

 protected:
  ~nsIInternalPluginTag();

  nsCString mName;                        // UTF-8
  nsCString mDescription;                 // UTF-8
  nsCString mFileName;                    // UTF-8
  nsCString mVersion;                     // UTF-8
  nsTArray<nsCString> mMimeTypes;         // UTF-8
  nsTArray<nsCString> mMimeDescriptions;  // UTF-8
  nsTArray<nsCString> mExtensions;        // UTF-8

  static uint32_t sNextId;
};
NS_DEFINE_STATIC_IID_ACCESSOR(nsIInternalPluginTag, NS_IINTERNALPLUGINTAG_IID)

// A class representing "fake" plugin tags for Javascript-based plugins.
// There are currently no other types of supported plugins.
class nsFakePluginTag : public nsIInternalPluginTag, public nsIFakePluginTag {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPLUGINTAG
  NS_DECL_NSIFAKEPLUGINTAG

  static nsresult Create(const mozilla::dom::FakePluginTagInit& aInitDictionary,
                         nsFakePluginTag** aPluginTag);
  nsFakePluginTag(uint32_t aId, already_AddRefed<nsIURI>&& aHandlerURI,
                  const char* aName, const char* aDescription,
                  const nsTArray<nsCString>& aMimeTypes,
                  const nsTArray<nsCString>& aMimeDescriptions,
                  const nsTArray<nsCString>& aExtensions,
                  const nsCString& aNiceName, const nsString& aSandboxScript);

  bool IsEnabled() override;
  const nsCString& GetNiceFileName() override;

  bool HandlerURIMatches(nsIURI* aURI);

  nsIURI* HandlerURI() const { return mHandlerURI; }

  uint32_t Id() const { return mId; }

  const nsString& SandboxScript() const { return mSandboxScript; }

  static const int32_t NOT_JSPLUGIN = -1;

 private:
  nsFakePluginTag();
  virtual ~nsFakePluginTag();

  // A unique id for this JS-implemented plugin. Registering a plugin through
  // nsPluginHost::RegisterFakePlugin assigns a new id. The id is transferred
  // through IPC when getting the list of JS-implemented plugins from child
  // processes, so it should be consistent across processes.
  // 0 is a valid id.
  uint32_t mId;

  // The URI of the handler for our fake plugin.
  // FIXME-jsplugins do we need to sanity check these?
  nsCOMPtr<nsIURI> mHandlerURI;

  nsCString mFullPath;
  nsCString mNiceName;

  nsString mSandboxScript;

  PluginState mState;
};

#endif  // nsPluginTags_h_
