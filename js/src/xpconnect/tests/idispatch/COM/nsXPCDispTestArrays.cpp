// nsXPCDispTestArrays.cpp : Implementation of CXPCIDispatchTestApp and DLL registration.

#include "stdafx.h"
#include "XPCIDispatchTest.h"
#include "nsXPCDispTestArrays.h"

unsigned int zero = 0;

/////////////////////////////////////////////////////////////////////////////
//

STDMETHODIMP nsXPCDispTestArrays::InterfaceSupportsErrorInfo(REFIID riid)
{
    static const IID* arr[] = 
    {
        &IID_nsIXPCDispTestArrays,
    };

    for (int i=0;i<sizeof(arr)/sizeof(arr[0]);i++)
    {
        if (InlineIsEqualGUID(*arr[i],riid))
            return S_OK;
    }
    return S_FALSE;
}

STDMETHODIMP nsXPCDispTestArrays::ReturnSafeArray(LPSAFEARRAY * result)
{
    if (result == NULL)
        return E_POINTER;

	*result = SafeArrayCreateVector(VT_I4, 0, 3);
    for (long index = 0; index < 3; ++index)
    {
        SafeArrayPutElement(*result, &index, &index);
    }
    return S_OK;
}

STDMETHODIMP nsXPCDispTestArrays::ReturnSafeArrayBSTR(LPSAFEARRAY * result)
{
    if (result == NULL)
        return E_POINTER;

	*result = SafeArrayCreateVector(VT_BSTR, 0, 3);
    for (long index = 0; index < 3; ++index)
    {
        _variant_t var(index);
        HRESULT hr = VariantChangeType(&var, &var, VARIANT_ALPHABOOL, VT_BSTR);

        if (FAILED(hr))
            return hr;
        SafeArrayPutElement(*result, &index, var.bstrVal);
    }
    return S_OK;
}

STDMETHODIMP nsXPCDispTestArrays::ReturnSafeArrayIDispatch(LPSAFEARRAY * result)
{
    if (result == NULL)
        return E_POINTER;

	*result = SafeArrayCreateVector(VT_DISPATCH, 0, 3);
    for (long index = 0; index < 3; ++index)
    {
        CComPtr<nsIXPCDispSimple> ptr;
        ptr.CoCreateInstance(CLSID_nsXPCDispSimple);
        SafeArrayPutElement(*result, &index, ptr.p);
    }
    return S_OK;
}

#define RETURN_IF_FAIL(x) hr = x; if (FAILED(hr)) return hr;

