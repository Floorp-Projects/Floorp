// nsXPCDispTestMethods.h: Definition of the nsXPCDispTestMethods class
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_NSXPCDISPTESTMETHODS_H__A516B1D7_1971_419C_AE35_EDFAC27D1227__INCLUDED_)
#define AFX_NSXPCDISPTESTMETHODS_H__A516B1D7_1971_419C_AE35_EDFAC27D1227__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "resource.h"       // main symbols
#include "XPCIDispatchTest.h"

/////////////////////////////////////////////////////////////////////////////
// nsXPCDispTestMethods

class nsXPCDispTestMethods : 
    public ISupportErrorInfo,
    public CComObjectRoot,
    public CComCoClass<nsXPCDispTestMethods,&CLSID_nsXPCDispTestMethods>,
    public IDispatchImpl<nsIXPCDispTestMethods, &IID_nsIXPCDispTestMethods, &LIBID_IDispatchTestLib>
{
public:
    nsXPCDispTestMethods() {}
BEGIN_CATEGORY_MAP(nsXPCDispTestMethods)
    IMPLEMENTED_CATEGORY(CATID_SafeForScripting)
END_CATEGORY_MAP()
BEGIN_COM_MAP(nsXPCDispTestMethods)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(ISupportErrorInfo)
    COM_INTERFACE_ENTRY(nsIXPCDispTestMethods)
END_COM_MAP()
DECLARE_NOT_AGGREGATABLE(nsXPCDispTestMethods) 

DECLARE_REGISTRY_RESOURCEID(IDR_nsXPCDispTestMethods)
// ISupportsErrorInfo
    STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid);

