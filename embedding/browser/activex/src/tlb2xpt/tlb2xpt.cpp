// tlb2xpt.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


std::string TYPEDESCtoString(ITypeInfo* pti, TYPEDESC* ptdesc);
std::string VTtoString(VARTYPE vt);

void DumpXPCOMInterfaceIDL(ITypeInfo *typeInfo);

FILE *fidl   = NULL;
FILE *fxpt   = NULL;
FILE *fstubc = NULL;
FILE *fstubh = NULL;

int main(int argc, char* argv[])
{
    BOOL verbose = FALSE;
    BOOL genIDL = FALSE;
    BOOL genXPT = FALSE;
    BOOL genStubs = FALSE;
    
    char *inputTLB = NULL;
    char *output = NULL;

    for (int arg = 1; arg < argc; arg++)
    {
        if (stricmp(argv[arg], "-verbose") == 0)
        {
            verbose = TRUE;
        }
        else if (stricmp(argv[arg], "-idl") == 0)
        {
            genIDL = TRUE;
        }
        else if (stricmp(argv[arg], "-xpt") == 0)
        {
            genXPT = TRUE;
        }
        else if (stricmp(argv[arg], "-stubs") == 0)
        {
            genStubs = TRUE;
        }
        else if (!inputTLB)
        {
          inputTLB = argv[arg];
        }
        else if (!output)
        {
            output = argv[arg];
        }
    }
    if (inputTLB == NULL)
    {
        fprintf(stderr, "Usage: tlb2xpt [-verbose] [-idl] [-stubs] [-xpt] typelibrary outputname\n");
        fputs("Files will be created with the outputname and .cpp, .h, .idl\n"
              "and .xpt extensions as appropriate for the specified switches.\n",
              stderr);
        return 1;
    }

    // Open FILE handles to the various things that need to be generated
    if (genIDL)
    {
        if (output)
        {
            std::string filename(output);
            filename += ".idl";
            fidl = fopen(filename.c_str(), "wt");
        }
        else
        {
            fidl = stdout;
        }
    }
    // TODO genStubs, genXPT

    if (verbose)
        fprintf(stderr, "Opening TLB \"%s\"\n", inputTLB);

    ITypeLibPtr typeLib;
    USES_CONVERSION;
    HRESULT hr = LoadTypeLib(A2W(inputTLB), &typeLib);
    if (FAILED(hr))
    {
        fprintf(stderr, "Error: failed to open \"%s\"\n", inputTLB);
        return 1;
    }

    UINT count = typeLib->GetTypeInfoCount();
    if (verbose)
        fprintf(stderr, "TLB contains %d entries\n", count);

    for (UINT i = 0; i < count; i++)
    {
        ITypeInfoPtr typeInfo;
        typeLib->GetTypeInfo(i, &typeInfo);
        ITypeInfo2Ptr type2Info = typeInfo;

        TYPEATTR *attr;
        type2Info->GetTypeAttr(&attr);

        char *type;
        switch (attr->typekind)
        {
        case TKIND_ENUM:
            type = "TKIND_ENUM";
            break;
        case TKIND_RECORD:
            type = "TKIND_RECORD";
            break;
        case TKIND_MODULE:
            type = "TKIND_MODULE";
            break;
        case TKIND_INTERFACE:
            type = "TKIND_INTERFACE";
            if (genIDL)
                DumpXPCOMInterfaceIDL(type2Info);
            break;
        case TKIND_DISPATCH:
            type = "TKIND_DISPATCH";
            break;
        case TKIND_COCLASS:
            type = "TKIND_COCLASS";
            break;
        case TKIND_ALIAS:
            type = "TKIND_ALIAS";
            break;
        case TKIND_UNION:
            type = "TKIND_UNION";
            break;
        case TKIND_MAX:
            type = "TKIND_MAX";
            break;
        default:
            type = "default";
            break;
        }
        if (verbose)
            fprintf(stderr, "Reading %s type\n", type);


        type2Info->ReleaseTypeAttr(attr);
    }
    return 0;
}


