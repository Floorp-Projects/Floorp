/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the IDispatch implementation for XPConnect
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

#ifndef xpcprivate_h___
#error "DispPrivate.h should not be included directly, please use XPCPrivate.h"
#endif

#define NS_DECL_IUNKNOWN                                                      \
public:                                                                       \
  STDMETHOD(QueryInterface)(const struct _GUID& aIID,                         \
                            void** aInstancePtr);                             \
  STDMETHODIMP_(nsrefcnt) AddRef(void);                                       \
  STDMETHODIMP_(nsrefcnt) Release(void);                                      \
protected:                                                                    \
  nsAutoRefCnt mRefCnt;                                                       \
  NS_DECL_OWNINGTHREAD                                                        \

#define NS_IMPL_COM_QUERY_HEAD(_class)                                        \
STDMETHODIMP _class::QueryInterface(const GUID& aIID, void** aInstancePtr)    \
{                                                                             \
  NS_ASSERTION(aInstancePtr,                                                  \
               "QueryInterface requires a non-NULL destination!");            \
  if( !aInstancePtr )                                                         \
    return NS_ERROR_NULL_POINTER;                                             \
  IUnknown* foundInterface;

#define NS_IMPL_COM_QUERY_BODY(_interface)                                    \
  if(IsEqualIID(aIID, __uuidof(_interface)) )                                 \
    foundInterface = NS_STATIC_CAST(_interface*, this);                       \
  else

#define NS_IMPL_COM_QUERY_TAIL_GUTS                                           \
    foundInterface = 0;                                                       \
  nsresult status;                                                            \
  if( !foundInterface )                                                       \
    status = NS_NOINTERFACE;                                                  \
  else                                                                        \
    {                                                                         \
      NS_ADDREF(foundInterface);                                              \
      status = NS_OK;                                                         \
    }                                                                         \
  *aInstancePtr = foundInterface;                                             \
  return status;                                                              \
}

#define NS_COM_MAP_BEGIN(_implClass)      NS_IMPL_COM_QUERY_HEAD(_implClass)
#define NS_COM_MAP_ENTRY(_interface)      NS_IMPL_COM_QUERY_BODY(_interface)
#define NS_COM_MAP_END              NS_IMPL_COM_QUERY_TAIL_GUTS

#ifndef NS_MT_SUPPORTED
#error "We need MT support
#endif

extern nsID NSID_IDISPATCH;

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
     */
    static
    VARTYPE JSTypeToCOMType(XPCCallContext& ccx, jsval val);

    /**
     * Converts a JSVal to a COM variant
     * @param ccx XPConnect call context
     * @param src JS Value to convert
     * @param dest COM variant to receive the converted value
     * @param err receives the error code if any of a failed conversion
     * @return Returns true if the conversion succeeded
     */
    static
    JSBool JSToCOM(XPCCallContext& ccx, jsval src, VARIANT & dest, uintN & err);

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
                   uintN & err);
private:
    /**
     * Converts a JS Array to a safe array
     */
    static
    JSBool JSArrayToCOMArray(XPCCallContext& ccx, JSObject *obj, VARIANT & var,
                          uintN & err);
    /**
     * Converts a COM Array to a JS Array
     */
    static
    JSBool COMArrayToJSArray(XPCCallContext& ccx, const VARIANT & src,
                             jsval & dest,uintN& err);
};

JSBool JS_DLL_CALLBACK
XPC_IDispatch_CallMethod(JSContext *cx, JSObject *obj,
                  uintN argc, jsval *argv, jsval *vp);
JSBool JS_DLL_CALLBACK
XPC_IDispatch_GetterSetter(JSContext *cx, JSObject *obj,
                    uintN argc, jsval *argv, jsval *vp);

/**
 * Used to invoke IDispatch methods
 */
class XPCDispObject
{
public:
    enum CallMode {CALL_METHOD, CALL_GETTER, CALL_SETTER};
    /**
     * Used to invoke an IDispatch method
     */
    static
    JSBool Invoke(XPCCallContext & ccx, CallMode mode);
    /**
     * Instantiates a COM object given a class ID or a prog ID
     */
    static
    IDispatch * COMCreateInstance(const char * className);
    /**
     * Create a COM object from an existing IDispatch interface (e.g. returned by another object)
     */
    static
    PRBool COMCreateFromIDispatch(IDispatch *pDispatch, JSContext *cx, JSObject *obj, jsval *rval);

    /**
     * Throws an error, converting the errNum to an exception
     */
    static
    JSBool Throw(uintN errNum, JSContext* cx);
    /**
     * Cleans up a variant if it was allocated
     */
    inline
    static
    void CleanupVariant(VARIANT & var);
};

/**
 * This class holds an array of names. It indexes based on a *one based* dispid
 * This is only used in wrapped JS objects, so the dispid's are not arbitrary
 */
