/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Temporary Interface Info related stuff. */

#ifndef xpcbogusii_h___
#define xpcbogusii_h___

class nsXPCType
{
private:
    uint8 t;    // only member

public:
    nsXPCType();    // no implementation
    nsXPCType(uint8 type) : t(type) {}

    nsXPCType& operator=(uint8 type) {t = type; return *this;}
    operator uint8() const {return t;}

    enum
    {
        IS_POINTER        = 0x80,
        IS_UNIQUE_POINTER = 0x40,
        IS_REFERENCE      = 0x20,
        SPECIAL_BIT       = 0x10,
        TYPE_MASK         = 0xf,

        T_I8          = 0,
        T_I16         = 1,
        T_I32         = 2,
        T_I64         = 3,
        T_U8          = 4,
        T_U16         = 5,
        T_U32         = 6,
        T_U64         = 7,
        T_FLOAT       = 8,
        T_DOUBLE      = 9,
        T_BOOL        = 10,
        T_CHAR        = 11,
        T_WCHAR       = 12,
        T_VOID        = 13,

        T_P_I8        = IS_POINTER | 0,
        T_P_I16       = IS_POINTER | 1,
        T_P_I32       = IS_POINTER | 2,
        T_P_I64       = IS_POINTER | 3,
        T_P_U8        = IS_POINTER | 4,
        T_P_U16       = IS_POINTER | 5,
        T_P_U32       = IS_POINTER | 6,
        T_P_U64       = IS_POINTER | 7,
        T_P_FLOAT     = IS_POINTER | 8,
        T_P_DOUBLE    = IS_POINTER | 9,
        T_P_BOOL      = IS_POINTER | 10,
        T_P_CHAR      = IS_POINTER | 11,
        T_P_WCHAR     = IS_POINTER | 12,
        T_P_VOID      = IS_POINTER | 13,
        T_P_IID       = IS_POINTER | 14,
        T_STRING      = IS_POINTER | 15,

        T_INTERFACE     = 16,   /* SPECIAL_BIT | 0 */
        T_INTERFACE_IS  = 17    /* SPECIAL_BIT | 1 */
    };
};

class nsVarient
{
    nsXPCType type;

    union
    {
        int8    i8;
        int16   i16;
        int32   i32;
        int64   i64;
        uint8   u8;
        uint16  u16;
        uint32  u32;
        uint64  u64;
        float   f;
        double  d;
        PRBool  b;
        char    c;
        wchar_t wc;
        void*   p;
    } value;
};


class nsXPCParamInfo
{
public:
    // add ctor/dtor
    enum
    {
        IS_IN  = 0x80,
        IS_OUT = 0x40
    };

    JSBool IsIn()  const {return (JSBool) (flags & IS_IN);}
    JSBool IsOut() const {return (JSBool) (flags & IS_OUT);}
    nsXPCType GetType() const {return type;}
    uint8 GetInterfaceIsArgNumber() const
    {
        NS_PRECONDITION(type == nsXPCType::T_INTERFACE_IS,"not an interface_is");
        return interface_is_arg_num;
    }
    nsIInterfaceInfo* GetInterface() const
    {
        NS_PRECONDITION(type == nsXPCType::T_INTERFACE,"not an interface");
        return interface;
    }

private:
    uint8 flags;
    nsXPCType type;
    union
    {
        uint8 interface_is_arg_num;
        nsIInterfaceInfo* interface;
    };
};

class nsXPCMethodInfo
{
public:
    // add ctor/dtor
    enum
    {
        IS_GETTER         = 0x80,
        IS_SETTER         = 0x40,
        IS_VAR_ARGS       = 0x20,
        IS_CONSTRUCTOR    = 0x10
    };

    JSBool IsGetter()      const {return (JSBool) (flags & IS_GETTER);}
    JSBool IsSetter()      const {return (JSBool) (flags & IS_SETTER);}
    JSBool IsVarArgs()     const {return (JSBool) (flags & IS_VAR_ARGS);}
    JSBool IsConstructor() const {return (JSBool) (flags & IS_CONSTRUCTOR);}
    const char* GetName()  const {return mName;}
    uint8 GetParamCount()  const {return param_count;}
    const nsXPCParamInfo* GetParam(uint8 index) const
    {
        NS_PRECONDITION(index < param_count,"bad arg");
        return &params[index];
    }
    const nsXPCParamInfo* GetResult() const {return &result;}
private:
    uint8 flags;
    char* mName;
    uint8 param_count;
    nsXPCParamInfo* params;
    nsXPCParamInfo result;
};

class nsXPCConstant
{
    // XXX flesh out
};

class nsInterfaceInfo : public nsIInterfaceInfo
{
    NS_DECL_ISUPPORTS;

    NS_IMETHOD GetName(const char** name);
    NS_IMETHOD GetIID(const nsIID** iid);

    NS_IMETHOD GetParent(nsIInterfaceInfo** parent);

    // these include counts of parents
    NS_IMETHOD GetMethodCount(int* count);
    NS_IMETHOD GetConstantCount(int* count);

    // these include methods and constants of parents
    NS_IMETHOD GetMethodInfo(unsigned index, const nsXPCMethodInfo** info);
    NS_IMETHOD GetConstant(unsigned index, const nsXPCConstant** constant);

public:
    nsInterfaceInfo(REFNSIID aIID, const char* aName, nsInterfaceInfo* aParent);
    virtual ~nsInterfaceInfo();

private:
    char* mName;
    nsIID mIID;
    nsInterfaceInfo* mParent;

    unsigned mMethodBaseIndex;
    unsigned mMethodCount;
    nsXPCMethodInfo** mMethods;

    unsigned mConstantBaseIndex;
    unsigned mConstantCount;
    nsXPCConstant** mConstants;
};

#endif  /* xpcbogusii_h___ */