// [scriptable, uuid(00000000-0000-0000-0000-000000000000)]
// interface axIFoo : axIBar
void DumpXPCOMInterfaceIDL(ITypeInfo *tiInterface)
{
    HRESULT hr;

    TYPEATTR *attr;
    tiInterface->GetTypeAttr(&attr);

    USES_CONVERSION;

    //
    // Attribute block
    //
    fputs("[scriptable, ", fidl);

    // uuid()
	WCHAR szGUID[64];
	StringFromGUID2(attr->guid, szGUID, sizeof(szGUID));
    szGUID[0] = L'(';
    szGUID[wcslen(szGUID) - 1] = L')';
    fprintf(fidl, "uuid%s", W2A(szGUID));
    
    fputs("]\n", fidl);


    //
    // Interface block
    //

    fprintf(fidl, "interface ");

    BSTR bstrName = NULL;
    hr = tiInterface->GetDocumentation( MEMBERID_NIL, &bstrName, NULL, NULL, NULL);

    fprintf(fidl, "ax%s", W2A(bstrName));

    // Check for the base interface
	for (UINT n = 0; n <  attr->cImplTypes; n++)
	{
		HREFTYPE href = NULL;
		if (FAILED(hr = tiInterface->GetRefTypeOfImplType(n, &href)))
            // TODO
           ;

        ITypeInfoPtr tiParent;
		if (FAILED(hr = tiInterface->GetRefTypeInfo(href, &tiParent)))
            // TODO
           ;

		if (FAILED(hr = tiParent->GetDocumentation( MEMBERID_NIL, &bstrName, NULL, NULL, NULL)))
            // TODO
           ;

		fprintf(fidl, " : ax%s", W2A(bstrName));

		SysFreeString(bstrName);
        bstrName = NULL;

		tiParent.Release();
	}

    //
    // Methods and properties block
    //

    fprintf(fidl, "\n");
    fprintf(fidl, "{\n");


    for (n = 0; n < attr->cFuncs; n++)
    {
        FUNCDESC *func;
        tiInterface->GetFuncDesc(n, &func);

        fprintf(fidl, "  ");
        if (func->invkind & INVOKE_PROPERTYPUT ||
            func->invkind & INVOKE_PROPERTYGET)
		{
		}

        // Return type
        TypeDesc tf(tiInterface, &func->elemdescFunc.tdesc);
        fprintf(fidl, "%s ", tf.ToXPIDLString().c_str());

        // Method / property name
        BSTR bstrName = NULL;
    	tiInterface->GetDocumentation(func->memid, &bstrName, NULL, NULL, NULL);
        fprintf(fidl, "%s (\n", W2A(bstrName));
        SysFreeString(bstrName);

        // Get the names of all the arguments
		UINT cNames;
	    BSTR rgbstrNames[100];
  		hr = tiInterface->GetNames(func->memid, rgbstrNames, 100, (UINT FAR*) &cNames);

        // Dump out all parameters
        for (int p = 0; p < func->cParams; p++)
        {
            fputs("    ", fidl);

            BOOL isIn = FALSE;
            BOOL isOut = FALSE;

            // Keywords
			if (func->lprgelemdescParam[p].idldesc.wIDLFlags & IDLFLAG_FRETVAL)
			{
				fputs("[retval] ", fidl);
			}
			if (func->lprgelemdescParam[p].idldesc.wIDLFlags & IDLFLAG_FIN &&
                func->lprgelemdescParam[p].idldesc.wIDLFlags & IDLFLAG_FOUT)
            {
                fputs("inout ", fidl);
            }
            else if (func->lprgelemdescParam[p].idldesc.wIDLFlags & IDLFLAG_FIN)
            {
                fputs("in ", fidl);
            }
            else if (func->lprgelemdescParam[p].idldesc.wIDLFlags & IDLFLAG_FOUT)
            {
                fputs("out ", fidl);
            }

            // Type
            TypeDesc tp(tiInterface, &func->lprgelemdescParam[p].tdesc);
            fprintf(fidl, "%s ", tp.ToXPIDLString().c_str());

            // Name
            fputs(W2A(rgbstrNames[p+1]), fidl);

            if (p < func->cParams - 1)
            {
                fprintf(fidl, ",\n");
            }
            else
            {
                fprintf(fidl, "\n");
            }
        	SysFreeString(rgbstrNames[0]);
        }
        fputs("  );\n", fidl);

        tiInterface->ReleaseFuncDesc(func);
    }


    // Fin
    fputs("};\n\n", fidl);

    tiInterface->ReleaseTypeAttr(attr);
}

