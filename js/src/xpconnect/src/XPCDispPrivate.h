/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is the IDispatch implementation for XPConnect.
 *
 * The Initial Developer of the Original Code is
 * David Bradley.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef XPCDispPrivate_h__
#define XPCDispPrivate_h__

/**
 * \file XPCDispPrivate.h
 * \brief Contains all the private class declarations
 */

#ifndef xpcprivate_h___
#error "DispPrivate.h should not be included directly, please use XPCPrivate.h"
#endif

/**
 * Needed for CComPtr and friends
 */
#include <atlbase.h>
/**
 * Needed for _variant_t and _bstr_t
 */
#include <comdef.h>
/**
 * IObjectSafety interface and friends
 */
#include "objsafe.h"

// MS clutters the global namespace with so many macro names :-(
// I tried to keep these includes in the CPP's but it became too
// convoluted
#undef GetClassInfo
#undef GetClassName
#undef GetMessage

// We need IDispatch
#include "nsIDispatchSupport.h"

// The following are macro helpers pulled from XPCOM but tailored to COM
#define NS_DECL_IUNKNOWN                                                      \
public:                                                                       \
  STDMETHOD(QueryInterface)(REFIID aIID,                                      \
                            void** aInstancePtr);                             \
  STDMETHODIMP_(ULONG) AddRef(void);                                          \
  STDMETHODIMP_(ULONG) Release(void);                                         \
protected:                                                                    \
  ULONG mRefCnt;                                                       

#define NS_IMPL_COM_QUERY_HEAD(_class)                                        \
STDMETHODIMP _class::QueryInterface(REFIID aIID, void** aInstancePtr)         \
{                                                                             \
  NS_ASSERTION(aInstancePtr,                                                  \
               "QueryInterface requires a non-NULL destination!");            \
  if( !aInstancePtr )                                                         \
    return E_POINTER;                                                         \
  IUnknown* foundInterface;

#define NS_IMPL_COM_QUERY_BODY(_interface)                                    \
  if(IsEqualIID(aIID, __uuidof(_interface)) )                                 \
    foundInterface = NS_STATIC_CAST(_interface*, this);                       \
  else

#define NS_IMPL_COM_QUERY_TAIL_GUTS                                           \
    foundInterface = 0;                                                       \
  HRESULT status;                                                             \
  if( !foundInterface )                                                       \
    status = E_NOINTERFACE;                                                   \
  else                                                                        \
    {                                                                         \
      NS_ADDREF(foundInterface);                                              \
      status = S_OK;                                                          \
    }                                                                         \
  *aInstancePtr = foundInterface;                                             \
  return status;                                                              \
}

#define NS_COM_MAP_BEGIN(_implClass)      NS_IMPL_COM_QUERY_HEAD(_implClass)
#define NS_COM_MAP_ENTRY(_interface)      NS_IMPL_COM_QUERY_BODY(_interface)
#define NS_COM_MAP_END                    NS_IMPL_COM_QUERY_TAIL_GUTS

