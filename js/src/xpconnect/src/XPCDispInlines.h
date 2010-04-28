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

/**
 * \file XPCDispInlines.h inline implementations
 * Implementations for inline members of classes found in DispPrivate.h
 */


inline
PRBool nsXPConnect::IsIDispatchEnabled() 
{
    return XPCIDispatchExtension::IsEnabled();
}

//=============================================================================
// XPCDispInterface::Member:ParamInfo inlines

inline
XPCDispInterface::Member::ParamInfo::ParamInfo(
    const ELEMDESC * paramInfo) : mParamInfo(paramInfo) 
{
}

inline
JSBool XPCDispInterface::Member::ParamInfo::InitializeOutputParam(
    void * varBuffer, VARIANT & var) const
{
    var.vt = GetType() | VT_BYREF;
    var.byref = varBuffer;
    return JS_TRUE;
}

inline
PRBool XPCDispInterface::Member::ParamInfo::IsFlagSet(
    unsigned short flag) const 
{
    return mParamInfo->paramdesc.wParamFlags & flag ? PR_TRUE : PR_FALSE; 
}

inline
PRBool XPCDispInterface::Member::ParamInfo::IsIn() const 
{
    return IsFlagSet(PARAMFLAG_FIN) || mParamInfo->paramdesc.wParamFlags == 0;
}

inline
PRBool XPCDispInterface::Member::ParamInfo::IsOut() const 
{
    return IsFlagSet(PARAMFLAG_FOUT);
}

inline
PRBool XPCDispInterface::Member::ParamInfo::IsOptional() const 
{
    return IsFlagSet(PARAMFLAG_FOPT);
}

inline
PRBool XPCDispInterface::Member::ParamInfo::IsRetVal() const 
{
    return IsFlagSet(PARAMFLAG_FRETVAL);
}

// TODO: Handle VT_ARRAY as well
inline
VARTYPE XPCDispInterface::Member::ParamInfo::GetType() const 
{
    return mParamInfo->tdesc.vt == VT_PTR ? mParamInfo->tdesc.lptdesc->vt : mParamInfo->tdesc.vt;
}

//=============================================================================
// XPCDispInterface::Member inlines

inline
XPCDispInterface::Member::Member() : 
    mType(UNINITIALIZED), mFuncDesc(nsnull), mGetterFuncDesc(nsnull),
    mTypeInfo(reinterpret_cast<ITypeInfo *>(nsnull))
{
}

inline
XPCDispInterface::Member::~Member() 
{
    if(mTypeInfo)
    {
        // Test to see if we have a separate getter. If we only have a getter they can
        // be the same
        PRBool releaseGetter = mGetterFuncDesc != nsnull && mFuncDesc != mGetterFuncDesc;
        if(mFuncDesc) 
            mTypeInfo->ReleaseFuncDesc(mFuncDesc);
        if(releaseGetter)
            mTypeInfo->ReleaseFuncDesc(mGetterFuncDesc);
    }
}

inline
void* XPCDispInterface::Member::operator new(size_t, Member* p)
{
    return p;
}

inline
void XPCDispInterface::Member::MakeGetter() 
{
    NS_ASSERTION(!IsFunction(), "Can't be function and property"); 
    mType |= GET_PROPERTY; 
}

inline
void XPCDispInterface::Member::MakeSetter() 
{ 
    NS_ASSERTION(!IsFunction(), "Can't be function and property"); 
    mType |= SET_PROPERTY; 
}

inline
void XPCDispInterface::Member::ResetType() 
{
    mType = UNINITIALIZED;
}

inline
void XPCDispInterface::Member::SetFunction() 
{ 
    NS_ASSERTION(!IsProperty(), "Can't be function and property"); 
    mType = FUNCTION; 
}

inline
PRBool XPCDispInterface::Member::IsFlagSet(unsigned short flag) const 
{
    return mType & flag ? PR_TRUE : PR_FALSE; 
}

inline
PRBool XPCDispInterface::Member::IsSetter() const
{
    return IsFlagSet(SET_PROPERTY);
}

inline
PRBool XPCDispInterface::Member::IsGetter() const
{
    return IsFlagSet(GET_PROPERTY);
}