static char *g_rgszVT[] =
{
	"void",             //VT_EMPTY           = 0,   /* [V]   [P]  nothing                     */
	"null",             //VT_NULL            = 1,   /* [V]        SQL style Null              */
	"short",            //VT_I2              = 2,   /* [V][T][P]  2 byte signed int           */
	"long",             //VT_I4              = 3,   /* [V][T][P]  4 byte signed int           */
	"single",           //VT_R4              = 4,   /* [V][T][P]  4 byte real                 */
	"double",           //VT_R8              = 5,   /* [V][T][P]  8 byte real                 */
	"CURRENCY",         //VT_CY              = 6,   /* [V][T][P]  currency                    */
	"DATE",             //VT_DATE            = 7,   /* [V][T][P]  date                        */
	"BSTR",             //VT_BSTR            = 8,   /* [V][T][P]  binary string               */
	"IDispatch*",       //VT_DISPATCH        = 9,   /* [V][T]     IDispatch FAR*              */
	"SCODE",            //VT_ERROR           = 10,  /* [V][T]     SCODE                       */
	"boolean",          //VT_BOOL            = 11,  /* [V][T][P]  True=-1, False=0            */
	"VARIANT",          //VT_VARIANT         = 12,  /* [V][T][P]  VARIANT FAR*                */
	"IUnknown*",        //VT_UNKNOWN         = 13,  /* [V][T]     IUnknown FAR*               */
	"wchar_t",          //VT_WBSTR           = 14,  /* [V][T]     wide binary string          */
	"",                 //                   = 15,
	"char",             //VT_I1              = 16,  /*    [T]     signed char                 */
	"unsigned char",             //VT_UI1             = 17,  /*    [T]     unsigned char               */
	"unsigned short",           //VT_UI2             = 18,  /*    [T]     unsigned short              */
	"unsigned long",            //VT_UI4             = 19,  /*    [T]     unsigned short              */
	"int64",            //VT_I8              = 20,  /*    [T][P]  signed 64-bit int           */
	"uint64",           //VT_UI8             = 21,  /*    [T]     unsigned 64-bit int         */
	"int",              //VT_INT             = 22,  /*    [T]     signed machine int          */
	"unsigned int",             //VT_UINT            = 23,  /*    [T]     unsigned machine int        */
	"void",             //VT_VOID            = 24,  /*    [T]     C style void                */
	"HRESULT",          //VT_HRESULT         = 25,  /*    [T]                                 */
	"PTR",              //VT_PTR             = 26,  /*    [T]     pointer type                */
	"SAFEARRAY",        //VT_SAFEARRAY       = 27,  /*    [T]     (use VT_ARRAY in VARIANT)   */
	"CARRAY",           //VT_CARRAY          = 28,  /*    [T]     C style array               */
	"USERDEFINED",      //VT_USERDEFINED     = 29,  /*    [T]     user defined type         */
	"LPSTR",            //VT_LPSTR           = 30,  /*    [T][P]  null terminated string      */
	"LPWSTR",           //VT_LPWSTR          = 31,  /*    [T][P]  wide null terminated string */
	"",                 //                   = 32,
	"",                 //                   = 33,
	"",                 //                   = 34,
	"",                 //                   = 35,
	"",                 //                   = 36,
	"",                 //                   = 37,
	"",                 //                   = 38,
	"",                 //                   = 39,
	"",                 //                   = 40,
	"",                 //                   = 41,
	"",                 //                   = 42,
	"",                 //                   = 43,
	"",                 //                   = 44,
	"",                 //                   = 45,
	"",                 //                   = 46,
	"",                 //                   = 47,
	"",                 //                   = 48,
	"",                 //                   = 49,
	"",                 //                   = 50,
	"",                 //                   = 51,
	"",                 //                   = 52,
	"",                 //                   = 53,
	"",                 //                   = 54,
	"",                 //                   = 55,
	"",                 //                   = 56,
	"",                 //                   = 57,
	"",                 //                   = 58,
	"",                 //                   = 59,
	"",                 //                   = 60,
	"",                 //                   = 61,
	"",                 //                   = 62,
	"",                 //                   = 63,
	"FILETIME",         //VT_FILETIME        = 64,  /*       [P]  FILETIME                    */
	"BLOB",             //VT_BLOB            = 65,  /*       [P]  Length prefixed bytes       */
	"STREAM",           //VT_STREAM          = 66,  /*       [P]  Name of the stream follows  */
	"STORAGE",          //VT_STORAGE         = 67,  /*       [P]  Name of the storage follows */
	"STREAMED_OBJECT",  //VT_STREAMED_OBJECT = 68,  /*       [P]  Stream contains an object   */
	"STORED_OBJECT",    //VT_STORED_OBJECT   = 69,  /*       [P]  Storage contains an object  */
	"BLOB_OBJECT",      //VT_BLOB_OBJECT     = 70,  /*       [P]  Blob contains an object     */
	"CF",               //VT_CF              = 71,  /*       [P]  Clipboard format            */
	"CLSID",            //VT_CLSID           = 72   /*       [P]  A Class ID                  */
};

