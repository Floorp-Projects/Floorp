/*
 * The contents of this file are subject to the Mozilla Public License 
 * Version 1.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at 
 * http://www.mozilla.org/MPL/ 
 *
 * Software distributed under the License is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for 
 * the specific language governing rights and limitations under the License.
 *
 * Contributors:
 *    Frank Mitchell (frank.mitchell@sun.com)
 */
#include <iostream.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "nscore.h" 
#include "nsIFactory.h" 
#include "nsIComponentManager.h" 
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "xptinfo.h"
#include "xptcall.h"
#include "xpt_struct.h"
#include "nsIAllocator.h"
#include "JSISample.h" 
#include "JSSample.h" 

#include "nsSpecialSystemDirectory.h" 


static NS_DEFINE_IID(kISampleIID, JSISAMPLE_IID); 
static NS_DEFINE_CID(kSampleCID, JS_SAMPLE_CID); 

#ifdef XP_PC
#define SAMPLE_DLL "xpjtest.dll"
#else
#ifdef XP_MAC
#define SAMPLE_DLL "XPJTEST_DLL"
#else
#define SAMPLE_DLL "libxpjtest.so"
#endif
#endif


static void ParamsFromArgv(nsXPTCVariant *result, 
			   const nsXPTMethodInfo *mi,
			   int argi, int argc, char **argv) {
  uint8 paramcount = mi->GetParamCount();

  memset(result, 0, sizeof(nsXPTCVariant) * paramcount);

  for (int i = 0; i < paramcount; i++) {
    nsXPTParamInfo param = mi->GetParam(i);

    result[i].ptr   = 0;
    result[i].type  = param.GetType().TagPart();
    result[i].flags = 0;

    if (param.IsIn()) {
      assert(argi < argc);

      cerr << "Consuming " << argv[argi] << endl;

      switch(result[i].type) {
      case nsXPTType::T_I8:
	result[i].val.i8 = atoi(argv[argi]);
	break;
      case nsXPTType::T_I16:
	result[i].val.i16 = atoi(argv[argi]);
	break;
      case nsXPTType::T_I32:
	result[i].val.i32 = atol(argv[argi]);
	break;
      case nsXPTType::T_I64:
	result[i].val.i64 = atol(argv[argi]); //s.b. conversion to long long
	break;
      case nsXPTType::T_U8:
	result[i].val.u8 = atoi(argv[argi]);
	break;
      case nsXPTType::T_U16:
	result[i].val.u16 = atoi(argv[argi]);
	break;
      case nsXPTType::T_U32:
	result[i].val.u32 = atol(argv[argi]);
	break;
      case nsXPTType::T_U64:
	result[i].val.u64 = atol(argv[argi]); //s.b. conversion to long long
	break;
      case nsXPTType::T_FLOAT:
	result[i].val.f = atof(argv[argi]);
	break;
      case nsXPTType::T_DOUBLE:
	result[i].val.d = atof(argv[argi]);
	break;
      case nsXPTType::T_BOOL:
	switch (argv[argi][0]) {
	case '1':
	case 't':
	case 'T':
	  result[i].val.b = 1;
	  break;
	default:
	  result[i].val.b = 0;
	  break;
	}
	break;
      case nsXPTType::T_CHAR:
	result[i].val.c = argv[argi][0];
	break;
      case nsXPTType::T_WCHAR:
	result[i].val.wc = argv[argi][0];   // PENDING: not wide
	break;
      case nsXPTType::T_CHAR_STR:
	result[i].val.p = argv[argi];
	// Copying every time would be wasteful
	if (param.IsOut()) {
	  char *tmpstr = new char[strlen(argv[argi]) + 1];
	  strcpy(tmpstr, argv[argi]);
	  result[i].val.p = tmpstr;
	  result[i].flags |= nsXPTCVariant::VAL_IS_OWNED;
	}
	break;
      case nsXPTType::T_VOID:
      case nsXPTType::T_IID:
      case nsXPTType::T_BSTR:
      case nsXPTType::T_WCHAR_STR:
      case nsXPTType::T_INTERFACE:
      case nsXPTType::T_INTERFACE_IS:
	// Ignore for now
	break;
      }
      // VAL_IS_OWNED: val.p holds alloc'd ptr that must be freed
      // VAL_IS_IFACE: val.p holds interface ptr that must be released

      argi++;
    }

    if (param.IsOut()) {
     // PTR_IS_DATA: ptr points to 'real' data in val
     result[i].flags |= nsXPTCVariant::PTR_IS_DATA;

      switch(result[i].type) {
      case nsXPTType::T_I8:
	result[i].ptr = &result[i].val.i8;
	break;
      case nsXPTType::T_I16:
	result[i].ptr = &result[i].val.i16;
	break;
      case nsXPTType::T_I32:
	result[i].ptr = &result[i].val.i32;
	break;
      case nsXPTType::T_I64:
	result[i].ptr = &result[i].val.i64;
	break;
      case nsXPTType::T_U8:
	result[i].ptr = &result[i].val.u8;
	break;
      case nsXPTType::T_U16:
	result[i].ptr = &result[i].val.u16;
	break;
      case nsXPTType::T_U32:
	result[i].ptr = &result[i].val.u32;
	break;
      case nsXPTType::T_U64:
	result[i].ptr = &result[i].val.u64;
	break;
      case nsXPTType::T_FLOAT:
	result[i].ptr = &result[i].val.f;
	break;
      case nsXPTType::T_DOUBLE:
	result[i].ptr = &result[i].val.d;
	break;
      case nsXPTType::T_BOOL:
	result[i].ptr = &result[i].val.b;
	break;
      case nsXPTType::T_CHAR:
	result[i].ptr = &result[i].val.c;
	break;
      case nsXPTType::T_WCHAR:
	result[i].ptr = &result[i].val.wc;
	break;
      default:
	result[i].ptr = &result[i].val.p;
	break;
      }
    }
  }
}


