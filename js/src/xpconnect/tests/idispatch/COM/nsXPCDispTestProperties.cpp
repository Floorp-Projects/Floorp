// nsXPCDispTestProperties.cpp : Implementation of CXPCIDispatchTestApp and DLL registration.

#include "stdafx.h"
#include "XPCIDispatchTest.h"
#include "nsXPCDispTestProperties.h"

const long PARAMETERIZED_PROPERTY_COUNT = 5;
/////////////////////////////////////////////////////////////////////////////
//

STDMETHODIMP nsXPCDispTestProperties::InterfaceSupportsErrorInfo(REFIID riid)
{
    static const IID* arr[] = 
    {
        &IID_nsIXPCDispTestProperties,
    };

    for (int i=0;i<sizeof(arr)/sizeof(arr[0]);i++)
    {
        if (InlineIsEqualGUID(*arr[i],riid))
            return S_OK;
    }
    return S_FALSE;
}

nsXPCDispTestProperties::nsXPCDispTestProperties() :
    mChar('a'),
    mBOOL(FALSE),
    mSCode(0),
    mDATE(0),
    mDouble(0.0),
    mFloat(0.0f),
    mLong(0),
    mShort(0),
    mParameterizedProperty(new long[PARAMETERIZED_PROPERTY_COUNT])
{
    mCURRENCY.int64 = 0;
    CComBSTR string("Initial value");
    mBSTR = string.Detach();
    for (long index = 0; index < PARAMETERIZED_PROPERTY_COUNT; ++index)
        mParameterizedProperty[index] = index + 1;
}

nsXPCDispTestProperties::~nsXPCDispTestProperties()
{
    delete [] mParameterizedProperty;
}

STDMETHODIMP nsXPCDispTestProperties::get_Short(short *pVal)
{
    *pVal = mShort;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::put_Short(short newVal)
{
    mShort = newVal;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::get_Long(long *pVal)
{
    *pVal = mLong;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::put_Long(long newVal)
{
    mLong = newVal;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::get_Float(float *pVal)
{
    *pVal = mFloat;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::put_Float(float newVal)
{
    mFloat = newVal;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::get_Double(double *pVal)
{
    *pVal = mDouble;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::put_Double(double newVal)
{
    mDouble = newVal;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::get_Currency(CURRENCY *pVal)
{
    *pVal = mCURRENCY;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::put_Currency(CURRENCY newVal)
{
    mCURRENCY = newVal;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::get_Date(DATE *pVal)
{
    *pVal = mDATE;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::put_Date(DATE newVal)
{
    mDATE = newVal;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::get_String(BSTR *pVal)
{
    *pVal = mBSTR.Copy();

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::put_String(BSTR newVal)
{
    mBSTR = newVal;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::get_DispatchPtr(IDispatch **pVal)
{
    mIDispatch.CopyTo(pVal);

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::put_DispatchPtr(IDispatch *newVal)
{
    mIDispatch = newVal;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::get_SCode(SCODE *pVal)
{
    *pVal = mSCode;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::put_SCode(SCODE newVal)
{
    mSCode = newVal;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::get_Boolean(BOOL *pVal)
{
    *pVal = mBOOL;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::put_Boolean(BOOL newVal)
{
    mBOOL = newVal;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::get_Variant(VARIANT *pVal)
{
    ::VariantCopy(pVal, &mVariant);

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::put_Variant(VARIANT newVal)
{
    mVariant = newVal;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::get_COMPtr(IUnknown **pVal)
{
    mIUnknown.CopyTo(pVal);

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::put_COMPtr(IUnknown *newVal)
{
    mIUnknown = newVal;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::get_Char(unsigned char *pVal)
{
    *pVal = mChar;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::put_Char(unsigned char newVal)
{
    mChar = newVal;

    return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::get_ParameterizedProperty(long aIndex, long *pVal)
{
    if (aIndex < 0 || aIndex >= PARAMETERIZED_PROPERTY_COUNT)
        return E_FAIL;

	*pVal = mParameterizedProperty[aIndex];

	return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::put_ParameterizedProperty(long aIndex, long newVal)
{
    if (aIndex < 0 || aIndex >= PARAMETERIZED_PROPERTY_COUNT)
        return E_FAIL;

	mParameterizedProperty[aIndex] = newVal;

	return S_OK;
}

STDMETHODIMP nsXPCDispTestProperties::get_ParameterizedPropertyCount(long *pVal)
{
	*pVal = PARAMETERIZED_PROPERTY_COUNT;

	return S_OK;
}
