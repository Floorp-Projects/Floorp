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

/**
 * \file XPCDispInlines.h inline implementations
 * Implementations for inline members of classes found in DispPrivate.h
 */


inline
PRBool nsXPConnect::IsIDispatchEnabled() 
{
    return XPCIDispatchExtension::IsEnabled();
}

// XPCDispObject inlines 
inline
void XPCDispObject::CleanupVariant(VARIANT & var)
{
    VariantClear(&var);
}

// XPCDispInterface inlines

inline
XPCDispInterface::Member::ParamInfo::ParamInfo(
    const ELEMDESC * paramInfo) : mParamInfo(paramInfo) 
{
}

inline
JSBool XPCDispInterface::Member::ParamInfo::InitializeOutputParam(
    char * varBuffer, VARIANT & var) const
{
    var.vt = GetType() | VT_BYREF;
    // TODO: This is a bit hacky, but we just pick one of the pointer types;
    var.byref = NS_REINTERPRET_CAST(BSTR*,varBuffer);
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
    return IsFlagSet(PARAMFLAG_FIN);
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

inline
XPCDispInterface::Member::Member() : 
    mType(UNINITIALIZED), mFuncDesc(0),
    mTypeInfo(0) 
{
}

inline
XPCDispInterface::Member::~Member() 
{
    if(mTypeInfo && mFuncDesc) 
        mTypeInfo->ReleaseFuncDesc(mFuncDesc);
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
    return IsFlagSet(GET_PROPERTY);
}

inline
PRBool XPCDispInterface::Member::IsGetter() const
{
    return IsFlagSet(SET_PROPERTY);
}

inline
PRBool XPCDispInterface::Member::IsProperty() const
{
    return IsFlagSet(SET_PROPERTY) || IsFlagSet(GET_PROPERTY); 
}

inline
PRBool XPCDispInterface::Member::IsFunction() const
{
    return IsFlagSet(FUNCTION);
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
PRUint32 XPCDispInterface::Member::GetParamCount() const
{
    return mFuncDesc->cParams;
}

inline
XPCDispInterface::Member::ParamInfo XPCDispInterface::Member::GetParamInfo(PRUint32 index) 
{
    NS_ASSERTION(index < GetParamCount(), "Array bounds error");
    return ParamInfo(mFuncDesc->lprgelemdescParam + index);
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
PRUint16 XPCDispInterface::Member::GetParamType(PRUint32 index) const 
{
    return mFuncDesc->lprgelemdescParam[index].paramdesc.wParamFlags; 
}

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
    free(p);
}

inline
XPCDispInterface::XPCDispInterface(JSContext* cx, 
                                             ITypeInfo * pTypeInfo, 
                                             PRUint32 members) : 
    mJSObject(nsnull) 
{
    InspectIDispatch(cx, pTypeInfo, members);
}

inline
void * XPCDispInterface::operator new (size_t, PRUint32 members) 
{
    return malloc(sizeof(XPCDispInterface) + sizeof(Member) * (members - 1));
}

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
    mNames = (size ? new nsCString[size] : 0);
}

inline
PRUint32 XPCDispNameArray::GetSize() const 
{
    return mCount;
}

inline
void XPCDispNameArray::SetName(PRUint32 index, nsACString const & name) 
{
    mNames[index - 1] = name;
}

inline
nsCString XPCDispNameArray::Get(PRUint32 index) const 
{
    if(index > 0)
        return mNames[index - 1];
    return nsCString();
}

inline
PRUint32 XPCDispNameArray::Find(const nsACString &target) const
{
    for(PRUint32 index = 0; index < mCount; ++index) 
    {
        if(mNames[index] == target) 
            return index + 1; 
    }
    return 0; 
}

// XPCDispIDArray inlines

inline
PRUint32 XPCDispIDArray::Length() const
{
    return mIDArray.Count();
}

inline
jsval XPCDispIDArray::Item(JSContext* cx, PRUint32 index) const
{
    jsval val;
    if (!JS_IdToValue(cx, 
                      NS_REINTERPRET_CAST(jsid, 
                                          mIDArray.ElementAt(index)), &val))
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

    // NOP. This is just here to make the AutoMarkingPtr code compile.
inline
void XPCDispIDArray::MarkBeforeJSFinalize(JSContext*) 
{
}

// XPCDispTypeInfo

inline
FUNCDESC* XPCDispTypeInfo::FuncDescArray::Get(PRUint32 index) 
{
    return NS_REINTERPRET_CAST(FUNCDESC*,mArray[index]);
}

inline
void XPCDispTypeInfo::FuncDescArray::Release(FUNCDESC *) 
{
}

inline
PRUint32 XPCDispTypeInfo::FuncDescArray::Length() const 
{
    return mArray.Count();
}

inline
nsCString XPCDispTypeInfo::GetNameForDispID(DISPID dispID)
{
    return mNameArray.Get(dispID);
}

inline
const nsIID & XPCDispGUID2nsIID(const struct _GUID & guid, nsIID & iid)
{
    NS_ASSERTION(sizeof(struct _GUID) == sizeof(nsIID), "GUID is not the same as nsIID");
    iid = NS_REINTERPRET_CAST(const nsIID &,guid);
    return iid;
}

inline
const GUID & XPCDispIID2GUID(const nsIID & iid, struct _GUID & guid)
{
    NS_ASSERTION(sizeof(struct _GUID) == sizeof(nsIID), "GUID is not the same as IID");
    guid = NS_REINTERPRET_CAST(const struct _GUID &, iid);
    return guid;
}
