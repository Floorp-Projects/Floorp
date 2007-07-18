// nsXPCDispTestWrappedJS.cpp : Implementation of CXPCIDispatchTestApp and DLL registration.

#include "stdafx.h"
#include "XPCIDispatchTest.h"
#include "nsXPCDispTestWrappedJS.h"
#include <string>
#include <sstream>

/////////////////////////////////////////////////////////////////////////////
//

STDMETHODIMP nsXPCDispTestWrappedJS::InterfaceSupportsErrorInfo(REFIID riid)
{
	static const IID* arr[] = 
	{
		&IID_nsIXPCDispTestWrappedJS,
	};

	for (int i=0;i<sizeof(arr)/sizeof(arr[0]);i++)
	{
		if (InlineIsEqualGUID(*arr[i],riid))
			return S_OK;
	}
	return S_FALSE;
}

struct TestData
{
    typedef bool (*CompFunc)(const _variant_t & left, const _variant_t & right);
    TestData(const char * name, const _variant_t & value, CompFunc cf) : 
        mName(name), mValue(value), mCompFunc(cf) {}
    const char * mName;
    _variant_t mValue;
    CompFunc mCompFunc;
    
};

bool CompareVariant(const _variant_t & left, const _variant_t & right)
{
    return left == right;
}

// These should be a template but VC++6 is brain dead in this area
#define CompareFunction(type)                                               \
bool CompareVariant##type(const _variant_t & left, const _variant_t & right)     \
{                                                                           \
    return static_cast<type>(left) == static_cast<type>(right);             \
}

CompareFunction(long);
CompareFunction(float);
CompareFunction(double);
CompareFunction(_bstr_t);

DISPID GetIDsOfNames(IDispatch * pIDispatch , char const * i_Method)
{
    DISPID dispid;
    CComBSTR method(i_Method);
    OLECHAR * pMethod = method;
    HRESULT hresult = pIDispatch->GetIDsOfNames(
        IID_NULL,
        &pMethod,
        1, 
        LOCALE_SYSTEM_DEFAULT,
        &dispid);
    if (!SUCCEEDED(hresult))
    {
        dispid = 0;
    }
    return dispid;
}

namespace
{

inline
_bstr_t ConvertVariantFromBSTR(_variant_t & variant)
{
    VARIANT temp = variant;
    if (SUCCEEDED(VariantChangeType(&temp, &temp, 0, VT_BSTR)))
    {
        variant.Attach(temp);
        return static_cast<_bstr_t>(variant);
    }
    return "**Type Conversion failed";
}

std::string DispInvoke(IDispatch * obj, const TestData & testData)
{
    // Perform a put on the property
    ITypeInfo * pTypeInfo;
    obj->GetTypeInfo(0, LOCALE_SYSTEM_DEFAULT, &pTypeInfo);
    DISPID dispID = ::GetIDsOfNames(obj, testData.mName);
    DISPPARAMS dispParams;
    dispParams.cArgs = 1;
    dispParams.rgvarg = const_cast<VARIANT*>(&reinterpret_cast<const VARIANT&>(testData.mValue));
    dispParams.cNamedArgs = 1;
    DISPID propPut = DISPID_PROPERTYPUT;
    dispParams.rgdispidNamedArgs = &propPut;
    _variant_t result1;
    HRESULT hResult = obj->Invoke(dispID, IID_NULL, LOCALE_SYSTEM_DEFAULT, DISPATCH_PROPERTYPUT, &dispParams, &result1, 0, 0);
    if (FAILED(hResult))
    {
        std::ostringstream msg;
        msg << "IDispatch::Invoke on " << testData.mName << " failed to set property with result " << hResult;
        return msg.str();
    }
    dispParams.cArgs = 1;
    dispParams.rgvarg = const_cast<VARIANT*>(&reinterpret_cast<const VARIANT&>(testData.mValue));
    dispParams.cNamedArgs = 0;
    dispParams.rgdispidNamedArgs = 0;
    _variant_t result2;
    hResult = obj->Invoke(dispID, IID_NULL, LOCALE_SYSTEM_DEFAULT, DISPATCH_PROPERTYGET, &dispParams, &result2, 0, 0);
    if (FAILED(hResult))
    {
        std::ostringstream msg;
        msg << "IDispatch::Invoke on " << testData.mName << " failed to retrieve property with result " << hResult;
        return msg.str();
    }
    if (!testData.mCompFunc(result2,testData.mValue))
    {
        std::ostringstream msg;
        VARIANT temp = testData.mValue;
        char const * const desired = SUCCEEDED(VariantChangeType(&temp, &temp, 0, VT_BSTR)) ? 0 : "**Conversion Failed";
        char const * const actual = SUCCEEDED(VariantChangeType(&result2, &result2, 0, VT_BSTR)) ? 0 : "**Conversion Failed";

        msg << testData.mName << " ["
            << (desired ? desired : static_cast<char const *>(static_cast<_bstr_t>(testData.mValue))) << "] [" 
            << (actual ? actual : static_cast<char const *>(static_cast<_bstr_t>(result2))) << "]";
        return msg.str();
    }
    return std::string();
}

}

STDMETHODIMP nsXPCDispTestWrappedJS::TestParamTypes(IDispatch *obj, BSTR *errMsg)
{
    CComPtr<IDispatch> ptr;
    CComBSTR progID("XPCIDispatchTest.nsXPCDispSimple.1");
    ptr.CoCreateInstance(progID);
    _variant_t dispatchPtr;
    dispatchPtr = static_cast<IDispatch*>(ptr);
    CURRENCY currency;
    currency.int64 = 55555;
    _variant_t date(3000.0, VT_DATE);
    _variant_t nullVariant;
    nullVariant.ChangeType(VT_NULL);
    const TestData tests[] =
    {
        TestData("Boolean", _variant_t(true), CompareVariant),
        TestData("Short", _variant_t(short(4200)), reinterpret_cast<TestData::CompFunc>(CompareVariantlong)),
        TestData("Long", _variant_t(long(-42000000)), CompareVariantlong),
        TestData("Float", _variant_t(float(4.5)), CompareVariantfloat),
        TestData("Double", _variant_t(-11111.11), CompareVariant),
//        TestData("Currency", _variant_t(currency), CompareVariantdouble),
        TestData("Date", date, CompareVariant_bstr_t),
        TestData("String", _variant_t("A String"), CompareVariant),
        TestData("DispatchPtr", dispatchPtr, CompareVariant),
//        TestData("SCode", _variant_t(long(5), VT_ERROR), CompareVariant_bstr_t),
        TestData("Variant", nullVariant, CompareVariant),
        TestData("Char", _variant_t(BYTE('x')), CompareVariant_bstr_t)
    };
    std::string errors;
    for (size_t index = 0; index < sizeof(tests) / sizeof(TestData); ++index)
    {
        std::string msg = DispInvoke(obj, tests[index]);
        if (!msg.empty())
        {
            errors += msg;
            errors += "\r\n";
        }
    }
    CComBSTR errorMsg(errors.c_str());
    *errMsg = errorMsg.Detach();
    return S_OK;
}