static void xpjd_PrintParams(const nsXPTCVariant params[], int paramcount) {
    for (int i = 0; i < paramcount; i++) {

        cerr << i << ") ";

        switch(params[i].type) {
        case nsXPTType::T_I8:
            cerr << params[i].val.i8;
            break;
        case nsXPTType::T_I16:
            cerr << params[i].val.i16;
            break;
        case nsXPTType::T_I32:
            cerr << params[i].val.i32;
            break;
        case nsXPTType::T_I64:
            cerr << params[i].val.i64;
            break;
        case nsXPTType::T_U8:
            cerr << params[i].val.u8;
            break;
        case nsXPTType::T_U16:
            cerr << params[i].val.u16;
            break;
        case nsXPTType::T_U32:
            cerr << params[i].val.u32;
            break;
        case nsXPTType::T_U64:
            cerr << params[i].val.u64;
            break;
        case nsXPTType::T_FLOAT:
            cerr <<       params[i].val.f;
            break;
        case nsXPTType::T_DOUBLE:
            cerr << params[i].val.d;
            break;
        case nsXPTType::T_BOOL:
            cerr << (params[i].val.b ? "true" : "false");
            break;
        case nsXPTType::T_CHAR:
            cerr << "'" << params[i].val.c << "'";
            break;
        case nsXPTType::T_WCHAR:
            cerr << "'" << params[i].val.wc << "'";
            break;
        case nsXPTType::T_CHAR_STR:
            cerr << params[i].val.p << ' '
                 << '"' << (char *)params[i].val.p << '"';
            break;
        default:
            // Ignore for now
            break;
        }
        cerr << " : type " << (int)(params[i].type)
             << ", ptr=" << params[i].ptr
             << (params[i].IsPtrData() ?      ", data" : "")
             << (params[i].IsValOwned() ?     ", owned" : "")
             << (params[i].IsValInterface() ? ", interface" : "")
             << endl;
    }
}


