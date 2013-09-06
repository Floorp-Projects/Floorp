/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_COLORMANAGEMENT_H
#define GFX_COLORMANAGEMENT_H

#include "nsIObserver.h"
#include "nsCOMPtr.h"
#include "gfxColor.h"
#include "nsString.h"
#include "qcms.h"

class gfxColorManagement;
namespace mozilla {
class Mutex;
}

enum eCMSMode {
    eCMSMode_Off          = 0,     // No color management
    eCMSMode_All          = 1,     // Color manage everything
    eCMSMode_TaggedOnly   = 2,     // Color manage tagged Images Only
    eCMSMode_AllCount     = 3
};

/**
 * Preferences, utilities and CMS profile management. These are "global".
 * The first use should be calling gfxColorManagement::InstanceNC().Initialize()
 * on the main thread.
 */
class gfxColorManagement MOZ_FINAL
{
public:
  /**
   * Manage the singleton.  Instance() will create it if it isn't there.
   */
  static const gfxColorManagement& Instance();
  static void Destroy();
  static bool Exists();

  /**
   * This must be called on the main thread.
   */
  void Initialize(qcms_profile* aDefaultPlatformProfile);

  /**
   * Reset all non-pref values, release various qcms profiles and transforms
   */
  void ResetAll();

  /**
   * The result of this may depend on the default platform profile set in
   * the Initialize() method.
   */
  qcms_profile* GetOutputProfile() const;

  /**
   *The following are going to set the cached values when first called, after
   * that reusing them until a relevant pref change or a ResetAll() call.
   */
  eCMSMode GetMode() const;
  int GetRenderingIntent() const;

  qcms_profile* GetsRGBProfile() const;
  qcms_transform* GetRGBTransform() const;
  qcms_transform* GetInverseRGBTransform() const;
  qcms_transform* GetRGBATransform() const;

  /**
   * Convert a pixel using a cms transform in an endian-aware manner.
   *
   * Sets aOut to aIn if aTransform is nullptr.
   */
  void TransformPixel(const gfxRGBA& aIn, gfxRGBA& aOut, qcms_transform* aTransform) const;

private:
  nsCOMPtr<nsIObserver> mPrefsObserver;
  mozilla::Mutex* mPrefLock;
  bool mPrefEnableV4;
  bool mPrefForcesRGB;
  int32_t mPrefMode;
  int32_t mPrefIntent;
  nsAdoptingCString mPrefDisplayProfile;
#ifdef DEBUG
  bool mPrefsInitialized;
#endif

  mutable bool mModeSet;
  mutable eCMSMode mMode;
  mutable int mIntent;

  // These two may point to the same profile
  mutable qcms_profile *mOutputProfile;
  mutable qcms_profile *msRGBProfile;

  mutable qcms_transform *mRGBTransform;
  mutable qcms_transform *mInverseRGBTransform;
  mutable qcms_transform *mRGBATransform;

  qcms_profile* mDefaultPlatformProfile;

private:
  static gfxColorManagement* sInstance;

  gfxColorManagement();
  ~gfxColorManagement();

  // =delete would have been nice here, but it isn't supported by all compilers
  // as of Sep 2013.
  gfxColorManagement(const gfxColorManagement&);
  gfxColorManagement& operator=(const gfxColorManagement&);

  void MigratePreferences();
  NS_IMETHODIMP PreferencesModified(const PRUnichar* prefName);

  // Only friends can get to the non-const version.  This is a bit arbitrary
  // as we currently don't have a reason to allow others access, but it isn't
  // really a hard requirement.
  friend class gfxPlatform;
  friend class CMSPrefsObserver;
  static gfxColorManagement& InstanceNC();
};

#endif /* GFX_COLORMANAGEMENT_H */
