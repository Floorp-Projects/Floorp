/* The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 */

/* 
   This is part of a MOZ_COVERAGE build.  (set MOZ_COVERAGE=1 and rebuild)
   When trace.dll is linked in to the build, it counts the number of times 
   each function is called.  When the program exits, it breaks out the 
   functions by module, sorts them by number of calls, then dumps them into 
   win32.order where the module is built.  The order files are used by the
   liker to rearrange the functions in the library.  
*/

#include <windows.h>
#include <imagehlp.h>
#include <stdio.h>

#include "pldhash.h"

class Reporter {
public:
    ~Reporter();
};

static Reporter theReporter;
static FILE* logfile;

/* 
   Hash of function names, and call counts]
 */
static PLDHashTable Calls;

struct CallEntry {
    struct PLDHashEntryHdr hdr;
    const void*            addr;
    unsigned               count;
};

static PLDHashTableOps Ops = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    PL_DHashGetKeyStub,
    PL_DHashVoidPtrKeyStub,
    PL_DHashMatchEntryStub,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub
};

class Node {
public:
    Node() {function = 0; count = 0; next = 0;};
    char* function;
    int   count;
    Node* next;
};


/* 
   Hash of each module.  Contains a sorted linked list of each function and
   its number of calls for that module.
*/

static PLDHashTable Modules;

struct ModulesEntry {
    struct PLDHashEntryHdr hdr;
    char*                  moduleName;
    Node*                  byCount;
};

BOOL PR_CALLBACK
ModuleMatchEntry(PLDHashTable* aTable, 
                 const PLDHashEntryHdr* aEntry, 
                 const void* aKey)
{
    ModulesEntry* mod = (ModulesEntry*) aEntry;
    return ( !strcmp(mod->moduleName,(char*)aKey) );
}

static PLDHashTableOps ModOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    PL_DHashGetKeyStub,
    PL_DHashStringKey,
    ModuleMatchEntry,  // PL_DHashMatchEntryStub,  
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub
};


/*
   Counts the number of times a function is called.
*/
extern "C"
static void
Log(void* addr)
{
static int initialized = 0;

    addr = (void*) ((unsigned) addr - 5);

    if (! initialized) {
        initialized = 1;
        PL_DHashTableInit(&Calls, &Ops, 0, sizeof(CallEntry), 16);
    }

    CallEntry* entry
        = (CallEntry*) PL_DHashTableOperate(&Calls, addr, PL_DHASH_LOOKUP);
    if (PL_DHASH_ENTRY_IS_FREE(&entry->hdr)) {
        entry = (CallEntry*) PL_DHashTableOperate(&Calls, addr, PL_DHASH_ADD);
        entry->addr = addr;
        entry->count = 0;
    }

    ++entry->count;
}

/*
   assembly to call Log, and count this function
*/

extern "C"
__declspec(naked dllexport)
void _penter()
{
    __asm {
        push ecx               // save ecx for caller
        push dword ptr [esp+4] // the caller's address is the only param
        call Log               // ...to Log()
        add  esp,4
        pop  ecx               // restore ecx for caller
        ret
    }
}

/*
   Steps through the hash of modules and dumps the function counts out to 
   win32.order
*/

static PLDHashOperator PR_CALLBACK
DumpFiles(PLDHashTable* table, PLDHashEntryHdr* hdr,
          PRUint32 number, void* arg)
{
    ModulesEntry* entry = (ModulesEntry*) hdr;    
    Node*         cur = entry->byCount;
    char          dest[MAX_PATH];
    char          pdbName[MAX_PATH];
    FILE*         orderFile;

    strcpy(pdbName, entry->moduleName);
    strcat(pdbName, ".pdb");

    if (!::SearchTreeForFile(MOZ_SRC, pdbName, dest) ) {
        fprintf(logfile,"+++ERROR Could not find %s\n",pdbName);
        return PL_DHASH_NEXT;
    }
    dest[strlen(dest)-strlen(pdbName)-strlen("WIN32_D.OBJ\\")] = 0;
    strcat(dest,"win32.order");
    orderFile = fopen(dest,"w");
    fprintf(logfile,"Creating order file %s\n",dest);
    
    while (cur) {
        if (cur->function[0] == '_')  // demangle "C" style function names
            fprintf(orderFile,"%s ; %d\n", cur->function+1, cur->count );
        else
            fprintf(orderFile,"%s ; %d\n", cur->function, cur->count );
        cur = cur->next;
    }

    fflush(orderFile);
    fclose(orderFile);
    return PL_DHASH_NEXT;
}