static nsresult GetMethodInfoByName(const nsID *iid, 
				    const char *methodname, 
				    PRBool wantSetter,
				    const nsXPTMethodInfo **miptr,
				    int *_retval) {
    nsresult res;
    nsIInterfaceInfoManager *iim;
    nsIInterfaceInfo *info = nsnull;

    // Get info
    iim = XPTI_GetInterfaceInfoManager();

    if (!iim) {
        cerr << "Failed to find InterfaceInfoManager" << endl;
        return NS_ERROR_NOT_INITIALIZED;
    }

    res = iim->GetInfoForIID(iid, &info);

    if (NS_FAILED(res)) {
        cerr << "Failed to find info for " << iid->ToString() << endl;
        return res;
    }

    // Find info for command
    uint16 methodcount;

    info->GetMethodCount(&methodcount);

    int i;
    for (i = 0; i < methodcount; i++) {
        const nsXPTMethodInfo *mi;

        info->GetMethodInfo(i, &mi);
        // PENDING(frankm): match against name, get/set, *AND* param types
        if (strcmp(methodname, mi->GetName()) == 0) {
            PRBool setter = mi->IsSetter();
            PRBool getter = mi->IsGetter();
            if ((!getter && !setter) 
                || (setter && wantSetter)
                || (getter && !wantSetter)) {
                *miptr = mi;
                *_retval = i;
                break;
            }
        }
    }
    if (i >= methodcount) {
        cerr << "Failed to find " << methodname << endl;
        *miptr = NULL;
        *_retval = -1;
        return NS_ERROR_FAILURE;
    }
    return res;
}


int main(int argc, char **argv) 
{
    JSISample *sample; 
    nsresult res;
    char *commandName;

    cout << "Starting ..." << endl;

    // Get options

    // Get command vector
    if (argc > 1) {
	commandName = argv[1];
    }
    else {
	commandName = "PrintStats";
    }

    // Initialize XPCOM
    nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, nsnull);

    // Create Instance
    res = nsComponentManager::CreateInstance(kSampleCID,
					     nsnull, 
					     kISampleIID, 
					     (void **) &sample); 

    if (NS_FAILED(res)) {
	cerr << "Failed to create instance" << endl;
	return 1;
    }

    // Get info
    int index;
    const nsXPTMethodInfo *mi;

    res = GetMethodInfoByName(&kISampleIID, commandName, PR_FALSE,
			      &mi, &index);

    if (NS_FAILED(res)) {
	cerr << "Failed to find " << commandName << endl;
	return 1;
    }

    uint8 paramcount = mi->GetParamCount();

    cerr << mi->GetName() 
	 << ":" << endl;

    for (int i = 0; i < paramcount; i++) {
	nsXPTParamInfo param = mi->GetParam(i);
	nsXPTType type = param.GetType();
	
	cerr << '\t' << i << ": " 
	     << " type = " << (int)type.TagPart()
	     << (type.IsUniquePointer() ? " UNIQUE" : "") 
	     << (type.IsReference() ?     " REF" : "") 
	     << (type.IsPointer() ?       " POINTER" : "") 
	     << (param.IsIn() ?         ", in" : "") 
	     << (param.IsOut() ?        ", out" : "")
	     << (param.IsRetval() ?     ", retval" : "")
	     << (param.IsShared() ?     ", shared" : "")
	     << endl;
    }

    // Translate and marshall arguments
    nsXPTCVariant params[32]; 

    if (paramcount > (sizeof(params)/sizeof(nsXPTCVariant))) {
	cerr << "Too Many Params" << endl;
	return 1;
    }

    ParamsFromArgv(params, mi, 2, argc, argv);

    cerr << "Arguments are: " << endl;

    xpjd_PrintParams(params, paramcount);

    // Invoke command

    cerr << "Calling XPTC_InvokeByIndex( " << sample 
	 << ", " << (int)index
	 << ", " << (int)paramcount
	 << ", " << params
	 << ")" << endl;

    res = XPTC_InvokeByIndex(sample, 
			     (PRUint32)index,
			     (PRUint32)paramcount, 
			     params);

    // Get result

    cerr << "Results are: " << endl;

    xpjd_PrintParams(params, paramcount);

    // Release Instance
    NS_RELEASE(sample); 

    return 0; 
}
