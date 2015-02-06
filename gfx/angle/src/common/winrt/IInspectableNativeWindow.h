//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// IInspectableNativeWindow.h: Host specific implementation interface for
// managing IInspectable native window types.

#ifndef COMMON_IINSPECTABLENATIVEWINDOW_H_
#define COMMON_IINSPECTABLENATIVEWINDOW_H_

#include "common/platform.h"
#include "common/NativeWindow.h"
#include "angle_windowsstore.h"

#include <windows.ui.xaml.h>
#include <windows.ui.xaml.media.dxinterop.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;

class IInspectableNativeWindow
{
  public:
    virtual bool initialize(EGLNativeWindowType window, IPropertySet *propertySet) = 0;
    virtual HRESULT createSwapChain(ID3D11Device *device, DXGIFactory *factory, DXGI_FORMAT format, unsigned int width, unsigned int height, DXGISwapChain **swapChain) = 0;
    virtual bool registerForSizeChangeEvents() = 0;
    virtual void unregisterForSizeChangeEvents() = 0;
    virtual HRESULT scaleSwapChain(const SIZE& newSize) { return S_OK; }

    IInspectableNativeWindow() :
        mSupportsSwapChainResize(true),
        mRequiresSwapChainScaling(false),
        mClientRectChanged(false),
        mClientRect({0,0,0,0}),
        mNewClientRect({0,0,0,0})
    {
        mSizeChangedEventToken.value = 0;
    }

    virtual ~IInspectableNativeWindow(){}

    bool getClientRect(RECT *rect)
    {
        if (mClientRectChanged && mSupportsSwapChainResize)
        {
            mClientRect = mNewClientRect;
        }

        *rect = mClientRect;

        return true;
    }

    void setNewClientSize(const SIZE &newSize)
    {
        if (mSupportsSwapChainResize && !mRequiresSwapChainScaling)
        {
            mNewClientRect = { 0, 0, newSize.cx, newSize.cy };
            mClientRectChanged = true;
        }

        if (mRequiresSwapChainScaling)
        {
            scaleSwapChain(newSize);
        }
    }

protected:
    bool mSupportsSwapChainResize;
    bool mRequiresSwapChainScaling;
    RECT mClientRect;
    RECT mNewClientRect;
    bool mClientRectChanged;

    EventRegistrationToken mSizeChangedEventToken;
};

bool isCoreWindow(EGLNativeWindowType window, ComPtr<ABI::Windows::UI::Core::ICoreWindow> *coreWindow = nullptr);
bool isEGLConfiguredPropertySet(EGLNativeWindowType window, ABI::Windows::Foundation::Collections::IPropertySet **propertySet = nullptr, IInspectable **inspectable = nullptr);
HRESULT getOptionalSizePropertyValue(const ComPtr<ABI::Windows::Foundation::Collections::IMap<HSTRING, IInspectable*>>& propertyMap, const wchar_t *propertyName, SIZE *value, bool *valueExists);

#endif // COMMON_IINSPECTABLENATIVEWINDOW_H_
