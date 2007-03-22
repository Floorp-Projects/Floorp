// tlb2xpt.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "xpt_xdr.h"

typedef void (*EnumTypeLibProc)(ITypeInfo *typeInfo, TYPEATTR *typeAttr);

void EnumTypeLib(ITypeLib *typeLib, EnumTypeLibProc pfn);
void EnumTypeLibProcVerbose(ITypeInfo *typeInfo, TYPEATTR *typeAttr);
void EnumTypeLibProcXPIDL(ITypeInfo *typeInfo, TYPEATTR *typeAttr);
void EnumTypeLibProcXPT(ITypeInfo *typeInfo, TYPEATTR *typeAttr);
void EnumTypeLibProcStubs(ITypeInfo *typeInfo, TYPEATTR *typeAttr);

void DumpXPCOMInterfaceXPT(FILE *f, ITypeInfo *tiInterface);
void DumpXPCOMInterfaceIDL(FILE *f, ITypeInfo *typeInfo);
void DumpXPCOMInterfaceStubH(FILE *f, ITypeInfo *tiInterface);
void DumpXPCOMInterfaceStubC(FILE *f, ITypeInfo *tiInterface);

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
    if (inputTLB == NULL || output == NULL)
    {
        fputs("Usage: tlb2xpt [-verbose] [-idl] [-stubs] [-xpt] typelibrary outputname\n"
              "  -verbose   Print out extra information\n"
              "  -idl       Generate an outputname.idl file\n"
              "  -xpt       Generate an outputname.xpt file\n"
              "  -stubs     Generate outputname.cpp and outputname.h stubs\n",
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
    if (genXPT)
    {
        if (output)
        {
            std::string filename(output);
            filename += ".xpy";
            fxpt = fopen(filename.c_str(), "wb");
        }
    }
    if (genStubs)
    {
        std::string stubh(output);
        std::string stubc(output);
        stubh += ".h";
        stubc += ".cpp";
        fstubh = fopen(stubh.c_str(), "wt");
        fstubc = fopen(stubc.c_str(), "wt");
    }

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

    if (verbose)
        EnumTypeLib(typeLib, EnumTypeLibProcVerbose);
    
    if (genIDL)
    {
        fputs("#include \"axIUnknown.idl\"\n\n", fidl);
        EnumTypeLib(typeLib, EnumTypeLibProcXPIDL);
    }

    if (genXPT)
        EnumTypeLib(typeLib, EnumTypeLibProcXPT);

    if (genStubs)
    {
        EnumTypeLib(typeLib, EnumTypeLibProcStubs);
    }

    return 0;
}



void EnumTypeLibProcVerbose(ITypeInfo *typeInfo, TYPEATTR *typeAttr)
{
    char *type;
    switch (typeAttr->typekind)
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
    fprintf(stderr, "Reading %s type\n", type);
}

void EnumTypeLibProcXPT(ITypeInfo *typeInfo, TYPEATTR *typeAttr)
{
    if (typeAttr->typekind == TKIND_INTERFACE)
    {
        DumpXPCOMInterfaceXPT(fxpt, typeInfo);
    }
}

void EnumTypeLibProcXPIDL(ITypeInfo *typeInfo, TYPEATTR *typeAttr)
{
    if (typeAttr->typekind == TKIND_INTERFACE)
    {
        DumpXPCOMInterfaceIDL(fidl, typeInfo);
    }
}


void EnumTypeLibProcStubs(ITypeInfo *typeInfo, TYPEATTR *typeAttr)
{
    if (typeAttr->typekind == TKIND_INTERFACE)
    {
        DumpXPCOMInterfaceStubH(fstubh, typeInfo);
        DumpXPCOMInterfaceStubC(fstubc, typeInfo);
    }
}

void EnumTypeLib(ITypeLib *typeLib, EnumTypeLibProc pfn)
{
    UINT count = typeLib->GetTypeInfoCount();
    for (UINT i = 0; i < count; i++)
    {
        ITypeInfoPtr typeInfo;
        typeLib->GetTypeInfo(i, &typeInfo);
        TYPEATTR *typeAttr = NULL;
        typeInfo->GetTypeAttr(&typeAttr);
        pfn(typeInfo, typeAttr);
        typeInfo->ReleaseTypeAttr(typeAttr);
    }
}


// [scriptable, uuid(00000000-0000-0000-0000-000000000000)]
// interface axIFoo : axIBar
void DumpXPCOMInterfaceXPT(FILE *f, ITypeInfo *tiInterface)
{
    XPTArena *arena = XPT_NewArena(1024 * 10, sizeof(double), "main xpt_link arena");
    // TODO. Maybe it would be better to just feed an IDL through the regular compiler
    //       than try and generate some XPT here.
    XPT_DestroyArena(arena);
}

void DumpXPCOMInterfaceStubH(FILE *f, ITypeInfo *tiInterface)
{
    HRESULT hr;

    USES_CONVERSION;
    BSTR bstrName = NULL;
    hr = tiInterface->GetDocumentation(MEMBERID_NIL, &bstrName, NULL, NULL, NULL);
    char *name = strdup(W2A(bstrName));
    SysFreeString(bstrName);

    fputs("template<class T>\n", f);
    fprintf(f, "class nsAX%sImpl : public ax%s\n", name, name);
    fputs("{\n}", f);

    for (char *c = name; *c; c++)
    {
        *c = toupper(*c);
    }
    fprintf(f, "  NS_DECL_AX%s\n", name);
    fputs("};\n\n", f);

    free(name);
}

void DumpXPCOMInterfaceStubC(FILE *f, ITypeInfo *tiInterface)
{

}

void DumpXPCOMInterfaceIDL(FILE *f, ITypeInfo *tiInterface)
{
    HRESULT hr;

    // [scriptable, uuid(00000000-0000-0000-0000-000000000000)]
    // interface axIFoo : axIBar
    // {
    //    void method1();
    // };

    TYPEATTR *attr;
    tiInterface->GetTypeAttr(&attr);

    USES_CONVERSION;

    //
    // Attribute block
    //
    fputs("[scriptable, ", f);

    // uuid()
	WCHAR szGUID[64];
	StringFromGUID2(attr->guid, szGUID, sizeof(szGUID));
    szGUID[0] = L'(';
    szGUID[wcslen(szGUID) - 1] = L')';
    fprintf(f, "uuid%s", W2A(szGUID));
    
    fputs("]\n", f);


    //
    // Interface block
    //

    fprintf(f, "interface ");

    BSTR bstrName = NULL;
    hr = tiInterface->GetDocumentation(MEMBERID_NIL, &bstrName, NULL, NULL, NULL);
    fprintf(f, "ax%s", W2A(bstrName));
    SysFreeString(bstrName);

    // Check for the base interface
	for (UINT n = 0; n <  attr->cImplTypes; n++)
	{
		HREFTYPE href = NULL;
		if (FAILED(hr = tiInterface->GetRefTypeOfImplType(n, &href)))
           ;  // TODO

        ITypeInfoPtr tiParent;
		if (FAILED(hr = tiInterface->GetRefTypeInfo(href, &tiParent)))
           ; // TODO

		if (FAILED(hr = tiParent->GetDocumentation(MEMBERID_NIL, &bstrName, NULL, NULL, NULL)))
           ; // TODO

		fprintf(f, " : ax%s", W2A(bstrName));

		SysFreeString(bstrName);
        bstrName = NULL;

		tiParent.Release();
	}


    //
    // Methods and properties block
    //

    fprintf(f, "\n");
    fprintf(f, "{\n");


    for (n = 0; n < attr->cFuncs; n++)
    {
        FUNCDESC *func;
        tiInterface->GetFuncDesc(n, &func);

        fprintf(f, "  ");
        if (func->invkind & INVOKE_PROPERTYPUT ||
            func->invkind & INVOKE_PROPERTYGET)
		{
		}

        // Return type
        TypeDesc tf(tiInterface, &func->elemdescFunc.tdesc);
        if (tf.mType == TypeDesc::T_RESULT)
        {
            fprintf(f, "void ");
        }
        else
        {
            fprintf(f, "%s ", tf.ToXPIDLString().c_str());
        }

        // Method / property name
        BSTR bstrName = NULL;
    	tiInterface->GetDocumentation(func->memid, &bstrName, NULL, NULL, NULL);
        fprintf(f, "%s (\n", W2A(bstrName));
        SysFreeString(bstrName);

        // Get the names of all the arguments
		UINT cNames;
	    BSTR rgbstrNames[100];
  		hr = tiInterface->GetNames(func->memid, rgbstrNames, 100, (UINT FAR*) &cNames);

        // Dump out all parameters
        for (int p = 0; p < func->cParams; p++)
        {
            fputs("    ", f);

            BOOL isIn = FALSE;
            BOOL isOut = FALSE;

            // Keywords
			if (func->lprgelemdescParam[p].idldesc.wIDLFlags & IDLFLAG_FRETVAL)
			{
				fputs("[retval] ", f);
			}
			if (func->lprgelemdescParam[p].idldesc.wIDLFlags & IDLFLAG_FIN &&
                func->lprgelemdescParam[p].idldesc.wIDLFlags & IDLFLAG_FOUT)
            {
                fputs("inout ", f);
            }
            else if (func->lprgelemdescParam[p].idldesc.wIDLFlags & IDLFLAG_FIN)
            {
                fputs("in ", f);
            }
            else if (func->lprgelemdescParam[p].idldesc.wIDLFlags & IDLFLAG_FOUT)
            {
                fputs("out ", f);
            }

            // Type
            // NOTE: If the arg is an out param, lop off the first pointer reference,
            //       because XPIDL implicitly expects out params to be pointers.
            TypeDesc tp(tiInterface, &func->lprgelemdescParam[p].tdesc);
            if (tp.mType == TypeDesc::T_POINTER &&
                func->lprgelemdescParam[p].idldesc.wIDLFlags & IDLFLAG_FOUT)
            {
                fprintf(f, "%s ", tp.mData.mPtr->ToXPIDLString().c_str());
            }
            else
            {
                // Type
                fprintf(f, "%s ", tp.ToXPIDLString().c_str());
            }

            // Name
            fputs(W2A(rgbstrNames[p+1]), f);

            if (p < func->cParams - 1)
            {
                fprintf(f, ",\n");
            }
            else
            {
                fprintf(f, "\n");
            }
        	SysFreeString(rgbstrNames[0]);
        }
        fputs("  );\n", f);

        tiInterface->ReleaseFuncDesc(func);
    }


    // Fin
    fputs("};\n\n", f);

    tiInterface->ReleaseTypeAttr(attr);
}