STDMETHODIMP nsXPCDispTestArrays::TakesSafeArray(LPSAFEARRAY array)
{
    long lbound;
    long ubound;
    HRESULT hr;

    RETURN_IF_FAIL(SafeArrayGetLBound(array, 1, &lbound));
    if (lbound != 0)
        return E_FAIL;
    RETURN_IF_FAIL(SafeArrayGetUBound(array, 1, &ubound));
    if (ubound != 3)
        return E_FAIL;
    for (long index = lbound; index <= ubound; ++index)
    {
        _variant_t value;
        RETURN_IF_FAIL(SafeArrayGetElement(array, &index, &value));
        if (value != _variant_t(index))
            return E_FAIL;
    }
    return S_OK;
}
STDMETHODIMP nsXPCDispTestArrays::TakesSafeArrayBSTR(LPSAFEARRAY array)
{
    long lbound;
    long ubound;
    HRESULT hr;

    RETURN_IF_FAIL(SafeArrayGetLBound(array, 1, &lbound));
    if (lbound != 0)
        return E_FAIL;
    RETURN_IF_FAIL(SafeArrayGetUBound(array, 1, &ubound));
    if (ubound != 3)
        return E_FAIL;
    for (long index = lbound; index <= ubound; ++index)
    {
        _variant_t value;
        RETURN_IF_FAIL(SafeArrayGetElement(array, &index, &value));
        _variant_t test(index);
        if (_bstr_t(value) != _bstr_t(test))
            return E_FAIL;
    }
    return S_OK;
}
STDMETHODIMP nsXPCDispTestArrays::TakesSafeArrayIDispatch(LPSAFEARRAY array)
{
    long lbound;
    long ubound;
    HRESULT hr;

    RETURN_IF_FAIL(SafeArrayGetLBound(array, 0, &lbound));
    if (lbound != 0)
        return E_FAIL;
    RETURN_IF_FAIL(SafeArrayGetUBound(array, 0, &ubound));
    if (ubound != 3)
        return E_FAIL;
    for (long index = lbound; index <= ubound; ++index)
    {
        _variant_t value;
        RETURN_IF_FAIL(SafeArrayGetElement(array, &index, &value));
        // We need to do more here, but this is good enough for now
        if (!value.pdispVal)
            return E_FAIL;
    }
    return S_OK;
}
STDMETHODIMP nsXPCDispTestArrays::InOutSafeArray(LPSAFEARRAY * array)
{
    if (array == NULL)
        return E_POINTER;
    long lbound;
    long ubound;
    HRESULT hr;

    RETURN_IF_FAIL(SafeArrayGetLBound(*array, 0, &lbound));
    if (lbound != 0)
        return E_FAIL;
    RETURN_IF_FAIL(SafeArrayGetUBound(*array, 0, &ubound));
    if (ubound != 3)
        return E_FAIL;
	LPSAFEARRAY newArray = SafeArrayCreateVector(VT_I4, lbound, ubound);
    for (long index = lbound; index <= ubound; ++index)
    {
        long value;
        RETURN_IF_FAIL(SafeArrayGetElement(*array, &index, &value));
        if (value != index)
            return E_FAIL;
        value += 42;
        RETURN_IF_FAIL(SafeArrayPutElement(newArray, &index, &value));
    }
    SafeArrayDestroy(*array);
    *array = newArray;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestArrays::InOutSafeArrayBSTR(LPSAFEARRAY * array)
{
    if (array == NULL)
        return E_POINTER;
        
    long lbound;
    long ubound;
    HRESULT hr;

    RETURN_IF_FAIL(SafeArrayGetLBound(*array, 0, &lbound));
    if (lbound != 0)
        return E_FAIL;
    RETURN_IF_FAIL(SafeArrayGetUBound(*array, 0, &ubound));
    if (ubound != 3)
        return E_FAIL;
	LPSAFEARRAY newArray = SafeArrayCreateVector(VT_BSTR, lbound, ubound);
    for (long index = lbound; index <= ubound; ++index)
    {
        BSTR value;
        RETURN_IF_FAIL(SafeArrayGetElement(*array, &index, &value));
        _bstr_t newValue(value, TRUE);
        newValue += L"Appended";
        RETURN_IF_FAIL(SafeArrayPutElement(newArray, &index, newValue.copy()));
    }
    SafeArrayDestroy(*array);
    *array = newArray;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestArrays::InOutSafeArrayIDispatch(LPSAFEARRAY * array)
{
    if (array == NULL)
        return E_POINTER;
        
    long lbound;
    long ubound;
    HRESULT hr;

    RETURN_IF_FAIL(SafeArrayGetLBound(*array, 0, &lbound));
    if (lbound != 0)
        return E_FAIL;
    RETURN_IF_FAIL(SafeArrayGetUBound(*array, 0, &ubound));
    if (ubound != 3)
        return E_FAIL;
	LPSAFEARRAY newArray = SafeArrayCreateVector(VT_DISPATCH, lbound, ubound);
    for (long index = lbound; index <= ubound; ++index)
    {
        IDispatch* value;
        RETURN_IF_FAIL(SafeArrayGetElement(*array, &index, &value));
        RETURN_IF_FAIL(SafeArrayPutElement(newArray, &index, &value));
    }
    SafeArrayDestroy(*array);
    *array = newArray;
    return S_OK;
}