std::string VTtoString(VARTYPE vt)
{
	std::string str;
	vt &= ~0xF000;
	if (vt <= VT_CLSID)
	   str = g_rgszVT[vt];
	else
	   str = "BAD VARTYPE";

	return str;
}




std::string TYPEDESCtoString(ITypeInfo* pti, TYPEDESC* ptdesc)
{
    std::string str;

	if (ptdesc->vt == VT_PTR)
	{
		// ptdesc->lptdesc points to a TYPEDESC that specifies the thing pointed to

		str = TYPEDESCtoString( pti, ptdesc->lptdesc );
		str += "*";
		return str;
	}

	if ((ptdesc->vt & 0x0FFF) == VT_CARRAY)
	{
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
	}

	if ((ptdesc->vt & 0x0FFF) == VT_SAFEARRAY)
	{
		str = "SAFEARRAY(" + TYPEDESCtoString( pti, ptdesc->lptdesc ) + ")";
		return str;
	}

	if (ptdesc->vt == VT_USERDEFINED)
	{
		// Use ptdesc->hreftype and pti->GetRefTypeInfo
		//
		ITypeInfo* ptiRefType = NULL;
		HRESULT hr = pti->GetRefTypeInfo( ptdesc->hreftype, &ptiRefType );
		if (SUCCEEDED(hr))
		{
			BSTR            bstrName = NULL;
			BSTR            bstrDoc = NULL;
			BSTR            bstrHelp = NULL;
			DWORD           dwHelpID;
			hr = ptiRefType->GetDocumentation( MEMBERID_NIL, &bstrName, &bstrDoc, &dwHelpID, &bstrHelp );
			if (FAILED(hr))
			{
                return std::string("ITypeInfo::GetDocumentation() failed in TYPEDESCToString");
			}

            USES_CONVERSION;
			str = W2A(bstrName);

			SysFreeString(bstrName);
			SysFreeString(bstrDoc);
			SysFreeString(bstrHelp);

			ptiRefType->Release();
		}
		else
		{
            return std::string("<GetRefTypeInfo failed>");
		}

		return str;
	}

    return VTtoString(ptdesc->vt);
}
