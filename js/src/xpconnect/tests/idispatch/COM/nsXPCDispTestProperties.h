// nsXPCDispTestProperties.h: Definition of the nsXPCDispTestProperties class
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NSXPCDISPTESTPROPERTIES_H__9E10C7AC_36AF_4A3A_91C7_2CFB9EB166A5__INCLUDED_)
#define AFX_NSXPCDISPTESTPROPERTIES_H__9E10C7AC_36AF_4A3A_91C7_2CFB9EB166A5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// nsXPCDispTestProperties

class nsXPCDispTestProperties : 
    public IDispatchImpl<nsIXPCDispTestProperties, &IID_nsIXPCDispTestProperties, &LIBID_IDispatchTestLib>, 
    public ISupportErrorInfo,
    public CComObjectRoot,
    public CComCoClass<nsXPCDispTestProperties,&CLSID_nsXPCDispTestProperties>
{
public:
    nsXPCDispTestProperties();
    virtual ~nsXPCDispTestProperties();
BEGIN_CATEGORY_MAP(nsXPCDispTestProperties)
    IMPLEMENTED_CATEGORY(CATID_SafeForScripting)
END_CATEGORY_MAP()
BEGIN_COM_MAP(nsXPCDispTestProperties)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(nsIXPCDispTestProperties)
    COM_INTERFACE_ENTRY(ISupportErrorInfo)
END_COM_MAP()
DECLARE_NOT_AGGREGATABLE(nsXPCDispTestProperties) 

DECLARE_REGISTRY_RESOURCEID(IDR_nsXPCDispTestProperties)
// ISupportsErrorInfo
    STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid);

// nsIXPCDispTestProperties
public:
	STDMETHOD(get_ParameterizedPropertyCount)(/*[out, retval]*/ long *pVal);
	STDMETHOD(get_ParameterizedProperty)(/*[in]*/long aIndex, /*[out, retval]*/ long *pVal);
	STDMETHOD(put_ParameterizedProperty)(/*[in]*/long aIndex, /*[in]*/ long newVal);
    STDMETHOD(get_Char)(/*[out, retval]*/ unsigned char *pVal);
    STDMETHOD(put_Char)(/*[in]*/ unsigned char newVal);
    STDMETHOD(get_COMPtr)(/*[out, retval]*/ IUnknown* *pVal);
    STDMETHOD(put_COMPtr)(/*[in]*/ IUnknown* newVal);
    STDMETHOD(get_Variant)(/*[out, retval]*/ VARIANT *pVal);
    STDMETHOD(put_Variant)(/*[in]*/ VARIANT newVal);
    STDMETHOD(get_Boolean)(/*[out, retval]*/ BOOL *pVal);
    STDMETHOD(put_Boolean)(/*[in]*/ BOOL newVal);
    STDMETHOD(get_SCode)(/*[out, retval]*/ SCODE *pVal);
    STDMETHOD(put_SCode)(/*[in]*/ SCODE newVal);
    STDMETHOD(get_DispatchPtr)(/*[out, retval]*/ IDispatch* *pVal);
    STDMETHOD(put_DispatchPtr)(/*[in]*/ IDispatch* newVal);
    STDMETHOD(get_String)(/*[out, retval]*/ BSTR *pVal);
    STDMETHOD(put_String)(/*[in]*/ BSTR newVal);
    STDMETHOD(get_Date)(/*[out, retval]*/ DATE *pVal);
    STDMETHOD(put_Date)(/*[in]*/ DATE newVal);
    STDMETHOD(get_Currency)(/*[out, retval]*/ CURRENCY *pVal);
    STDMETHOD(put_Currency)(/*[in]*/ CURRENCY newVal);
    STDMETHOD(get_Double)(/*[out, retval]*/ double *pVal);
    STDMETHOD(put_Double)(/*[in]*/ double newVal);
    STDMETHOD(get_Float)(/*[out, retval]*/ float *pVal);
    STDMETHOD(put_Float)(/*[in]*/ float newVal);
    STDMETHOD(get_Long)(/*[out, retval]*/ long *pVal);
    STDMETHOD(put_Long)(/*[in]*/ long newVal);
    STDMETHOD(get_Short)(/*[out, retval]*/ short *pVal);
    STDMETHOD(put_Short)(/*[in]*/ short newVal);
private:
    unsigned char       mChar;
    CComPtr<IUnknown>   mIUnknown;
    CComVariant         mVariant;
    BOOL                mBOOL;
    SCODE               mSCode;
    CComPtr<IDispatch>  mIDispatch;
    CComBSTR            mBSTR;
    DATE                mDATE;
    CURRENCY            mCURRENCY;
    double              mDouble;
    float               mFloat;
    long                mLong;
    short               mShort;
    long *              mParameterizedProperty;
};

#endif // !defined(AFX_NSXPCDISPTESTPROPERTIES_H__9E10C7AC_36AF_4A3A_91C7_2CFB9EB166A5__INCLUDED_)
