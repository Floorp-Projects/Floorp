
#include "stdafx.h"

#include "TypeDesc.h"

TypeDesc::TypeDesc(ITypeInfo* pti, TYPEDESC* ptdesc)
{
	if (ptdesc->vt == VT_PTR)
	{
		// ptdesc->lptdesc points to a TYPEDESC that specifies the thing pointed to
        mType = T_POINTER;
        mData.mPtr = new TypeDesc(pti, ptdesc->lptdesc);
	}
	else if ((ptdesc->vt & 0x0FFF) == VT_CARRAY)
	{
        mType = T_UNSUPPORTED;
        mData.mName = strdup("VT_CARRAY");
        /*
        // ptdesc->lpadesc points to an ARRAYDESC
		str = TYPEDESCtoString(pti, &ptdesc->lpadesc->tdescElem );

		// Allocate cDims * lstrlen("[123456]")
        char buf[20];
		for (USHORT n = 0; n < ptdesc->lpadesc->cDims; n++)
		{
            sprintf(buf, "[%d]", ptdesc->lpadesc->rgbounds[n].cElements);
			str += buf;
		}
		return str;
        */
	}
    else if ((ptdesc->vt & 0x0FFF) == VT_SAFEARRAY)
	{
        mType = T_UNSUPPORTED;
        mData.mName = strdup("VT_SAFEARRAY");
        /*
		str = "SAFEARRAY(" + TYPEDESCtoString( pti, ptdesc->lptdesc ) + ")";
		return str;
        */
	}
    else  switch(ptdesc->vt)
    {
    case VT_VOID:
        mType = T_VOID;
        break;
    case VT_HRESULT:
        mType = T_RESULT;
        break;
    case VT_I1:
        mType = T_INT8;
        break;
    case VT_I2:
        mType = T_INT16;
        break;
    case VT_INT:
    case VT_I4:
        mType = T_INT32;
        break;
    case VT_I8:
        mType = T_INT64;
        break;
    case VT_UI1:
        mType = T_UINT8;
        break;
    case VT_UI2:
        mType = T_UINT16;
        break;
    case VT_UINT:
    case VT_UI4:
        mType = T_UINT32;
        break;
    case VT_UI8:
        mType = T_UINT64;
        break;
    case VT_BSTR:
    case VT_LPWSTR:
        mType = T_WSTRING;
        break;
    case VT_LPSTR:
        mType = T_STRING;
        break;
    case VT_UNKNOWN:
    case VT_DISPATCH:
        mType = T_INTERFACE;
        break;
    case VT_BOOL:
        mType = T_BOOL;
        break;
    case VT_R4:
        mType = T_FLOAT;
        break;
    case VT_R8:
        mType = T_DOUBLE;
        break;
    case VT_EMPTY:
        mType = T_UNSUPPORTED;
        mData.mName = strdup("VT_EMPTY");
        break;
    case VT_NULL:
        mType = T_UNSUPPORTED;
        mData.mName = strdup("VT_NULL");
        break;
    case VT_CY:
        mType = T_UNSUPPORTED;
        mData.mName = strdup("VT_CY");
        break;
    case VT_DATE:
        mType = T_UNSUPPORTED;
        mData.mName = strdup("VT_DATE");
        break;
    case VT_ERROR:
        mType = T_UNSUPPORTED;
        mData.mName = strdup("VT_ERROR");
        break;
    case VT_VARIANT:
        mType = T_UNSUPPORTED;
        mData.mName = strdup("VT_VARIANT");
        break;
    case VT_USERDEFINED:
        mType = T_UNSUPPORTED;
        mData.mName = strdup("VT_USERDEFINED");
        break;
    default:
        {
            char szBuf[50];
            sprintf(szBuf, "VT = %08x", ptdesc->vt);
            mType = T_UNSUPPORTED;
            mData.mName = strdup(szBuf);
        }
        break;
    }
}

TypeDesc::~TypeDesc()
{
    if (mType == T_ARRAY)
    {
        delete mData.mArray.mElements;
    }
    else if (mType == T_POINTER)
    {
        delete mData.mPtr;
    }
    else if (mType == T_UNSUPPORTED)
    {
        free(mData.mName);
    }
}

std::string TypeDesc::ToCString()
{
    std::string str;

	if (mType == T_POINTER)
	{
		// ptdesc->lptdesc points to a TYPEDESC that specifies the thing pointed to
		str = mData.mPtr->ToXPIDLString();
		str += " *";
		return str;
	}
	else if (mType == T_ARRAY)
	{
        // TODO
        str = "void * /* T_ARRAY */";
		return str;
	}
    else switch (mType) {
    case T_VOID:    return "void";
    case T_RESULT:  return "nsresult";
    case T_CHAR:    return "char";
    case T_WCHAR:   return "PRUnichar";
    case T_INT8:    return "PRInt8";
    case T_INT16:   return "PRInt16";
    case T_INT32:   return "PRInt32";
    case T_INT64:   return "PRInt64";
    case T_UINT8:   return "PRUint8";
    case T_UINT16:  return "PRUint16";
    case T_UINT32:  return "PRUint32";
    case T_UINT64:  return "PRUint64";
    case T_STRING:  return "char*";
    case T_WSTRING: return "PRUnichar*";
    case T_FLOAT:   return "float";
    case T_DOUBLE:  return "double";
    case T_BOOL:    return "PRBool";
    case T_INTERFACE: return "nsISupports *";
    default:
    case T_UNSUPPORTED:
        {
            std::string str = "/*";
            str += mData.mName;
            str += " */ void ";
            return str;
        }
   }
}

std::string TypeDesc::ToXPIDLString()
{
    std::string str;

	if (mType == T_POINTER)
	{
		// ptdesc->lptdesc points to a TYPEDESC that specifies the thing pointed to
		str = mData.mPtr->ToXPIDLString();
		str += " *";
		return str;
	}
	else if (mType == T_ARRAY)
	{
        // TODO
        str = "void * /* T_ARRAY */";
		return str;
	}
    else switch (mType) {
    case T_VOID:    return "void";
    case T_RESULT:  return "nsresult";
    case T_CHAR:    return "char";
    case T_WCHAR:   return "wchar";
    case T_INT8:    return "octet";
    case T_INT16:   return "short";
    case T_INT32:   return "long";
    case T_INT64:   return "long long";
    case T_UINT8:   return "octect";
    case T_UINT16:  return "unsigned short";
    case T_UINT32:  return "unsigned long";
    case T_UINT64:  return "unsigned long long";
    case T_STRING:  return "string";
    case T_WSTRING: return "wstring";
    case T_FLOAT:   return "float";
    case T_DOUBLE:  return "double";
    case T_BOOL:    return "boolean";
    case T_INTERFACE: return "nsISupports";
    default:
    case T_UNSUPPORTED:
        {
            std::string str = "/* ";
            str += mData.mName;
            str += " */ void";
            return str;
        }
   }
}