inline
PRBool XPCDispInterface::Member::IsProperty() const
{
    return IsSetter() || IsGetter(); 
}

inline
PRBool XPCDispInterface::Member::IsFunction() const
{
    return IsFlagSet(FUNCTION);
}

inline
PRBool XPCDispInterface::Member::IsParameterizedSetter() const
{
    return IsSetter() && GetParamCount() > 1;
}

inline
PRBool XPCDispInterface::Member::IsParameterizedGetter() const
{
    return IsGetter() && (GetParamCount(PR_TRUE) > 1 ||
        (GetParamCount(PR_TRUE) == 1 && !GetParamInfo(0, PR_TRUE).IsRetVal()));
}

inline
PRBool XPCDispInterface::Member::IsParameterizedProperty() const
{
    return IsParameterizedSetter() || IsParameterizedGetter();
}

inline
jsval XPCDispInterface::Member::GetName() const
{
    return mName;
}

inline
void XPCDispInterface::Member::SetName(jsval name)
{
    mName = name;
}

inline
PRUint32 XPCDispInterface::Member::GetDispID() const
{
    return mFuncDesc->memid;
}

inline
PRUint32 XPCDispInterface::Member::GetParamCount(PRBool getter) const
{
    return (getter && mGetterFuncDesc) ? mGetterFuncDesc->cParams : mFuncDesc->cParams;
}

inline
XPCDispInterface::Member::ParamInfo XPCDispInterface::Member::GetParamInfo(PRUint32 index, PRBool getter) const
{
    NS_ASSERTION(index < GetParamCount(getter), "Array bounds error");
    return ParamInfo(((getter && mGetterFuncDesc) ? mGetterFuncDesc->lprgelemdescParam : mFuncDesc->lprgelemdescParam) + index);
}

inline
void XPCDispInterface::Member::SetTypeInfo(DISPID dispID, 
                                                ITypeInfo* pTypeInfo, 
                                                FUNCDESC* funcdesc)
{
    mTypeInfo = pTypeInfo; 
    mFuncDesc = funcdesc;
}

inline
void XPCDispInterface::Member::SetGetterFuncDesc(FUNCDESC* funcdesc)
{
    mGetterFuncDesc = funcdesc;
}

inline
PRUint16 XPCDispInterface::Member::GetParamType(PRUint32 index) const 
{
    return mFuncDesc->lprgelemdescParam[index].paramdesc.wParamFlags; 
}

inline
void XPCDispInterface::Member::SetMemID(DISPID memID)
{
    mMemID = memID;
}

inline
DISPID XPCDispInterface::Member::GetMemID() const
{
    return mMemID;
}

//=============================================================================
// XPCDispInterface::Allocator

inline
XPCDispInterface* XPCDispInterface::Allocator::Allocate()
{
    return Valid() ? new (Count()) XPCDispInterface(mCX, mTypeInfo, mIDispatchMembers) : nsnull;
}

inline
XPCDispInterface::Allocator::~Allocator()
{
    delete [] mMemIDs;
}

PRBool XPCDispInterface::Allocator::Valid() const
{
    return mMemIDs ? PR_TRUE : PR_FALSE;
}

//=============================================================================
// XPCDispInterface inlines

inline
JSObject* XPCDispInterface::GetJSObject() const
{
    return mJSObject;
}

inline
void XPCDispInterface::SetJSObject(JSObject* jsobj) 
{
    mJSObject = jsobj;
}

inline
const XPCDispInterface::Member* XPCDispInterface::FindMember(jsval name) const
{
    // Iterate backwards to save time
    const Member* member = mMembers + mMemberCount;
    while(member > mMembers)
    {
        --member;
        if(name == member->GetName())
        {
            return member;
        }
    }
    return nsnull;
}


inline
const XPCDispInterface::Member& XPCDispInterface::GetMember(PRUint32 index)
{ 
    NS_ASSERTION(index < mMemberCount, "invalid index");
    return mMembers[index]; 
}

inline
PRUint32 XPCDispInterface::GetMemberCount() const 
{
    return mMemberCount;
}

inline
void XPCDispInterface::operator delete(void * p) 
{
    PR_Free(p);
}