/*
   We have a function name.  Figure out which module it is from.  Then add that
   function and its call count into the module's sorted list.
*/

static PLDHashOperator PR_CALLBACK
ListCounts(PLDHashTable* table, PLDHashEntryHdr* hdr,
           PRUint32 number, void* arg)
{
    BOOL ok;
    CallEntry* entry = (CallEntry*) hdr;

    IMAGEHLP_MODULE module;
    module.SizeOfStruct = sizeof(module);

    ok = ::SymGetModuleInfo(::GetCurrentProcess(),
                            (unsigned) entry->addr,
                            &module);

    char buf[sizeof(IMAGEHLP_SYMBOL) + 512];
    PIMAGEHLP_SYMBOL symbol = (PIMAGEHLP_SYMBOL) buf;
    symbol->SizeOfStruct = sizeof(buf);
    symbol->MaxNameLength = 512;

    DWORD displacement;
    ok = ::SymGetSymFromAddr(::GetCurrentProcess(),
                             (unsigned) entry->addr,
                             &displacement,
                             symbol);

    if (ok)
    {
        if (displacement > 0) 
            return PL_DHASH_NEXT;
        static int modInitialized = 0;
        if (! modInitialized) {
            modInitialized = 1;
            PL_DHashTableInit(&Modules, &ModOps, 0, sizeof(ModulesEntry), 16);
        }

        ModulesEntry* mod
            = (ModulesEntry*) PL_DHashTableOperate(&Modules, 
                                                   module.ModuleName, 
                                                   PL_DHASH_LOOKUP);

        if (PL_DHASH_ENTRY_IS_FREE(&mod->hdr)) {
            mod = (ModulesEntry*) PL_DHashTableOperate(&Modules,
                                                       module.ModuleName,
                                                       PL_DHASH_ADD);
            mod->moduleName = strdup(module.ModuleName);
            mod->byCount = new Node();
            mod->byCount->function = strdup(symbol->Name);
            mod->byCount->count = entry->count;
        } else {
            // insertion sort.
            Node* cur = mod->byCount;
            Node* foo = new Node();
            foo->function = strdup(symbol->Name);
            foo->count = entry->count;

            if (cur->count < entry->count) {
                if (!strcmp(cur->function,symbol->Name)) 
                    return PL_DHASH_NEXT;
                foo->next = mod->byCount;
                mod->byCount = foo;                
            } else {
                while (cur->next) {
                    if (!strcmp(cur->function,symbol->Name)) 
                        return PL_DHASH_NEXT;
                    if (cur->next->count > entry->count) { cur = cur->next; }
                    else { break; }
                }
                foo->next = cur->next;  
                cur->next = foo;
            }
        }
    } // if (ok)
    return PL_DHASH_NEXT;
}

Reporter::~Reporter()
{
    SymInitialize(GetCurrentProcess(), 0, TRUE);
    
    DWORD options = SymGetOptions();

    // We want the nasty name, as we'll have to pass it back to the
    // linker.
    options &= ~SYMOPT_UNDNAME;
    SymSetOptions(options);

    char logName[MAX_PATH];
    strcpy(logName,MOZ_SRC);
    strcat(logName,"\\tracelog");
    logfile = fopen(logName,"w");

    // break the function names out by module and sort them.
    PL_DHashTableEnumerate(&Calls, ListCounts, NULL);
    // dump the order files for each module.
    PL_DHashTableEnumerate(&Modules, DumpFiles, NULL);

    fclose(logfile);
    SymCleanup(GetCurrentProcess());
}
