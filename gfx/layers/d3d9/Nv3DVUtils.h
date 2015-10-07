/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_NV3DVUTILS_H
#define GFX_NV3DVUTILS_H

#include "Layers.h"
#include <windows.h>
#include <d3d9.h>

namespace mozilla {
namespace layers {

#define FIREFOX_3DV_APP_HANDLE    0xECB992B6

enum Nv_Stereo_Mode {
  NV_STEREO_MODE_LEFT_RIGHT = 0,
  NV_STEREO_MODE_RIGHT_LEFT = 1,
  NV_STEREO_MODE_TOP_BOTTOM = 2,
  NV_STEREO_MODE_BOTTOM_TOP = 3,
  NV_STEREO_MODE_MONO       = 4,
  NV_STEREO_MODE_LAST       = 5
};

class INv3DVStreaming : public IUnknown {

public:
  virtual bool Nv3DVInitialize()                  = 0;
  virtual bool Nv3DVRelease()                     = 0;
  virtual bool Nv3DVSetDevice(IUnknown* pDevice)  = 0;
  virtual bool Nv3DVControl(Nv_Stereo_Mode eStereoMode, bool bEnableStereo, DWORD dw3DVAppHandle) = 0;
  virtual bool Nv3DVMetaData(DWORD dwWidth, DWORD dwHeight, HANDLE hSrcLuma, HANDLE hDst) = 0;
};

/*
 * Nv3DVUtils class
 */
class Nv3DVUtils {

public:
  Nv3DVUtils();
  ~Nv3DVUtils();

  /*
   * Initializes the Nv3DVUtils object.
   */
  void Initialize();

  /*
   * Release any resources if needed
   *
   */
  void UnInitialize();

  /*
   * Sets the device info, along with any other initialization that is needed after device creation
   * Pass the D3D9 device pointer is an IUnknown input argument
   */
  void SetDeviceInfo(IUnknown *devUnknown);

  /*
   * Send Stereo Control Information. Used mainly to re-route 
   * calls from ImageLayerD3D9 to the 3DV COM object
   */
  void SendNv3DVControl(Nv_Stereo_Mode eStereoMode, bool bEnableStereo, DWORD dw3DVAppHandle);

  /*
   * Send Stereo Metadata. Used mainly to re-route calls 
   * from ImageLayerD3D9 to the 3DV COM object
   */
  void SendNv3DVMetaData(unsigned int dwWidth, unsigned int dwHeight, HANDLE hSrcLuma, HANDLE hDst);

private:

  /* Nv3DVStreaming interface pointer */
  nsRefPtr<INv3DVStreaming> m3DVStreaming;

};


} // namespace layers
} // namespace mozilla

#endif /* GFX_NV3DVUTILS_H */