#define NS_COM_IMPL_ADDREF(_class)                                            \
STDMETHODIMP_(ULONG) _class::AddRef(void)                                     \
{                                                                             \
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");                   \
  NS_ASSERT_OWNINGTHREAD(_class);                                             \
  ++mRefCnt;                                                                  \
  NS_LOG_ADDREF(this, mRefCnt, #_class, sizeof(*this));                       \
  return mRefCnt;                                                             \
}

#define NS_COM_IMPL_RELEASE(_class)                                           \
STDMETHODIMP_(ULONG) _class::Release(void)                                    \
{                                                                             \
  NS_PRECONDITION(0 != mRefCnt, "dup release");                               \
  NS_ASSERT_OWNINGTHREAD(_class);                                             \
  --mRefCnt;                                                                  \
  NS_LOG_RELEASE(this, mRefCnt, #_class);                                     \
  if(mRefCnt == 0) {                                                         \
    mRefCnt = 1; /* stabilize */                                              \
    delete this;                                                              \
    return 0;                                                                 \
  }                                                                           \
  return mRefCnt;                                                             \
}

extern const nsID NSID_IDISPATCH;

/**
 * JS<>COM Conversion functions. XPCDispConvert serves more as a namespace than
 * a class. It contains the functions to convert between JS and COM data and
 * any helper functions needed
 */
class XPCDispConvert
{
public:
    /**
     * Returns the COM type for a given jsval
     * @param ccx XPConnect call context
     * @param val Value to look up the type for
     * @return the COM variant type
     */
    static
    VARTYPE JSTypeToCOMType(XPCCallContext& ccx, jsval val);

    /**
     * Converts a JSVal to a COM variant
     * @param ccx XPConnect call context
     * @param src JS Value to convert
     * @param dest COM variant to receive the converted value
     * @param err receives the error code if any of a failed conversion
     * @return True if the conversion succeeded
     */
    static
    JSBool JSToCOM(XPCCallContext& ccx, jsval src, VARIANT & dest, 
                   nsresult& err, JSBool isByRef = JS_FALSE);

    /**
     * Converts a COM variant to a jsval
     * @param ccx XPConnect call context
     * @param src COM variant to convert
     * @param dest jsval to receive the converted value
     * @param err receives the error code if any of a failed conversion
     * @return Returns true if the conversion succeeded
     */
    static
    JSBool COMToJS(XPCCallContext& ccx, const VARIANT & src, jsval & dest,
                   nsresult& err);
private:
    /**
     * Converts a JS Array to a safe array
     * @param ccx XPConnect call context
     * @param obj JSObject that is the array
     * @param var the variant to receive the array
     * @param err receives the error code if any of a failed conversion
     * @return True if the conversion succeeded
     */
    static
    JSBool JSArrayToCOMArray(XPCCallContext& ccx, JSObject *obj, VARIANT & var,
                          nsresult& err);
    /**
     * Converts a COM Array to a JS Array
     * @param ccx XPConnect call context
     * @param src the variant holding the array
     * @param dest the jsval to receive the array
     * @param err receives the error code if any of a failed conversion
     * @return True if the conversion succeeded
     */
    static
    JSBool COMArrayToJSArray(XPCCallContext& ccx, const VARIANT & src,
                             jsval & dest, nsresult& err);
};

/**
 * JS callback function that is called when a function is invoked
 * @param cx the JS context the function is invoked with
 * @param obj the object the function is invoked on
 * @param argc the number of parameters passed to the function
 * @param argv the array of parameters passed to the function
 * @param vp holds the result of the function
 * @return true if the function completes without error
 */
JSBool JS_DLL_CALLBACK
XPC_IDispatch_CallMethod(JSContext *cx, JSObject *obj, uintN argc,
                         jsval *argv, jsval *vp);
/**
 * JS callback function that is called when a getter/setter is invoked
 * @param cx the JS context the function is invoked with
 * @param obj the object the function is invoked on
 * @param argc the number of parameters passed to the function
 * @param argv the array of parameters passed to the function
 * @param vp holds the result of the function
 * @return true if the function completes without error
 */
JSBool JS_DLL_CALLBACK
XPC_IDispatch_GetterSetter(JSContext *cx, JSObject *obj, uintN argc, 
                           jsval *argv, jsval *vp);

/**
 * This class holds an array of names. This is only used in wrapped
 * JS objects, so the dispid's are not arbitrary. They begin with 1
 * and so dispid - 1 is the index into the array mNames
 */
class XPCDispNameArray
{
public:
    /**
     * Intializes the array to zero elements
     */
    XPCDispNameArray();
    /**
     * Cleans up the mNames array
     */
    ~XPCDispNameArray();
    /**
     * Sets the size of the array
     * @param size the new size of the array
     */
    void SetSize(PRUint32 size);
    /**
     * Returns the current size of the array
     * @return the current size of the array
     */
    PRUint32 GetSize() const;
    /**
     * Assigns a name/string to a given element. This function will not
     * expand the array and performs bounds checking in debug builds.
     * @param dispid IDispatch id for the name
     * @param name the name to assign
     */
    void SetName(DISPID dispid, nsAString const & name);
    /**
     * Retrieves a name/string for a dispid. This function
     * performs bounds checking in debug builds
     * @param dispid dispatch ID of the name to retrieve
     */
    const nsAString&  GetName(DISPID dispid) const;
    /**
     * Performs a search of the array for the target returning the
     * the id for the name
     * @param target the name to find
     */
    DISPID Find(const nsAString &target) const;
private:
    /**
     * This is used by Find when it doesn't find something
     * @see XPCDispNameArray::Find
     */
    static const nsString sEmpty;
    PRUint32 mCount;
    nsString* mNames;
};

/**
 * This class represents an array of JSID's.
 * It takes care of marking the ID's during GC
 */
class XPCDispIDArray
{
public:
    /**
     * Initializes the array from the JSIdArray passed in
     * @param ccx XPConnect call context
     * @param array a JS array of ID's
     */
    XPCDispIDArray(XPCCallContext& ccx, JSIdArray* array);
    /**
     * Returns the length of the array
     * @return length of the array
     */
    PRUint32 Length() const;
    /**
     * Returns an ID within the array
     * @param cx a JS context
     * @param index index into the array
     * @return the ID as a jsval
     */
    jsval Item(JSContext* cx, PRUint32 index) const;

    /**
     * Called to mark the ID's during GC
     */
    void Mark();
    /**
     * Called to unmark the ID's after GC has been done
     */
    void Unmark();
    /**
     * Tests whether the ID is marked
     */
    JSBool IsMarked() const;

    /**
     * NOP. This is just here to make the AutoMarkingPtr code compile.
     */
    inline void MarkBeforeJSFinalize(JSContext*);
private:
    JSBool mMarked;
    nsVoidArray mIDArray;
};

/**
 * Implements ITypeInfo interface for JSObjects
 */
class XPCDispTypeInfo : public ITypeInfo
{
    NS_DECL_IUNKNOWN
public:
    /**
     * Manages an array of FUNCDESC structs
     */
    class FuncDescArray
    {
    public:
        /**
         * Initializes the function description array
         */
        FuncDescArray(XPCCallContext& ccx, JSObject* obj, 
                      const XPCDispIDArray& array, XPCDispNameArray & names);
        /**
         * cleans up the function description array
         */
        ~FuncDescArray();
        /**
         * Retrieves a function description from the array.
         * @param index index into the array
         * @return pointer to the function description
         */
        FUNCDESC* Get(PRUint32 index);
        /**
         * Releases a function description object. Currently there is nothing
         * to do as ownership is maintained by the object and the objects 
         * returned never live longer than the FuncDescArray instance they
         * were created on
         */
        void Release(FUNCDESC *);
        /**
         * Returns the length of the array
         * @return the length of the array
         */
        PRUint32 Length() const;
    private:
        nsVoidArray      mArray;
        /**
         * Initializes a function description object
         * @param ccx XPConnect context
         * @param obj Array to used for the initialization
         * @param propInfo property information for the element/function
         */
        PRBool BuildFuncDesc(XPCCallContext& ccx, JSObject* obj,
                           XPCDispJSPropertyInfo & propInfo);
    };
    /**
     * Creates an instance of XPCDispTypeInfo
     * This static function is to be used to create instances of XPCDispTypeInfo.
     */
    static
    XPCDispTypeInfo* New(XPCCallContext& ccx, JSObject* obj);
    virtual ~XPCDispTypeInfo();
    // ITypeInfo methods, see MSDN for detail information
    STDMETHOD(GetTypeAttr)( 
        /* [out] */ TYPEATTR __RPC_FAR *__RPC_FAR *ppTypeAttr);
    
    STDMETHOD(GetTypeComp)(
        /* [out] */ ITypeComp __RPC_FAR *__RPC_FAR *ppTComp);
    
    STDMETHOD(GetFuncDesc)( 
        /* [in] */ UINT index,
        /* [out] */ FUNCDESC __RPC_FAR *__RPC_FAR *ppFuncDesc);
    
    STDMETHOD(GetVarDesc)( 
        /* [in] */ UINT index,
        /* [out] */ VARDESC __RPC_FAR *__RPC_FAR *ppVarDesc);
    
    STDMETHOD(GetNames)( 
        /* [in] */ MEMBERID memid,
        /* [length_is][size_is][out] */ BSTR __RPC_FAR *rgBstrNames,
        /* [in] */ UINT cMaxNames,
        /* [out] */ UINT __RPC_FAR *pcNames);
    
    STDMETHOD(GetRefTypeOfImplType)( 
        /* [in] */ UINT index,
        /* [out] */ HREFTYPE __RPC_FAR *pRefType);
    
    STDMETHOD(GetImplTypeFlags)( 
        /* [in] */ UINT index,
        /* [out] */ INT __RPC_FAR *pImplTypeFlags);
    
    STDMETHOD(GetIDsOfNames)( 
        /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames,
        /* [in] */ UINT cNames,
        /* [size_is][out] */ MEMBERID __RPC_FAR *pMemId);
    
    STDMETHOD(Invoke)( 
        /* [in] */ PVOID pvInstance,
        /* [in] */ MEMBERID memid,
        /* [in] */ WORD wFlags,
        /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams,
        /* [out] */ VARIANT __RPC_FAR *pVarResult,
        /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo,
        /* [out] */ UINT __RPC_FAR *puArgErr);
    
    STDMETHOD(GetDocumentation)( 
        /* [in] */ MEMBERID memid,
        /* [out] */ BSTR __RPC_FAR *pBstrName,
        /* [out] */ BSTR __RPC_FAR *pBstrDocString,
        /* [out] */ DWORD __RPC_FAR *pdwHelpContext,
        /* [out] */ BSTR __RPC_FAR *pBstrHelpFile);
    
    STDMETHOD(GetDllEntry)( 
        /* [in] */ MEMBERID memid,
        /* [in] */ INVOKEKIND invKind,
        /* [out] */ BSTR __RPC_FAR *pBstrDllName,
        /* [out] */ BSTR __RPC_FAR *pBstrName,
        /* [out] */ WORD __RPC_FAR *pwOrdinal);
    
    STDMETHOD(GetRefTypeInfo)( 
        /* [in] */ HREFTYPE hRefType,
        /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo);
    
    STDMETHOD(AddressOfMember)( 
        /* [in] */ MEMBERID memid,
        /* [in] */ INVOKEKIND invKind,
        /* [out] */ PVOID __RPC_FAR *ppv);
    
    STDMETHOD(CreateInstance)( 
        /* [in] */ IUnknown __RPC_FAR *pUnkOuter,
        /* [in] */ REFIID riid,
        /* [iid_is][out] */ PVOID __RPC_FAR *ppvObj);
    
    STDMETHOD(GetMops)( 
        /* [in] */ MEMBERID memid,
        /* [out] */ BSTR __RPC_FAR *pBstrMops);
    
    STDMETHOD(GetContainingTypeLib)( 
        /* [out] */ ITypeLib __RPC_FAR *__RPC_FAR *ppTLib,
        /* [out] */ UINT __RPC_FAR *pIndex);
    
    virtual /* [local] */ void STDMETHODCALLTYPE ReleaseTypeAttr( 
        /* [in] */ TYPEATTR __RPC_FAR *pTypeAttr);
    
    virtual /* [local] */ void STDMETHODCALLTYPE ReleaseFuncDesc( 
        /* [in] */ FUNCDESC __RPC_FAR *pFuncDesc);
    
    virtual /* [local] */ void STDMETHODCALLTYPE ReleaseVarDesc( 
        /* [in] */ VARDESC __RPC_FAR *pVarDesc);
    /**
     * Returns the name of a function given a DISPID
     * @param dispID the DISPID to look up
     * @return the name of the function
     */
    const nsAString& GetNameForDispID(DISPID dispID);
private:
    /**
     * Initializes the object
     * @param ccx an XPConnect call context
     * @param obj the JS object being wrapped
     * @param array the array of JS ID's for the object
     */
    XPCDispTypeInfo(XPCCallContext& ccx, JSObject* obj, XPCDispIDArray* array);
    JSObject*               mJSObject;
    XPCDispIDArray*         mIDArray;
    XPCDispNameArray        mNameArray;
    // mFuncDescArray must occur after
    // TODO: We should probably refactor this so this isn't a requirement
    FuncDescArray        mFuncDescArray;
};

/**
 * Helper class that describes a JS function or property
 */
class XPCDispJSPropertyInfo
{
public:
    /**
     * Inspects a JS Function or property
     * @param cx A JS Context
     * @param memid the ID of the property or function
     * @param obj the JS object the property or function resides on
     * @param val ID val of the property or function
     */
    XPCDispJSPropertyInfo(JSContext*cx, PRUint32 memid, JSObject* obj, jsval val);
    /**
     * Returns true if the property information was initialized property
     * @return true if the property information was initialized property
     */
    PRBool Valid() const;
    /**
     * Returns the number of parameters
     * If this is a setter, the parameter count is always one
     * @return the number of parameters
     */
    PRUint32 GetParamCount() const;
    /**
     * Returns the generated member ID/dispid
     * This is based on the order of the function/property within the object
     * @return the memid of the property
     */
    PRUint32 GetMemID() const;
    /**
     * Returns the COM's INVOKEKIND for the property/method
     * @return the COM's INVOKEKIND for the property/method
     */
    INVOKEKIND GetInvokeKind() const;
    /**
     * Assigns the return type in elemDesc
     * @param ccx an xpconnect call context
     * @param elemDesc the element description to set the return type
     */
    void GetReturnType(XPCCallContext& ccx, ELEMDESC & elemDesc);
    /**
     * Returns an element description for the property
     * @return the element descriptoin object, ownership is assumed by
     * the caller.
     */
    ELEMDESC * GetParamInfo();
    /**
     * Tests whether this is a property
     * @return true if this is a property
     */
    PRBool IsProperty() const;
    /**
     * Test whether this is read-only
     * @return true if this is read-only
     */
    PRBool IsReadOnly() const;
    /**
     * Tests whether this is a setter
     * @return true if this is a setter
     */
    PRBool IsSetter() const;
    /**
     * Denotes this property has a setter (is not read-only)
     */
    void SetSetter();
    /**
     * returns the name of the property/method
     * @return the name of the property/method
     */
    nsAString const & GetName() const;
private:
    enum property_type
    {
        INVALID,
        PROPERTY,
        READONLY_PROPERTY,
        FUNCTION,
        SETTER_MODE = 0x20
    };

    PRUint32        mPropertyType;
    PRUint32        mParamCount;
    PRUint32        mMemID;
    jsval           mProperty;
    nsString        mName;

    /**
     * Accessor for the property type mProperty
     * @return property_type for the property
     */
    inline
    property_type PropertyType() const;
};

/**
 * Tearoff for nsXPCWrappedJS to use
 */
class XPCDispatchTearOff : public IDispatch, public ISupportErrorInfo
{
public:
    /**
     * Constructor initializes our COM pointer back to our main object
     */
    XPCDispatchTearOff(nsIXPConnectWrappedJS * wrappedJS);
    /**
     * Release the our allocated data, and decrements our main objects refcnt
     */
    virtual ~XPCDispatchTearOff();
    /**
     * Error handling function
     * @param riid the interface IID of the error
     */
    STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid);
    /**
     * Thread safe AddRef
     */
    STDMETHODIMP_(ULONG) AddRef();
    /**
     * Thread safe Release
     */
    STDMETHODIMP_(ULONG) Release();
    /**
     * QueryInterface that returns us or the main object
     * See MSDN for form information
     * @param IID interface ID we're querying to
     * @param pPtr a pointer to the pointer that will receive the resultant 
     * interface pointer
     * @return HRESULT
     */
    STDMETHOD(QueryInterface)(REFIID IID,void ** pPtr);
    /**
     * Returns the number of type info's for this IDispatch instance.
     * See MSDN for form information
     * @param pctinfo pointer to the variable to receive the count
     * @return HRESULT
     */
    STDMETHOD(GetTypeInfoCount)(unsigned int * pctinfo);
    /**
     * Returns the type information for this IDispatch instance
     * See MSDN for form information
     * @return HRESULT
     */
    STDMETHOD(GetTypeInfo)(unsigned int iTInfo, LCID lcid, 
                           ITypeInfo FAR* FAR* ppTInfo);
    /**
     * Returns the ID's for the given names of methods
     * See MSDN for form information
     * @return HRESULT
     */
    STDMETHOD(GetIDsOfNames)(REFIID riid, OLECHAR FAR* FAR* rgszNames, 
                             unsigned int cNames, LCID  lcid, 
                             DISPID FAR* rgDispId);
    /**
     * Invokes an interface method
     * See MSDN for form information
     * @return HRESULT
     */
    STDMETHOD(Invoke)(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
                      DISPPARAMS FAR* pDispParams, VARIANT FAR* pVarResult, 
                      EXCEPINFO FAR* pExcepInfo, unsigned int FAR* puArgErr);

private:
    // Pointer back to our main object
    nsCOMPtr<nsIXPConnectWrappedJS> mWrappedJS;
    // The Type information for our instance
    XPCDispTypeInfo *                 mCOMTypeInfo;
    // Reference count
    ULONG                             mRefCnt;
    // Returns the type information
    XPCDispTypeInfo *                 GetCOMTypeInfo();
    // Returns the JS Object being used to wrap
    inline
    JSObject* GetJSObject();

    NS_DECL_OWNINGTHREAD;
};

/**
 * IDispatch interface information.
 * This is used within the XPCWrappedNativeTearOff to collect and maintain 
 * information for a specific IDispatch interface. This is similar to
 * XPCNativeInterface
 */
class XPCDispInterface
{
public:
    /**
     * Member information. This class is used within the XPCDispInterface
     * to describe a specific method of an IDispatch interface.
     */
    class Member
    {
    public:
        /**
         * Parameter Information. This class is mainly a wrapper around 
         * ELEMDESC that allows easier inspection
         */
        class ParamInfo
        {
        public:
            /**
             * Initializes mParamInfo to the element description passed in
             * @param paramInfo the parameter information being wrapped
             */
            ParamInfo(const ELEMDESC * paramInfo);
            /**
             * Initializes an output parameter
             * @param varBuffer a pointer to the variant's buffer
             * @param var Pointer to the variant being initialized
             */
            JSBool InitializeOutputParam(void * varBuffer, 
                                         VARIANT & var) const;
            /**
             * Tests if a specific flag is set
             * @param flag the flag to be tested
             * @return true if the flag is set
             */
            PRBool IsFlagSet(unsigned short flag) const;
            /**
             * Returns true if this is an input parameter
             * @return true if this is an input parameter
             */
            PRBool IsIn() const;
            /**
             * Returns true if this is an output parameter
             * @return true if this is an output parameter
             */
            PRBool IsOut() const;
            /**
             * Returns true if this is an optional parameter
             * @return true if this is an optional parameter
             */
            PRBool IsOptional() const;
            /**
             * Returns true if this is a return value parameter
             * @return true if this is a return value parameter
             */
            PRBool IsRetVal() const;
            // TODO: Handle VT_ARRAY as well
            /**
             * Returns the type of the parameter
             * @return VARTYPE, the type of the parameter
             */
            VARTYPE GetType() const;
        private:
            const ELEMDESC * mParamInfo;
        };
        Member();
        ~Member();
        /**
         * Placement new is needed to initialize array in class XPCDispInterface
         * @param p the address of the member to construct
         * @return p
         */
        void* operator new(size_t, Member* p) CPP_THROW_NEW;
        /**
         * Is this a setter
         * @return true if this is a setter
         */
        PRBool IsSetter() const;
        /**
         * Is this a getter
         * @return true if this is a getter
         */
        PRBool IsGetter() const;
        /**
         * Is this a property
         * @return true if this is a property
         */
        PRBool IsProperty() const;
        /**
         * Is this a parameterized setter
         * @return true if this is a parameterized property
         */
        PRBool IsParameterizedSetter() const;
        /**
         * Is this a parameterized getter
         * @return true if this is a parameterized property
         */
        PRBool IsParameterizedGetter() const;
        /**
         * Is this a parameterized property
         * @return true if this is a parameterized property
         */
        PRBool IsParameterizedProperty() const;
        /**
         * Is this a function
         * @return true if this is a function
         */
        PRBool IsFunction() const;
        /**
         * Returns the name of the method as a jsval
         * @return the name of the method as a jsval
         */
        jsval GetName() const;
        /**
         * Returns the function object as a value for the method
         * @param ccx an XPConnect call context
         * @param iface The native interface of the function
         * @param retval pointer to the jsval to receive the name
         * @return JS_TRUE if the function object was return
         */
        JSBool GetValue(XPCCallContext& ccx, XPCNativeInterface* iface, 
                        jsval * retval) const;
        /**
         * returns the dispid of the method
         * @return the dispid of the method
         */
        PRUint32 GetDispID() const;
        /**
         * returns the number of parameters of the method
         * @param Ask from getter instead of setter version of the function
         * @return the number of parameters of the method
         */
        PRUint32 GetParamCount(PRBool getter = PR_FALSE) const;
        /**
         * Returns parameter information for the specified parameter
         * @param index the index of the parameter
         * @param Ask from getter instead of setter version of the function
         * @return Parameter information
         */
        ParamInfo GetParamInfo(PRUint32 index, PRBool getter = PR_FALSE) const;
        // === Setup functions ===
        /**
         * Sets the name of the method
         * @param name the name to assign
         */
        void SetName(jsval name);
        /**
         * Marks the member as a getter.
         * Both MakeGetter and MakeSetter can be called, making it a setter/getter
         */
        void MakeGetter();
        /**
         * Marks the member as a setter
         */
        void MakeSetter();
        /**
         * Marks the member as a function
         * Should not be called if MakeGetter/Setter is called. Will assert
         */
        void SetFunction();
        /**
         * Used to reset the type of of member
         */
        void ResetType();
        /**
         * Sets the type information for the parameter
         * @param dispID the DISPID of the member
         * @param pTypeInfo Pointer to the COM type information
         * @param fundesc function description
         */ 
        void SetTypeInfo(DISPID dispID, ITypeInfo* pTypeInfo,
                         FUNCDESC* funcdesc);
        /**
         * Sets the function description for the getter version of the function.
         * @param funcdesc function description
         */
        void SetGetterFuncDesc(FUNCDESC* funcdesc);
        /**
         * Sets the member ID
         * @param memID the IDispatch ID of the member
         */
        void SetMemID(DISPID memID);
        /**
         * Returns the IDispatch ID of the member
         * @return the IDispatch ID of the member
         */
        DISPID GetMemID() const;

    private:
       DISPID   mMemID;
        /**
         * Our internal flags identify the type of member
         * A member can be both getter/setter
         */
        enum member_type
        {
            UNINITIALIZED = 0,
            SET_PROPERTY = 1,
            GET_PROPERTY = 2,
            FUNCTION = 4,
            RESOLVED = 8
        };
        PRUint16 mType;
        jsval mVal;     // Mutable
        jsval mName;    // Mutable
        CComPtr<ITypeInfo> mTypeInfo;
        FUNCDESC* mFuncDesc; // We own this
        FUNCDESC* mGetterFuncDesc; // We own this
        /**
         * Helper function to return the parameter type
         * @param index index of the parameter to return the type of
         * @return The parameter type
         */
        PRUint16 GetParamType(PRUint32 index) const;
        /**
         * Helper function to test if a flag is set in mType
         * @param flag the flag to test for
         * @return true if the flag is set
         */
        PRBool IsFlagSet(unsigned short flag) const;
    };
    /**
     * Returns the JSObject for the tearoff
     * @return pointer to the JSObject
     * @see mJSObject
     */
    JSObject* GetJSObject() const;
    /**
     * Sets the JSObject for the tearoff
     * @param jsobj the object being assigned
     * @see GetJSObject() const
     */
    void SetJSObject(JSObject* jsobj);
    /**
     * Locates the member by name
     * @param name the name of the member to be returned
     * @return pointer to the member found, nsnull if not found
     */
    const Member * FindMember(jsval name) const;
    /**
     * Looksup a member ignoring case
     * TODO: We should look at performance issues concerning this
     * @param ccx A call context
     * @param name The name of the member
     * @return A pointer to a member or nsnull if not found
     */
    const Member* FindMemberCI(XPCCallContext& ccx, jsval name) const;
    /**
     * Returns a member via index
     * @param index the index of the parameter
     * @return reference to the member in the array
     */
    const Member & GetMember(PRUint32 index);
    /**
     * Returns the number of members
     * @return the number of members
     */
    PRUint32 GetMemberCount() const;
    /**
     * Creates a new instance of XPCDispInterface
     * @param cx a JS Context
     * @param pIface the interface pointer to the object
     * @return new instance of XPCDispInterface
     */
    static
    XPCDispInterface* NewInstance(JSContext* cx, nsISupports * pIface);
    /**
     * Delete operator that frees up the memory allocated to the object
     * @param p pointer to the objects memory
     */
    void operator delete(void * p);
    /**
     * Cleans up the members
     */
    ~XPCDispInterface();
private:
    /**
     * Initializes the object's members
     * @param cx a JS context
     * @param pTypeInfo pointer to the type type information
     * @param members number of members for the object
     */
    XPCDispInterface(JSContext* cx, 
                          ITypeInfo * pTypeInfo,
                          PRUint32 members);
    /**
     * Allocates the memory for the object
     * @param members number of members in this interface
     * @return pointer to the memory for the object
     */
    void * operator new (size_t, PRUint32 members) CPP_THROW_NEW;

    /**
     * This stores the JSObject for the tearoff, since this object
     * is stored as the JSObject * in the tearoff
     */
    JSObject*   mJSObject;
    PRUint32    mMemberCount;
    Member      mMembers[1];
    /**
     * Inspects the type information and stores it in this object
     * @param cx a JS context
     * @param pTypeInfo pointer to the type information for the object
     * @param members number of members in the interface
     * @return PR_TRUE if it worked, PR_FALSE if it didn't (usually out of 
     * memory)
     */
    PRBool InspectIDispatch(JSContext * cx, ITypeInfo * pTypeInfo, 
                          PRUint32 members);

    /**
     * Small utility to count members needed for XPConnect
     * XPConnect has one entry for a property while IDispatch can have two
     * Generally interfaces are small enough, that linear searching should
     * be ok
     */
    class Allocator
    {
    public:
        /**
         * Constructor, creates the initial buffer
         * @param cx a JS context
         * @param pTypeInfo pointer to IDispatch type info, our caller holds
         * the reference we don't need to
         */
        Allocator(JSContext * cx, ITypeInfo * pTypeInfo);
        /**
         * Destructor, frees the buffer we allocated
         */
        inline
        ~Allocator();
        /**
         * Returns the allocated XPCDispInterface object
         * @return the allocated XPCDispInterface object
         */
        inline
        XPCDispInterface* Allocate();
    private:
        DISPID * mMemIDs;
        PRUint32 mCount;            // Total unique ID's found
        PRUint32 mIDispatchMembers; // Total entries reported by ITypeInfo
        JSContext* mCX;
        ITypeInfo* mTypeInfo;

        /**
         * Returns the number of members found
         * @return The number of members found
         */
        inline
        PRUint32 Count() const;
        /**
         * Adds the member ID to the list
         * @param memID The member ID to test
         */
        void Add(DISPID memID);
        /**
         * Allows our caller to handle unexpected problems like out of memory
         * @return PR_TRUE if the buffer was allocated
         */
        inline
        PRBool Valid() const;

        // No copying or assigning allowed
        Allocator(const Allocator&);
        Allocator& operator =(const Allocator&);
    };
    /**
     * Friendship need to gain access to private operator new
     */
    friend class Allocator;
};

/**
 * Used to invoke IDispatch methods
 * This has turned into kind of a catch all, and probably should be
 * cleaned up
 */
class XPCDispObject
{
public:
    enum CallMode {CALL_METHOD, CALL_GETTER, CALL_SETTER};
    /**
     * This invokes an IDispatch method
     * @param ccx an XPConnect call context
     * @param pDisp the IDispatch pointer
     * @param dispID the DISPID of the method/property
     * @param mode the call mode, method/property
     * @param params the parameters need for the method/property
     * @param retval pointer to a jsval to receive the return value
     * @param member a pointer to an interface member
     * @param rt a pointer to the XPConnect JS Runtime
     * @return true if the method/property was invoked properly
     */
    static
    JSBool Dispatch(XPCCallContext& ccx, IDispatch * pDisp,
                    DISPID dispID, CallMode mode, XPCDispParams * params,
                    jsval* retval, XPCDispInterface::Member* member = nsnull,
                    XPCJSRuntime* rt = nsnull);
    /**
     * Used to invoke an IDispatch method using the XPCCallContext
     * @param ccx an XPConnect call context
     * @param mode call mode for the call
     */
    static
    JSBool Invoke(XPCCallContext & ccx, CallMode mode);
    /**
     * Performs the various security checks, caps, hosting flags, etc.
     * Instantiates the object and will return that object if createdObject 
     * result is not null
     * @param ccx an XPConnect call context
     * @param aCID the class ID to be tested
     * @param createdObject is the optional object to be returned
     */
    static
    HRESULT SecurityCheck(XPCCallContext & ccx, const CLSID & aCID,
                          IDispatch ** createdObject = nsnull);
    /**
     * Instantiates a COM object given a class ID or a prog ID
     * @param ccx an XPConnect call context
     * @param className a prog ID or a class ID in the form of 
     * {00000000-0000-0000-0000-000000000000}
     * @param enforceSecurity if true, will apply checks to ensure
     *                        the object can be created giving the current
     *                        security settings.
     * @param result pointer to the pointer to receive the interface pointer
     */
    static
    HRESULT COMCreateInstance(XPCCallContext & ccx, BSTR className,
                              PRBool enforceSecurity, IDispatch ** result);
    /**
     * Wraps an IDispatch interface, returning the object as a jsval
     * @param pDispatch IDispatch pointer
     * @param cx a JS Context
     * @param obj A pointer to a JS object serving as the global object
     * @param rval is a pointer to a jsval to receive the JS object wrapper
     */
    static
    PRBool WrapIDispatch(IDispatch *pDispatch, XPCCallContext & ccx,
                         JSObject *obj, jsval *rval);
};

class XPCIDispatchExtension
{
public:
    /**
     * Reset the enabled flag if xpconnect is re-initialized.
     */
    static void InitStatics() { mIsEnabled = PR_TRUE; }

    /**
     * returns true if IDispatch extension is enabled
     * @return true if IDispatch extension is enabled
     */
    static PRBool IsEnabled() { return mIsEnabled; }
    /**
     * Enables the IDispatch extension
     */
    static void Enable() { mIsEnabled = PR_TRUE; }
    /**
     * Disables the IDispatch extension
     */
    static void Disable() { mIsEnabled = PR_FALSE; }
    /**
     * Initializes the IDispatch support system
     * this exposes the ActiveXObject and COMObject to JS
     * @param aJSContext a JS context
     * @param aGlobalJSObj a global JS object
     */
    static JSBool Initialize(JSContext * aJSContext,
                             JSObject* aGlobalJSObj);
    /**
     * This is the define property for the IDispatch system. It called from
     * the XPConnect's DefineProperty
     * @param ccx an XPConnect call context
     * @param obj the JS object receiving the property
     * @param idval ID of the property to add
     * @param wrapperToReflectInterfaceNames the wrapper
     * @param propFlags JS property flags
     * @param resolved a pointer to a JSBool, set to true if properly resolved
     */
    static JSBool DefineProperty(XPCCallContext & ccx, 
                                 JSObject *obj, jsval idval,
                                 XPCWrappedNative* wrapperToReflectInterfaceNames,
                                 uintN propFlags, JSBool* resolved);
    /**
     * IDispatch system's enumeration function. This is called 
     * from XPC_WN_Shared_Enumerate
     * @param ccx a XPConnect call context
     * @param obj pointer to the JSObject
     * @param wrapper pointer to the wrapper
     * @return true if the enumeration was successful
     */
    static JSBool Enumerate(XPCCallContext& ccx, JSObject* obj, 
                            XPCWrappedNative * wrapper);
    /**
     * This is the delegated QI called from the wrapped JS class
     * DelegatedQueryInterface
     * @param self pointer to the object
     * @param aInstancePtr pointer to an interface pointer to receive the QI'd
     * pointer
     * @return an XPCOM result
     */
    static nsresult IDispatchQIWrappedJS(nsXPCWrappedJS * self, 
                                         void ** aInstancePtr);

private:
    static PRBool  mIsEnabled;
};

/**
 * This is a helper class that factored out the parameter cleanup code
 */
class XPCDispParams
{
public:
    /**
     * Initializes the parameters object
     * @param args the number of parameters this object will hold
     */
    XPCDispParams(PRUint32 args);
    /**
     * Cleans up the parameters' data
     */
    ~XPCDispParams();
    /**
     * This sets the named prop ID to our local mPropId. This is used for
     * setters.
     */
    void SetNamedPropID();
    /**
     * Returns a reference to a parameter
     * @param index index of the parameter
     * @return a reference to the parameter at index
     */
    VARIANT & GetParamRef(PRUint32 index);
    /**
     * Returns the parameter by value
     * @param index index of the parameter
     * @return a copy of the parameter
     */
    _variant_t GetParam(PRUint32 index) const;
    /**
     * Returns the output buffer for an output parameter
     * @param index index of the output parameter
     * @return a pointer to the buffer for the output parameter
     */
    void * GetOutputBuffer(PRUint32 index);
    /**
     * Returns a DISPPARAMS structure pointer for the parameters
     * @return a DISPPARAMS structure pointer for the parameters
     */
    DISPPARAMS* GetDispParams() const { return &NS_CONST_CAST(XPCDispParams*,this)->mDispParams; }
    /**
     * Returns the number of parameters
     * @return the number of parameters
     */
    uintN GetParamCount() const { return mDispParams.cArgs; }
    /**
     * Inserts a parameter
     * This is mainly used for parameterized properties
     * @param var the parameter to insert
     */
    void InsertParam(_variant_t & var);
private:
    /**
     * Don't allow copying
     */
    XPCDispParams(const XPCDispParams & other) {
        NS_ERROR("XPCDispParams can't be copied"); }
    /**
     * We don't allow assignments
     */
    XPCDispParams& operator =(const XPCDispParams&) {
        NS_ERROR("XPCDispParams can't be assigned"); }

    enum
    {
        DEFAULT_ARG_ARRAY_SIZE = 8,
        DEFAULT_REF_BUFFER_SIZE = 8 * sizeof(VARIANT)
    };
    static
    PRUint32 RefBufferSize(PRUint32 args) { return (args + 1) * sizeof(VARIANT); }

    DISPPARAMS  mDispParams;
    char*       mRefBuffer;
    VARIANT*    mDispParamsAllocated;
    char*       mRefBufferAllocated;
    /**
     * Used by output/ref variant types
     */
    char        mStackRefBuffer[DEFAULT_REF_BUFFER_SIZE];
    VARIANT     mStackArgs[DEFAULT_ARG_ARRAY_SIZE];
    DISPID      mPropID;
#ifdef DEBUG
    PRBool mInserted;
#endif
};

/**
 * Parameterized property object JSClass
 * This class is used to support parameterized properties for IDispatch
 */
class XPCDispParamPropJSClass
{
public:
    /**
     * returns a new or existing JS object as a jsval. This currently always
     * returns a new instance. But it may be worth looking into reusing
     * objects
     * @param ccx an XPConnect call context
     * @param wrapper the wrapper this parameterized property belongs to
     * @param dispID DISPID of the parameterized property
     * @param dispParams the parameters for the parameterized properties
     * @param paramPropObj pointer to the jsval that will receive the
     * JS function object
     * @return true if the JS function object was created
     */
    static JSBool NewInstance(XPCCallContext& ccx, XPCWrappedNative* wrapper,
                               PRUint32 dispID,
                               XPCDispParams* dispParams,
                               jsval* paramPropObj);
    /**
     * Cleans up the member, derefs the mDispObj, mWrapper and such
     */
    ~XPCDispParamPropJSClass();
    /**
     * Returns the wrapper
     * @return pointer to the wrapper (on loan)
     */
    XPCWrappedNative*       GetWrapper() const { return mWrapper; }
    /**
     * Invokes the parameterized getter/setter
     * @param ccx XPConnect call context
     * @param mode call mode
     * @param retval pointer to a jsval to receive the result
     */
    JSBool                  Invoke(XPCCallContext& ccx, 
                                   XPCDispObject::CallMode mode, 
                                   jsval* retval);
    /**
     * Returns the parameters for the parameterized property
     * @return a reference to the parameters for the parameterized property
     */
    XPCDispParams*    GetParams() const { return mDispParams; }
private:
    /**
     * Private constructor to initialize data members
     * @param wrapper The wrapper this parameterized object belongs to
     * @param dispObj pointer to the IDispatch object
     * @param dispID the DISPID of the parametersized property
     * @param dispParams the parameters for the parameterized property
     */
    XPCDispParamPropJSClass(XPCWrappedNative* wrapper, nsISupports* dispObj, 
                            PRUint32 dispID, XPCDispParams* dispParams);

    XPCWrappedNative*           mWrapper;
    PRUint32                    mDispID;
    XPCDispParams*              mDispParams;
    IDispatch*                  mDispObj;
};

/**
 * This class is a service that exposes some handy utility methods for
 * IDispatch users
 */
class nsDispatchSupport : public nsIDispatchSupport
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDISPATCHSUPPORT
    /**
     * Initialize nsISupports base objects
     */
    nsDispatchSupport();
    /**
     * Cleansup nsISupports
     */
    virtual ~nsDispatchSupport();
    /**
     * Returns the existing instance or creates a new one
     * @return an nsDispatchSupport object
     */
    static nsDispatchSupport* GetSingleton();
    /**
     * Called on shutdown to free the instance
     */
    static void FreeSingleton() { NS_IF_RELEASE(mInstance); }

private:
    static nsDispatchSupport* mInstance;
};

/**
 * Provides class info for IDispatch based objects
 */
class XPCIDispatchClassInfo : public nsIClassInfo
{
public:
    /**
     * Returns a single instance of XPCIDispatchClassInfo
     * @return the lone instance
     */
    static XPCIDispatchClassInfo* GetSingleton();
    /**
     * Releases our hold on the instance
     */
    static void FreeSingleton();
    NS_DECL_ISUPPORTS
    NS_DECL_NSICLASSINFO
private:
    /**
     * Only our methods create and destroy instances
     */
    XPCIDispatchClassInfo() {}
    virtual ~XPCIDispatchClassInfo() {}

    static XPCIDispatchClassInfo*  sInstance;
};

#include "XPCDispInlines.h"

#endif