// nsIXPCDispTestMethods
public:
// nsIXPCDispTestMethod
    STDMETHOD(NoParameters)();
    STDMETHOD(ReturnBSTR)(BSTR * result);
    STDMETHOD(ReturnI4)(INT * result);
    STDMETHOD(ReturnUI1)(BYTE * result);
    STDMETHOD(ReturnI2)(SHORT * result);
    STDMETHOD(ReturnR4)(FLOAT * result);
    STDMETHOD(ReturnR8)(DOUBLE * result);
    STDMETHOD(ReturnBool)(VARIANT_BOOL * result);
    STDMETHOD(ReturnIDispatch)(IDispatch * * result);
    STDMETHOD(ReturnError)(SCODE * result);
    STDMETHOD(ReturnDate)(DATE * result);
    STDMETHOD(ReturnIUnknown)(IUnknown * * result);
    STDMETHOD(ReturnI1)(unsigned char * result);
    STDMETHOD(ReturnUI2)(USHORT * result);
    STDMETHOD(ReturnUI4)(ULONG * result);
    STDMETHOD(ReturnInt)(INT * result);
    STDMETHOD(ReturnUInt)(UINT * result);
    STDMETHOD(TakesBSTR)(BSTR result);
    STDMETHOD(TakesI4)(INT result);
    STDMETHOD(TakesUI1)(BYTE result);
    STDMETHOD(TakesI2)(SHORT result);
    STDMETHOD(TakesR4)(FLOAT result);
    STDMETHOD(TakesR8)(DOUBLE result);
    STDMETHOD(TakesBool)(VARIANT_BOOL result);
    STDMETHOD(TakesIDispatch)(IDispatch * result);
    STDMETHOD(TakesError)(SCODE result);
    STDMETHOD(TakesDate)(DATE result);
    STDMETHOD(TakesIUnknown)(IUnknown * result);
    STDMETHOD(TakesI1)(unsigned char result);
    STDMETHOD(TakesUI2)(USHORT result);
    STDMETHOD(TakesUI4)(ULONG result);
    STDMETHOD(TakesInt)(INT result);
    STDMETHOD(TakesUInt)(UINT result);
    STDMETHOD(OutputsBSTR)(BSTR * result);
    STDMETHOD(OutputsI4)(LONG * result);
    STDMETHOD(OutputsUI1)(BYTE * result);
    STDMETHOD(OutputsI2)(SHORT * result);
    STDMETHOD(OutputsR4)(FLOAT * result);
    STDMETHOD(OutputsR8)(DOUBLE * result);
    STDMETHOD(OutputsBool)(VARIANT_BOOL * result);
    STDMETHOD(OutputsIDispatch)(IDispatch * * result);
    STDMETHOD(OutputsError)(SCODE * result);
    STDMETHOD(OutputsDate)(DATE * result);
    STDMETHOD(OutputsIUnknown)(IUnknown * * result);
    STDMETHOD(OutputsI1)(unsigned char * result);
    STDMETHOD(OutputsUI2)(USHORT * result);
    STDMETHOD(OutputsUI4)(ULONG * result);
    STDMETHOD(InOutsBSTR)(BSTR * result);
    STDMETHOD(InOutsI4)(LONG * result);
    STDMETHOD(InOutsUI1)(BYTE * result);
    STDMETHOD(InOutsI2)(SHORT * result);
    STDMETHOD(InOutsR4)(FLOAT * result);
    STDMETHOD(InOutsR8)(DOUBLE * result);
    STDMETHOD(InOutsBool)(VARIANT_BOOL * result);
    STDMETHOD(InOutsIDispatch)(IDispatch * * result);
    STDMETHOD(InOutsError)(SCODE * result);
    STDMETHOD(InOutsDate)(DATE * result);
    STDMETHOD(InOutsIUnknown)(IUnknown * * result);
    STDMETHOD(InOutsI1)(unsigned char * result);
    STDMETHOD(InOutsUI2)(USHORT * result);
    STDMETHOD(InOutsUI4)(ULONG * result);
    STDMETHOD(OneParameterWithReturn)(LONG input, LONG * result);
    STDMETHOD(StringInputAndReturn)(BSTR str, BSTR * result);
    STDMETHOD(IDispatchInputAndReturn)(IDispatch * input, IDispatch** result);
    STDMETHOD(TwoParameters)(LONG one, LONG two);
    STDMETHOD(TwelveInParameters)(LONG one, LONG two, LONG three, LONG four,
                                  LONG five, LONG six, LONG seven, LONG eight,
                                  LONG nine, LONG ten, LONG eleven,
                                  LONG twelve);
    STDMETHOD(TwelveOutParameters)(LONG * one, LONG * two, LONG * three, 
                                   LONG * four, LONG * five, LONG * six,
                                   LONG * seven, LONG * eight, LONG * nine,
                                   LONG * ten, LONG * eleven, LONG * twelve);
    STDMETHOD(TwelveStrings)(BSTR one, BSTR two, BSTR three, BSTR four,
                             BSTR five, BSTR six, BSTR seven, BSTR eight,
                             BSTR nine, BSTR ten, BSTR eleven, BSTR twelve);
    STDMETHOD(TwelveOutStrings)(BSTR * one, BSTR * two, BSTR * three,
                                BSTR * four, BSTR * five, BSTR * six,
                                BSTR * seven, BSTR * eight, BSTR * nine,
                                BSTR * ten, BSTR * eleven, BSTR * twelve);
    STDMETHOD(TwelveIDispatch)(IDispatch * one, IDispatch * two,
                               IDispatch * three, IDispatch * four,
                               IDispatch * five, IDispatch * six,
                               IDispatch * seven, IDispatch * eight,
                               IDispatch * nine, IDispatch * ten,
                               IDispatch * eleven, IDispatch * twelve);
    STDMETHOD(TwelveOutIDispatch)(IDispatch * * one, IDispatch * * two,
                                  IDispatch * * three, IDispatch * * four,
                                  IDispatch * * five, IDispatch * * six,
                                  IDispatch * * seven, IDispatch * * eight,
                                  IDispatch * * nine, IDispatch * * ten,
                                  IDispatch * * eleven, IDispatch * * twelve);
    STDMETHOD(CreateError)();
};

#endif // !defined(AFX_NSXPCDISPTESTMETHODS_H__A516B1D7_1971_419C_AE35_EDFAC27D1227__INCLUDED_)