class XPCDispNameArray
{
public:
    XPCDispNameArray();
    ~XPCDispNameArray();
    void SetSize(PRUint32 size);
    PRUint32 GetSize() const;
    void SetName(PRUint32 index, nsACString const & name);
    nsCString Get(PRUint32 index) const;
    PRUint32 Find(const nsACString &target) const;
private:
    PRUint32 mCount;
    nsCString* mNames;
};

/**
 * This class represents an array of JSID's
 * It takes care of marking the ID's during GC
 */
class XPCDispIDArray
{
public:
    XPCDispIDArray(XPCCallContext& ccx, JSIdArray* array);
    PRUint32 Length() const;
    jsval Item(JSContext* cx, PRUint32 index) const;
    // Members used by AutoMarking framework
    void Mark();
    void Unmark();
    JSBool IsMarked() const;

    // NOP. This is just here to make the AutoMarkingPtr code compile.
    inline void MarkBeforeJSFinalize(JSContext*);
private:
    JSBool mMarked;
    nsVoidArray mIDArray;
};

class XPCDispJSPropertyInfo;

DEFINE_AUTO_MARKING_PTR_TYPE(AutoMarkingIDArrayPtr, XPCDispIDArray)

/**
 * Implements ITypeInfo for JSObjects
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
        FuncDescArray(XPCCallContext& ccx, JSObject* obj, 
                      const XPCDispIDArray& array, XPCDispNameArray & names);
        ~FuncDescArray();
        FUNCDESC* Get(PRUint32 index);
        void Release(FUNCDESC *);
        PRUint32 Length() const;
    private:
        nsVoidArray      mArray;

        void BuildFuncDesc(XPCCallContext& ccx, JSObject* obj,
                           XPCDispJSPropertyInfo & propInfo);
    };
    /**
     * Creates an instance of XPCDispTypeInfo
     * This static function is to be used to create instances of XPCDispTypeInfo.
     */
    static
    XPCDispTypeInfo* New(XPCCallContext& ccx, JSObject* obj);
    ~XPCDispTypeInfo();
    // ITypeInfo methods
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
    nsCString GetNameForDispID(DISPID dispID);
private:
    XPCDispTypeInfo(XPCCallContext& ccx, JSObject* obj, XPCDispIDArray* array);
    JSObject*               mJSObject;
    AutoMarkingIDArrayPtr   mIDArray;
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
     */
    XPCDispJSPropertyInfo(JSContext*cx, PRUint32 memid, JSObject* obj, jsval val);
    /**
     * Returns true if build properly
     */
    PRBool Valid() const { return mPropertyType != INVALID; }
    /**
     * Returns the number of parameters
     * If this is a setter, the parameter count is always one
     */
    PRUint32 GetParamCount() const { return IsSetterMode() ? 1 : mParamCount; }
    /**
     * Returns the generated member ID/dispid
     * This is based on the order of the function/property within the object
     */
    PRUint32 GetMemID() const { return mMemID; }
    /**
     * Returns the COM's INVOKEKIND for the function/property
     */
    INVOKEKIND GetInvokeKind() const { return IsSetterMode() ? INVOKE_PROPERTYPUT : (IsProperty() ? INVOKE_PROPERTYGET : INVOKE_FUNC); }
    /**
     * Assigns the return type in elemDesc
     */
    void GetReturnType(XPCCallContext& ccx, ELEMDESC & elemDesc);
    /**
     * Returns a
     */
    ELEMDESC * GetParamInfo();

    PRBool IsProperty() const { return PropertyType() == PROPERTY || PropertyType() == READONLY_PROPERTY; }
    PRBool IsReadOnly() const { return PropertyType()== READONLY_PROPERTY; }
    PRBool IsSetterMode() const { return (mPropertyType & SETTER_MODE) != 0; }
    void SetSetterMode() { mPropertyType |= SETTER_MODE; }
    nsACString const & GetName() const { return mName; }
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
    nsCAutoString   mName;

    VARTYPE COMType(XPCCallContext& ccx) const { return XPCDispConvert::JSTypeToCOMType(ccx, mProperty); }
    property_type PropertyType() const { return NS_STATIC_CAST(property_type,mPropertyType & ~SETTER_MODE); }
};

class nsIXPConnectWrappedJS;

/**
 * Tearoff for nsXPCWrappedJS to use
 */
