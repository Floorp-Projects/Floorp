// nsXPCDispTestMethods.cpp : Implementation of CIDispatchTestApp and DLL registration.

#include "stdafx.h"
#include "XPCIDispatchTest.h"
#include "nsXPCDispTestMethods.h"
#include "XPCDispUtilities.h"
#include "nsXPCDispSimple.h"

/////////////////////////////////////////////////////////////////////////////
//

STDMETHODIMP nsXPCDispTestMethods::InterfaceSupportsErrorInfo(REFIID riid)
{
    static const IID* arr[] = 
    {
        &IID_nsIXPCDispTestMethods,
    };

    for (int i=0;i<sizeof(arr)/sizeof(arr[0]);i++)
    {
        if (InlineIsEqualGUID(*arr[i],riid))
            return S_OK;
    }
    return S_FALSE;
}

STDMETHODIMP nsXPCDispTestMethods::NoParameters()
{
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::ReturnBSTR(BSTR * result)
{
    if (result == NULL)
        return E_POINTER;
        
    CComBSTR x("Boo");
    *result = x.Detach();
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::ReturnI4(INT * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = 99999;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::ReturnUI1(BYTE * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = 99;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::ReturnI2(SHORT * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = 9999;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::ReturnR4(FLOAT * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = 99999.1f;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::ReturnR8(DOUBLE * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = 99999999999.99;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::ReturnBool(VARIANT_BOOL * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = VARIANT_TRUE;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::ReturnIDispatch(IDispatch * * result)
{
    if (result == NULL)
        return E_POINTER;
    return nsXPCDispSimple::CreateInstance(result);
}
STDMETHODIMP nsXPCDispTestMethods::ReturnError(SCODE * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = E_FAIL;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::ReturnDate(DATE * result)
{
    if (result == NULL)
        return E_POINTER;
    CComBSTR dateStr(L"5/2/02");
    return VarDateFromStr(dateStr, LOCALE_SYSTEM_DEFAULT,
                          LOCALE_NOUSEROVERRIDE, result);
}
STDMETHODIMP nsXPCDispTestMethods::ReturnIUnknown(IUnknown * * result)
{
    if (result == NULL)
        return E_POINTER;
        
    return XPCCreateInstance<IUnknown>(CLSID_nsXPCDispTestNoIDispatch, IID_IUnknown, result);
}
STDMETHODIMP nsXPCDispTestMethods::ReturnI1(unsigned char * result)
{
    if (result == NULL)
        return E_POINTER;
        
    *result = 120;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::ReturnUI2(USHORT * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = 9999;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::ReturnUI4(ULONG * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = 3000000000;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::ReturnInt(INT * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = -999999;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::ReturnUInt(UINT * result)
{
    if (result == NULL)
        return E_POINTER;
        
    *result = 3000000000;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::TakesBSTR(BSTR result)
{
    CComBSTR str(result);
    static CComBSTR test(L"TakesBSTR");
    return str == test ? S_OK: E_FAIL;
}
STDMETHODIMP nsXPCDispTestMethods::TakesI4(INT result)
{
    return result == 999999 ? S_OK : E_FAIL;
}
STDMETHODIMP nsXPCDispTestMethods::TakesUI1(BYTE result)
{
    return result == 42 ? S_OK : E_FAIL;
}
STDMETHODIMP nsXPCDispTestMethods::TakesI2(SHORT result)
{
    return result == 32000 ? S_OK : E_FAIL;
}
STDMETHODIMP nsXPCDispTestMethods::TakesR4(FLOAT result)
{
    // Hopefully we won't run into any precision/rounding issues
    return result == 99999.99f ? S_OK : E_FAIL;
}
STDMETHODIMP nsXPCDispTestMethods::TakesR8(DOUBLE result)
{
    // Hopefully we won't run into any precision/rounding issues
    return result == 999999999.99 ? S_OK : E_FAIL;
}
STDMETHODIMP nsXPCDispTestMethods::TakesBool(VARIANT_BOOL result)
{
    return result ? S_OK : E_FAIL;
}

inline
HRESULT GetProperty(IDispatch *pDisp, const CComBSTR & name, CComVariant& output)
{
    DISPID dispid = GetIDsOfNames(pDisp, name);
    DISPPARAMS dispParams;
    dispParams.cArgs = 0;
    dispParams.cNamedArgs = 0;
    dispParams.rgdispidNamedArgs = 0;
    dispParams.rgvarg = 0;
    return pDisp->Invoke(dispid, IID_NULL, LOCALE_SYSTEM_DEFAULT,DISPATCH_PROPERTYGET, &dispParams, &output, 0, 0);
}
    
inline
HRESULT PutProperty(IDispatch *pDisp, const CComBSTR & name, const CComVariant& input)
{
    DISPPARAMS dispParams;
    DISPID did = DISPID_PROPERTYPUT;
    dispParams.cArgs = 1;
    CComVariant var(input);
    dispParams.rgvarg = &var;
    dispParams.cNamedArgs = 1;
    dispParams.rgdispidNamedArgs = &did;
    CComVariant result;
    DISPID dispID = GetIDsOfNames(pDisp, name);
    return pDisp->Invoke(dispID, IID_NULL, LOCALE_SYSTEM_DEFAULT,
                               DISPATCH_PROPERTYPUT, &dispParams, &result,
                               0, 0);
}

HRESULT VerifynsXPCDispSimple(IDispatch * result)
{
    CComVariant property;
    HRESULT hResult = GetProperty(result, L"Number", property);
    CComVariant test((long)5);
    if (FAILED(hResult))
        return hResult;
    if (property != test)
        return E_FAIL;
    return PutProperty(result, L"Number", 76);
}

STDMETHODIMP nsXPCDispTestMethods::TakesIDispatch(IDispatch * result)
{
    return VerifynsXPCDispSimple(result);
}
STDMETHODIMP nsXPCDispTestMethods::TakesError(SCODE result)
{
    return result == E_FAIL ? S_OK : E_FAIL;
}
STDMETHODIMP nsXPCDispTestMethods::TakesDate(DATE result)
{
    CComBSTR dateStr(L"5/2/02");
    DATE myDate;
    HRESULT hResult = VarDateFromStr(dateStr, LOCALE_SYSTEM_DEFAULT,
                                    LOCALE_NOUSEROVERRIDE, &myDate);
    if (SUCCEEDED(hResult))
        return myDate == result ? S_OK : E_FAIL;
    else
        return hResult;
}
STDMETHODIMP nsXPCDispTestMethods::TakesIUnknown(IUnknown * result)
{
    CComPtr<IUnknown> ptr(result);
    ULONG before = result->AddRef();
    ULONG after = result->Release();
    CComQIPtr<nsIXPCDispTestNoIDispatch> noIDispatch(ptr);
    return before - 1 == after ? S_OK : E_FAIL;
}
STDMETHODIMP nsXPCDispTestMethods::TakesI1(unsigned char result)
{
    return result == 42 ? S_OK : E_FAIL;
}
STDMETHODIMP nsXPCDispTestMethods::TakesUI2(USHORT result)
{
    return result == 50000 ? S_OK : E_FAIL;
}
STDMETHODIMP nsXPCDispTestMethods::TakesUI4(ULONG result)
{
    return result == 0xF0000000 ? S_OK : E_FAIL;
}
STDMETHODIMP nsXPCDispTestMethods::TakesInt(INT result)
{
    return result == -10000000 ? S_OK : E_FAIL;
}
STDMETHODIMP nsXPCDispTestMethods::TakesUInt(UINT result)
{
    return result == 0xE0000000 ? S_OK : E_FAIL;
}
STDMETHODIMP nsXPCDispTestMethods::OutputsBSTR(BSTR * result)
{
    if (result == NULL)
        return E_POINTER;
        
    CComBSTR x("Boo");
    *result = x.Detach();
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::OutputsI4(LONG * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = 99999;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::OutputsUI1(BYTE * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = 99;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::OutputsI2(SHORT * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = 9999;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::OutputsR4(FLOAT * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = 999999.1f;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::OutputsR8(DOUBLE * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = 99999999999.99;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::OutputsBool(VARIANT_BOOL * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = VARIANT_TRUE;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::OutputsIDispatch(IDispatch * * result)
{
    if (result == NULL)
        return E_POINTER;
    return nsXPCDispSimple::CreateInstance(result);
}
STDMETHODIMP nsXPCDispTestMethods::OutputsError(SCODE * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = E_FAIL;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::OutputsDate(DATE * result)
{
    if (result == NULL)
        return E_POINTER;
    CComBSTR dateStr(L"5/2/02");
    return VarDateFromStr(dateStr, LOCALE_SYSTEM_DEFAULT,
                          LOCALE_NOUSEROVERRIDE, result);
}
STDMETHODIMP nsXPCDispTestMethods::OutputsIUnknown(IUnknown * * result)
{
    if (result == NULL)
        return E_POINTER;
        
    return XPCCreateInstance<IUnknown>(CLSID_nsXPCDispTestNoIDispatch, IID_IUnknown, result);
}
STDMETHODIMP nsXPCDispTestMethods::OutputsI1(unsigned char * result)
{
    if (result == NULL)
        return E_POINTER;
        
    *result = L'x';
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::OutputsUI2(USHORT * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = 9999;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::OutputsUI4(ULONG * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = 3000000000;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::InOutsBSTR(BSTR * result)
{
    if (result == NULL)
        return E_POINTER;
    CComBSTR str(*result);
    str += L"Appended";
    SysFreeString(*result);
    *result = str.Detach();
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::InOutsI4(LONG * result)
{
    if (result == NULL)
        return E_POINTER;
    *result -= 4000000;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::InOutsUI1(BYTE * result)
{
    if (result == NULL)
        return E_POINTER;
    *result -= 42;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::InOutsI2(SHORT * result)
{
    if (result == NULL)
        return E_POINTER;
    *result += 10000;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::InOutsR4(FLOAT * result)
{
    if (result == NULL)
        return E_POINTER;
    *result += 5.05f;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::InOutsR8(DOUBLE * result)
{
    if (result == NULL)
        return E_POINTER;
    *result += 50000000.00000005;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::InOutsBool(VARIANT_BOOL * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = !*result;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::InOutsIDispatch(IDispatch * * result)
{
    if (result == NULL)
        return E_POINTER;
    CComPtr<nsIXPCDispSimple> ptr;
    ptr.CoCreateInstance(CLSID_nsXPCDispSimple);
    CComPtr<IDispatch> incoming(*result);
    CComVariant value;
    HRESULT hResult = GetProperty(incoming, L"Number", value);
    if (FAILED(hResult))
        return hResult;
    if (value.lVal != 10)
        return E_FAIL;
    hResult = ptr->put_Number(value.lVal + 5);
    if (FAILED(hResult))
        return hResult;

    *result = ptr.Detach();
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::InOutsError(SCODE * result)
{
    if (result == NULL)
        return E_POINTER;
    *result += 1;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::InOutsDate(DATE * result)
{
    if (result == NULL)
        return E_POINTER;
    ULONG days;
    HRESULT hResult = VarUI4FromDate(*result, &days);
    if (FAILED(hResult))
        return hResult;

    return VarDateFromUI4(days + 1, result);
}
STDMETHODIMP nsXPCDispTestMethods::InOutsIUnknown(IUnknown * * result)
{
    if (result == NULL)
        return E_POINTER;
        
    CComPtr<IUnknown> ptr(*result);
    ULONG before = (*result)->AddRef();
    ULONG after = (*result)->Release();
    if (before - 1 != after)
        return E_FAIL;
    return nsXPCDispSimple::CreateInstance(result);
}
STDMETHODIMP nsXPCDispTestMethods::InOutsI1(unsigned char * result)
{
    if (result == NULL)
        return E_POINTER;
    ++*result;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::InOutsUI2(USHORT * result)
{
    if (result == NULL)
        return E_POINTER;
    *result += 42;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::InOutsUI4(ULONG * result)
{
    if (result == NULL)
        return E_POINTER;
    *result -= 42;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::OneParameterWithReturn(LONG input, 
                                                          LONG * result)
{
    if (result == NULL)
        return E_POINTER;
    *result = input + 42;
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::StringInputAndReturn(BSTR str, 
                                                        BSTR * result)
{
    if (result == NULL)
        return E_POINTER;
    CComBSTR input(str);
    input += L"Appended";
    *result = input.Detach();
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::IDispatchInputAndReturn(IDispatch * input, IDispatch * * result)
{
    if (result == NULL)
        return E_POINTER;
    HRESULT hResult = VerifynsXPCDispSimple(input);

    hResult = XPCCreateInstance<IDispatch>(CLSID_nsXPCDispSimple, IID_IDispatch, result);
    if (FAILED(hResult))
        return hResult;
    CComVariant variant;
    hResult = GetProperty(input, L"Number", variant);
    if (FAILED(hResult))
        return hResult;
    return PutProperty(*result, L"Number", variant.lVal + 5);
}
STDMETHODIMP nsXPCDispTestMethods::TwoParameters(LONG one, LONG two)
{
    return one + 1 == two ? S_OK : E_FAIL;
}
STDMETHODIMP nsXPCDispTestMethods::TwelveInParameters(LONG one, LONG two,
                                                      LONG three, LONG four,
                                                      LONG five, LONG six,
                                                      LONG seven, LONG eight,
                                                      LONG nine, LONG ten,
                                                      LONG eleven, LONG twelve)
{
    return one + two + three + four + five + six + seven + eight + nine + 
        ten + eleven + twelve == 78 ? S_OK : E_FAIL;
}
STDMETHODIMP nsXPCDispTestMethods::TwelveOutParameters(LONG * one, LONG * two,
                                                       LONG * three,
                                                       LONG * four,
                                                       LONG * five, LONG * six,
                                                       LONG * seven,
                                                       LONG * eight,
                                                       LONG * nine, LONG * ten,
                                                       LONG * eleven,
                                                       LONG * twelve)
{
    if (one == 0 || two == 0 || three == 0 || four == 0 || 
        five == 0 || six == 0 || seven == 0 || eight == 0 ||
        nine == 0 || ten == 0 || eleven == 0 || twelve == 0)
        return E_POINTER;
    
    *one = 1;
    *two = 2;
    *three = 3;
    *four = 4;
    *five = 5;
    *six = 6;
    *seven = 7;
    *eight = 8;
    *nine = 9;
    *ten = 10;
    *eleven = 11;
    *twelve = 12;
    return S_OK;
}

inline
boolean Equals(BSTR left, const char * str)
{
    return CComBSTR(left) == str;
}
#define TESTPARAM(val) Equals(val, #val)

STDMETHODIMP nsXPCDispTestMethods::TwelveStrings(BSTR one, BSTR two, BSTR three, BSTR four, BSTR five, BSTR six, BSTR seven, BSTR eight, BSTR nine, BSTR ten, BSTR eleven, BSTR twelve)
{
    return TESTPARAM(one) && TESTPARAM(two) && TESTPARAM(three) && 
        TESTPARAM(four) && TESTPARAM(five) && TESTPARAM(six) && 
        TESTPARAM(seven) && TESTPARAM(eight) && TESTPARAM(nine) && 
        TESTPARAM(ten) && TESTPARAM(eleven) && TESTPARAM(twelve) ? 
            S_OK : E_FAIL;
}

#define ASSIGNPARAM(val) \
    if (val == 0) \
        return E_POINTER; \
    *val = CComBSTR(#val).Detach()
STDMETHODIMP nsXPCDispTestMethods::TwelveOutStrings(BSTR * one, BSTR * two, BSTR * three, BSTR * four, BSTR * five, BSTR * six, BSTR * seven, BSTR * eight, BSTR * nine, BSTR * ten, BSTR * eleven, BSTR * twelve)
{
    ASSIGNPARAM(one);
    ASSIGNPARAM(two);
    ASSIGNPARAM(three);
    ASSIGNPARAM(four);
    ASSIGNPARAM(five);
    ASSIGNPARAM(six);
    ASSIGNPARAM(seven);
    ASSIGNPARAM(eight);
    ASSIGNPARAM(nine);
    ASSIGNPARAM(ten);
    ASSIGNPARAM(eleven);
    ASSIGNPARAM(twelve);
    return S_OK;
}
#define VERIFYSIMPLE(param) \
    hResult = VerifynsXPCDispSimple(param); \
    if (FAILED(hResult)) \
        return hResult
STDMETHODIMP nsXPCDispTestMethods::TwelveIDispatch(IDispatch * one,
                                                   IDispatch * two,
                                                   IDispatch * three,
                                                   IDispatch * four,
                                                   IDispatch * five,
                                                   IDispatch * six,
                                                   IDispatch * seven,
                                                   IDispatch * eight,
                                                   IDispatch * nine,
                                                   IDispatch * ten,
                                                   IDispatch * eleven,
                                                   IDispatch * twelve)
{
    HRESULT hResult;
    VERIFYSIMPLE(one);
    VERIFYSIMPLE(two);
    VERIFYSIMPLE(three);
    VERIFYSIMPLE(four);
    VERIFYSIMPLE(five);
    VERIFYSIMPLE(six);
    VERIFYSIMPLE(seven);
    VERIFYSIMPLE(eight);
    VERIFYSIMPLE(nine);
    VERIFYSIMPLE(ten);
    VERIFYSIMPLE(eleven);
    VERIFYSIMPLE(twelve);
    return S_OK;
}

#define ASSIGNSIMPLE(param) \
    if (param == 0) \
        return E_POINTER; \
    hResult = nsXPCDispSimple::CreateInstance(param); \
    if (FAILED(hResult)) \
        return hResult;  \
    
STDMETHODIMP nsXPCDispTestMethods::TwelveOutIDispatch(IDispatch * * one,
                                                      IDispatch * * two,
                                                      IDispatch * * three,
                                                      IDispatch * * four,
                                                      IDispatch * * five,
                                                      IDispatch * * six,
                                                      IDispatch * * seven,
                                                      IDispatch * * eight,
                                                      IDispatch * * nine,
                                                      IDispatch * * ten,
                                                      IDispatch * * eleven,
                                                      IDispatch * * twelve){
    HRESULT hResult;
    ASSIGNSIMPLE(one);
    ASSIGNSIMPLE(two);
    ASSIGNSIMPLE(three);
    ASSIGNSIMPLE(four);
    ASSIGNSIMPLE(five);
    ASSIGNSIMPLE(six);
    ASSIGNSIMPLE(seven);
    ASSIGNSIMPLE(eight);
    ASSIGNSIMPLE(nine);
    ASSIGNSIMPLE(ten);
    ASSIGNSIMPLE(eleven);
    ASSIGNSIMPLE(twelve);
    return S_OK;
}
STDMETHODIMP nsXPCDispTestMethods::CreateError()
{
    CComBSTR someText(L"CreateError Test");
    ICreateErrorInfo * pCreateError;
    IErrorInfo * pError;
    HRESULT result = CreateErrorInfo(&pCreateError);
    if (FAILED(result))
        return E_NOTIMPL;
    result = pCreateError->QueryInterface(&pError);
    if (FAILED(result))
        return E_NOTIMPL;
    result = pCreateError->SetDescription(someText);
    if (FAILED(result))
        return E_NOTIMPL;
    result = pCreateError->SetGUID(IID_nsIXPCDispTestMethods);
    if (FAILED(result))
        return E_NOTIMPL;
    CComBSTR source(L"XPCIDispatchTest.nsXPCDispTestMethods.1");
    result = pCreateError->SetSource(source);
    if (FAILED(result))
        return E_NOTIMPL;
    result = SetErrorInfo(0, pError);
    if (FAILED(result))
        return E_NOTIMPL;
    pError->Release();
    pCreateError->Release();
    return E_FAIL;
}
