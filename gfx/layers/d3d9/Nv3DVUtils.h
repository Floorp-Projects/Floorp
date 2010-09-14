/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is NVIDIA Corporation Code.
 *
 * The Initial Developer of the Original Code is NVIDIA Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Atul Apte <aapte135@gmail.com>
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

#ifndef GFX_NV3DVUTILS_H
#define GFX_NV3DVUTILS_H

#include "Layers.h"
#include <windows.h>
#include <d3d9.h>

namespace mozilla {
namespace layers {

#define FIREFOX_3DV_APP_HANDLE    0xECB992B6

typedef enum _Stereo_Mode {
  STEREO_MODE_LEFT_RIGHT = 0,
  STEREO_MODE_RIGHT_LEFT = 1,
  STEREO_MODE_TOP_BOTTOM = 2,
  STEREO_MODE_BOTTOM_TOP = 3,
  STEREO_MODE_LAST       = 4 
} Stereo_Mode;

class INv3DVStreaming : public IUnknown {

public:
  virtual bool Nv3DVInitialize()                  = 0;
  virtual bool Nv3DVRelease()                     = 0;
  virtual bool Nv3DVSetDevice(IUnknown* pDevice)  = 0;
  virtual bool Nv3DVControl(Stereo_Mode eStereoMode, bool bEnableStereo, DWORD dw3DVAppHandle) = 0;
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
  void SendNv3DVControl(Stereo_Mode eStereoMode, bool bEnableStereo, DWORD dw3DVAppHandle);

  /*
   * Send Stereo Metadata. Used mainly to re-route calls 
   * from ImageLayerD3D9 to the 3DV COM object
   */
  void SendNv3DVMetaData(unsigned int dwWidth, unsigned int dwHeight, HANDLE hSrcLuma, HANDLE hDst);

private:

  /* Nv3DVStreaming interface pointer */
  nsRefPtr<INv3DVStreaming> m3DVStreaming;

};


} /* layers */
} /* mozilla */

#endif /* GFX_NV3DVUTILS_H */
