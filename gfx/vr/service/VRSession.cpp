#include "VRSession.h"

#include "moz_external_vr.h"

#if defined(XP_WIN)
#include <d3d11.h>
#endif // defined(XP_WIN)

using namespace mozilla::gfx;

VRSession::VRSession()
{

}

VRSession::~VRSession()
{

}

#if defined(XP_WIN)
bool
VRSession::CreateD3DContext(RefPtr<ID3D11Device> aDevice)
{
  if (!mDevice) {
    if (!aDevice) {
      NS_WARNING("OpenVRSession::CreateD3DObjects failed to get a D3D11Device");
      return false;
    }
    if (FAILED(aDevice->QueryInterface(__uuidof(ID3D11Device1), getter_AddRefs(mDevice)))) {
      NS_WARNING("OpenVRSession::CreateD3DObjects failed to get a D3D11Device1");
      return false;
    }
  }
  if (!mContext) {
    mDevice->GetImmediateContext1(getter_AddRefs(mContext));
    if (!mContext) {
      NS_WARNING("OpenVRSession::CreateD3DObjects failed to get an immediate context");
      return false;
    }
  }
  if (!mDeviceContextState) {
    D3D_FEATURE_LEVEL featureLevels[] {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0
    };
    mDevice->CreateDeviceContextState(0,
                                      featureLevels,
                                      2,
                                      D3D11_SDK_VERSION,
                                      __uuidof(ID3D11Device1),
                                      nullptr,
                                      getter_AddRefs(mDeviceContextState));
  }
  if (!mDeviceContextState) {
    NS_WARNING("VRDisplayHost::CreateD3DObjects failed to get a D3D11DeviceContextState");
    return false;
  }
  return true;
}

ID3D11Device1*
VRSession::GetD3DDevice()
{
  return mDevice;
}

ID3D11DeviceContext1*
VRSession::GetD3DDeviceContext()
{
  return mContext;
}

ID3DDeviceContextState*
VRSession::GetD3DDeviceContextState()
{
  return mDeviceContextState;
}

#endif // defined(XP_WIN)