inline
XPCDispInterface::~XPCDispInterface()
{
    // Cleanup our members, the first gets cleaned up by the destructor
    // We have to cleanup the rest manually. These members are allocated
    // as part of the XPCIDispInterface object at the end
    for(PRUint32 index = 1; index < GetMemberCount(); ++index)
    {
        mMembers[index].~Member();
    }
}

inline
XPCDispInterface::XPCDispInterface(JSContext* cx, ITypeInfo * pTypeInfo,
                                   PRUint32 members) : mJSObject(nsnull)
{
    InspectIDispatch(cx, pTypeInfo, members);
}

inline
void * XPCDispInterface::operator new (size_t, PRUint32 members) 
{
    // Must allow for the member in XPCDispInterface
    if(!members)
        members = 1;
    // Calculate the size needed for the base XPCDispInterface and its members
    return PR_Malloc(sizeof(XPCDispInterface) + sizeof(Member) * (members - 1));
}

//=============================================================================
// XPCDispNameArray inlines

inline
XPCDispNameArray::XPCDispNameArray() : mCount(0), mNames(0) 
{
}

inline
XPCDispNameArray::~XPCDispNameArray() 
{ 
    delete [] mNames;
}

inline
void XPCDispNameArray::SetSize(PRUint32 size) 
{
    NS_ASSERTION(mCount == 0, "SetSize called more than once");
    mCount = size;
    mNames = (size ? new nsString[size] : 0);
}

inline
PRUint32 XPCDispNameArray::GetSize() const 
{
    return mCount;
}

inline
void XPCDispNameArray::SetName(DISPID dispid, nsAString const & name) 
{
    NS_ASSERTION(dispid <= (PRInt32)mCount, "Array bounds error in XPCDispNameArray::SetName");
    mNames[dispid - 1] = name;
}

inline
const nsAString & XPCDispNameArray::GetName(DISPID dispid) const 
{
    NS_ASSERTION(dispid <= (PRInt32)mCount, "Array bounds error in XPCDispNameArray::Get");
    if(dispid > 0)
        return mNames[dispid - 1];
    return EmptyString();
}

inline
DISPID XPCDispNameArray::Find(const nsAString &target) const
{
    for(PRUint32 index = 0; index < mCount; ++index) 
    {
        if(mNames[index].Equals(target)) 
            return static_cast<DISPID>(index + 1);
    }
    return 0; 
}

//=============================================================================
// XPCDispIDArray inlines

inline
PRUint32 XPCDispIDArray::Length() const
{
    return mIDArray.Length();
}

inline
jsval XPCDispIDArray::Item(JSContext* cx, PRUint32 index) const
{
    jsval val;
    if(!JS_IdToValue(cx, mIDArray.ElementAt(index), &val))
        return JSVAL_NULL;
    return val;
}

inline
void XPCDispIDArray::Unmark()
{
    mMarked = JS_FALSE;
}

inline
JSBool XPCDispIDArray::IsMarked() const
{
    return mMarked;
}

//=============================================================================
// XPCDispTypeInfo inlines

inline
FUNCDESC* XPCDispTypeInfo::FuncDescArray::Get(PRUint32 index) 
{
    return &mArray[index];
}

inline
void XPCDispTypeInfo::FuncDescArray::Release(FUNCDESC *) 
{
}

inline
PRUint32 XPCDispTypeInfo::FuncDescArray::Length() const 
{
    return mArray.Length();
}

inline
const nsAString & XPCDispTypeInfo::GetNameForDispID(DISPID dispID)
{
    return mNameArray.GetName(dispID);
}

//=============================================================================
// XPCDispJSPropertyInfo inlines

inline
PRBool XPCDispJSPropertyInfo::Valid() const 
{
    return mPropertyType != INVALID;
}

inline
PRUint32 XPCDispJSPropertyInfo::GetParamCount() const
{
    return IsSetter() ? 1 : mParamCount;
}

inline
PRUint32 XPCDispJSPropertyInfo::GetMemID() const
{
    return mMemID;
}

inline
INVOKEKIND XPCDispJSPropertyInfo::GetInvokeKind() const
{
    return IsSetter() ? INVOKE_PROPERTYPUT : 
        (IsProperty() ? INVOKE_PROPERTYGET : INVOKE_FUNC); 
}