class XPCDispatchTearOff : public IDispatch, public ISupportErrorInfo
{
public:
    STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid);

    /**
     * Constructor initializes our COM pointer back to our main object
     */
    XPCDispatchTearOff(nsIXPConnectWrappedJS * wrappedJS);
    /**
     * Release the our allocated data, and decrements our main objects refcnt
     */
    virtual ~XPCDispatchTearOff();
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
     */
    STDMETHOD(QueryInterface)(const struct _GUID & IID,void ** pPtr);
    /**
     * Returns the number of type info's for this IDispatch instance
     */
    STDMETHOD(GetTypeInfoCount)(unsigned int * pctinfo);
    /**
     * Returns the type information for this IDispatch instance
     */
    STDMETHOD(GetTypeInfo)(unsigned int iTInfo, LCID lcid, 
                           ITypeInfo FAR* FAR* ppTInfo);
    /**
     * Returns the ID's for the given names of methods
     */
    STDMETHOD(GetIDsOfNames)(REFIID riid, OLECHAR FAR* FAR* rgszNames, 
                             unsigned int cNames, LCID  lcid, 
                             DISPID FAR* rgDispId);
    /**
     * Invokes an interface method
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
    nsrefcnt                        mRefCnt;
    // Returns the type information
    XPCDispTypeInfo *                 GetCOMTypeInfo();
    // Returns the JS Object being used to wrap
    inline
    JSObject* GetJSObject();
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
            ParamInfo(const ELEMDESC * paramInfo);
            JSBool InitializeOutputParam(char * varBuffer, 
                                         VARIANT & var) const;
            /**
             * Tests if a specific flag is set
             */
            PRBool IsFlagSet(unsigned short flag) const;
            PRBool IsIn() const;
            PRBool IsOut() const;
            PRBool IsOptional() const;
            PRBool IsRetVal() const;
            // TODO: Handle VT_ARRAY as well
            VARTYPE GetType() const;
        private:
            const ELEMDESC * mParamInfo;
        };
        Member();
        ~Member();
        //=== Inspection functions ===
        /**
         * Placement new is needed to initialize array in class XPCDispInterface
         */
        void* operator new(size_t, Member* p);
        PRBool IsSetter() const;
        /**
         * Returns true if this is a getter
         */
        PRBool IsGetter() const;
        /**
         * Returns true if this is a setter
         */
        PRBool IsProperty() const;
        /**
         * Returns true if this is a function
         */
        PRBool IsFunction() const;
        /**
         * Returns the name of the method as a jsval
         */
        jsval GetName() const;
        /**
         * Returns the function object as a value for the method
         */
        JSBool GetValue(XPCCallContext& ccx, XPCNativeInterface* iface, 
                        jsval * retval) const;
        /**
         * returns the dispid of the method
         */
        PRUint32 GetDispID() const;
        /**
         * returns the number of parameters of the method
         */
        PRUint32 GetParamCount() const;
        /**
         * Returns parameter information for the specified parameter
         */
        ParamInfo GetParamInfo(PRUint32 index);
        // === Setup functions ===
        /**
         * Sets the name of the method
         */
        void SetName(jsval name);
        /**
         * Marks the member as a getter
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
         * Returns true if the member is a setter
         */ 
        void SetTypeInfo(DISPID dispID, ITypeInfo* pTypeInfo,
                         FUNCDESC* funcdesc);
    private:
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
        FUNCDESC* mFuncDesc; // We keep hold on this so we don't have 

        PRUint16 GetParamType(PRUint32 index) const;
        PRBool IsFlagSet(unsigned short flag) const;
    };
    JSObject* GetJSObject() const;
    void SetJSObject(JSObject* jsobj);
    const Member * FindMember(jsval name) const;
    const Member & GetMember(PRUint32 index);
    PRUint32 GetMemberCount() const;

    static
    XPCDispInterface* NewInstance(JSContext* cx, nsISupports * pIface);
    void operator delete(void * p);
private:
    XPCDispInterface(JSContext* cx, 
                          ITypeInfo * pTypeInfo,
                          PRUint32 members);
    void * operator new (size_t, PRUint32 members);

    JSObject*   mJSObject;
    PRUint32    mMemberCount;
    Member      mMembers[1];

    void InspectIDispatch(JSContext * cx, ITypeInfo * pTypeInfo, 
                          PRUint32 members);
};

class XPCIDispatchExtension
{
public:
    static PRBool IsEnabled() { return mIsEnabled; }
    static void Enable() { mIsEnabled = PR_TRUE; }
    static void Disable() { mIsEnabled = PR_FALSE; }
    static JSBool Initialize(JSContext * aJSContext,
                             JSObject* aGlobalJSObj);
    static JSBool DefineProperty(XPCCallContext & ccx, 
                                 JSObject *obj, jsval idval,
                                 XPCWrappedNative* wrapperToReflectInterfaceNames,
                                 uintN propFlags, JSBool* resolved);
    static JSBool Enumerate(XPCCallContext& ccx, JSObject* obj, 
                            XPCWrappedNative * wrapper);
    static nsresult IDispatchQIWrappedJS(nsXPCWrappedJS * self, 
                                         void ** aInstancePtr);

private:
    static PRBool  mIsEnabled;
};

#include "XPCDispInlines.h"

#endif
