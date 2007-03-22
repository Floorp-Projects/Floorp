#ifndef XPCDispUtilities_h
#define XPCDispUtilities_h

template <class T>
inline
HRESULT XPCCreateInstance(const CLSID & clsid, const IID & iid, T ** result)
{
    return CoCreateInstance(clsid, 0, CLSCTX_ALL, iid, reinterpret_cast<void**>(result));
}

DISPID GetIDsOfNames(IDispatch * pIDispatch , CComBSTR const & name)
{
    DISPID dispid;
    OLECHAR * pName = name;
    HRESULT hresult = pIDispatch->GetIDsOfNames(
        IID_NULL,
        &pName,
        1, 
        LOCALE_SYSTEM_DEFAULT,
        &dispid);
    if (!SUCCEEDED(hresult))
    {
        dispid = 0;
    }
    return dispid;
}

#endif