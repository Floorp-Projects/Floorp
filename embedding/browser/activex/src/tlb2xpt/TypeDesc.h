#ifndef TYPEDESC_H
#define TYPEDESC_H

class TypeDesc
{
public:
    enum Type
    {
        T_POINTER, // A pointer to something else
        T_ARRAY,   // An array of other things
        T_VOID,
        T_RESULT, // nsresult / HRESULT
        T_CHAR,
        T_WCHAR,
        T_INT8,
        T_INT16,
        T_INT32,
        T_INT64,
        T_UINT8,
        T_UINT16,
        T_UINT32,
        T_UINT64,
        T_STRING,
        T_WSTRING,
        T_FLOAT,
        T_DOUBLE,
        T_BOOL,
        T_INTERFACE,
        T_OTHER,
        T_UNSUPPORTED
    };

    Type      mType;
    union {
        // T_POINTER
        TypeDesc *mPtr;
        // T_ARRAY
        struct {
            long mNumElements;
            TypeDesc **mElements;
        } mArray;
        // T_UNSUPPORTED
        char *mName;
    } mData;

    TypeDesc(ITypeInfo* pti, TYPEDESC* ptdesc);
    ~TypeDesc();

	std::string ToXPIDLString();
	std::string ToCString();
};

#endif