inline
PRBool XPCDispJSPropertyInfo::IsProperty() const
{
    return PropertyType() == PROPERTY || PropertyType() == READONLY_PROPERTY;
}

inline
PRBool XPCDispJSPropertyInfo::IsReadOnly() const
{
    return PropertyType()== READONLY_PROPERTY;
}

inline
PRBool XPCDispJSPropertyInfo::IsSetter() const
{
    return (mPropertyType & SETTER_MODE) != 0;
}
inline
void XPCDispJSPropertyInfo::SetSetter()
{
    mPropertyType |= SETTER_MODE;
}

inline
const nsAString & XPCDispJSPropertyInfo::GetName() const
{
    return mName; 
}

inline
XPCDispJSPropertyInfo::property_type XPCDispJSPropertyInfo::PropertyType() const
{
    return static_cast<property_type>(mPropertyType & ~SETTER_MODE);
}

//=============================================================================
// GUID/nsIID/nsCID conversion functions

inline
const nsIID & XPCDispIID2nsIID(const IID & iid)
{
    NS_ASSERTION(sizeof(IID) == sizeof(nsIID), "IID is not the same size as nsIID");
    return reinterpret_cast<const nsIID &>(iid);
}

inline
const IID & XPCDispIID2IID(const nsIID & iid)
{
    NS_ASSERTION(sizeof(IID) == sizeof(nsIID), "IID is not the same size as nsIID");
    return reinterpret_cast<const IID &>(iid);
}

inline
const nsCID & XPCDispCLSID2nsCID(const CLSID & clsid)
{
    NS_ASSERTION(sizeof(CLSID) == sizeof(nsCID), "CLSID is not the same size as nsCID");
    return reinterpret_cast<const nsCID &>(clsid);
}

inline
const CLSID & XPCDispnsCID2CLSID(const nsCID & clsid)
{
    NS_ASSERTION(sizeof(CLSID) == sizeof(nsCID), "CLSID is not the same size as nsCID");
    return reinterpret_cast<const CLSID &>(clsid);
}

//=============================================================================
// XPCDispParams inlines

inline
void XPCDispParams::SetNamedPropID()
{
    mDispParams.rgdispidNamedArgs = &mPropID; 
    mDispParams.cNamedArgs = 1; 
}

inline
VARIANT & XPCDispParams::GetParamRef(PRUint32 index)
{
    NS_ASSERTION(index < mDispParams.cArgs, "XPCDispParams::GetParam bounds error");
    return mDispParams.rgvarg[mDispParams.cArgs - index - 1];
}

inline
_variant_t XPCDispParams::GetParam(PRUint32 index) const
{
    return const_cast<XPCDispParams*>(this)->GetParamRef(index);
}

inline
void * XPCDispParams::GetOutputBuffer(PRUint32 index)
{
    NS_ASSERTION(index < mDispParams.cArgs, "XPCDispParams::GetParam bounds error");
    return mRefBuffer + sizeof(VARIANT) * index;
}

//=============================================================================
// Other helper functions

/**
 * Converts a jsval that is a string to a char const *
 * @param cx a JS context
 * @param val The JS value to be converted
 * @return a C string (Does not need to be freed)
 */
inline
jschar * xpc_JSString2String(JSContext * cx, jsval val, PRUint32 * len = 0)
{
    JSString* str = JSVAL_IS_STRING(val) ? JSVAL_TO_STRING(val) : 
                                           JS_ValueToString(cx, val);
    if(str)
    {
        if(len)
            *len = JS_GetStringLength(str);
        return JS_GetStringChars(str);
    }
    if(len)
        *len = 0;
    return nsnull;
}

/**
 * Converts a jsval that is a string to a PRUnichar *
 * @param cx a JS context
 * @param val the JS value to vbe converted
 * @param length optional pointer to a variable to hold the length
 * @return a PRUnichar buffer (Does not need to be freed)
 */
inline
PRUnichar* xpc_JSString2PRUnichar(XPCCallContext& ccx, jsval val,
                                  size_t* length = nsnull)
{
    JSString* str = JS_ValueToString(ccx, val);
    if(!str)
        return nsnull;
    if(length)
        *length = JS_GetStringLength(str);
    return reinterpret_cast<PRUnichar*>(JS_GetStringChars(str));
}

