/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JavaScript Debugging support - Atom support
 */

#include "jsd.h"

/* #define TEST_ATOMS 1 */

#ifdef TEST_ATOMS
static void 
_testAtoms(JSDContext*jsdc)
{
    JSDAtom* atom0 = jsd_AddAtom(jsdc, "foo");
    JSDAtom* atom1 = jsd_AddAtom(jsdc, "foo");
    JSDAtom* atom2 = jsd_AddAtom(jsdc, "bar");
    JSDAtom* atom3 = jsd_CloneAtom(jsdc, atom1);
    JSDAtom* atom4 = jsd_CloneAtom(jsdc, atom2);

    const char* c0 = JSD_ATOM_TO_STRING(atom0);
    const char* c1 = JSD_ATOM_TO_STRING(atom1);
    const char* c2 = JSD_ATOM_TO_STRING(atom2);
    const char* c3 = JSD_ATOM_TO_STRING(atom3);
    const char* c4 = JSD_ATOM_TO_STRING(atom4);

    jsd_DropAtom(jsdc, atom0);
    jsd_DropAtom(jsdc, atom1);
    jsd_DropAtom(jsdc, atom2);
    jsd_DropAtom(jsdc, atom3);
    jsd_DropAtom(jsdc, atom4);
}        
#endif    

static int
_atom_smasher(JSHashEntry *he, int i, void *arg)
{
    MOZ_ASSERT(he);
    MOZ_ASSERT(he->value);
    MOZ_ASSERT(((JSDAtom*)(he->value))->str);

    free(((JSDAtom*)(he->value))->str);
    free(he->value);
    he->value = nullptr;
    he->key   = nullptr;
    return HT_ENUMERATE_NEXT;
}

static int
_compareAtomKeys(const void *v1, const void *v2)
{
    return 0 == strcmp((const char*)v1, (const char*)v2);
}        

static int
_compareAtoms(const void *v1, const void *v2)
{
    return 0 == strcmp(((JSDAtom*)v1)->str, ((JSDAtom*)v2)->str);
}        


bool
jsd_CreateAtomTable(JSDContext* jsdc)
{
    jsdc->atoms = JS_NewHashTable(256, JS_HashString,
                                  _compareAtomKeys, _compareAtoms,
                                  nullptr, nullptr);
#ifdef TEST_ATOMS
    _testAtoms(jsdc);
#endif    
    return !!jsdc->atoms;
}

void
jsd_DestroyAtomTable(JSDContext* jsdc)
{
    if( jsdc->atoms )
    {
        JS_HashTableEnumerateEntries(jsdc->atoms, _atom_smasher, nullptr);
        JS_HashTableDestroy(jsdc->atoms);
        jsdc->atoms = nullptr;
    }
}

JSDAtom*
jsd_AddAtom(JSDContext* jsdc, const char* str)
{
    JSDAtom* atom;

    if(!str)
    {
        MOZ_ASSERT(0);
        return nullptr;
    }

    JSD_LOCK_ATOMS(jsdc);
    
    atom = (JSDAtom*) JS_HashTableLookup(jsdc->atoms, str);

    if( atom )
        atom->refcount++;
    else
    {
        atom = (JSDAtom*) malloc(sizeof(JSDAtom));
        if( atom )
        {
            atom->str = strdup(str);
            atom->refcount = 1;
            if(!JS_HashTableAdd(jsdc->atoms, atom->str, atom))
            {
                free(atom->str);
                free(atom);
                atom = nullptr;
            }
        }
    }

    JSD_UNLOCK_ATOMS(jsdc);
    return atom;
}        

JSDAtom*
jsd_CloneAtom(JSDContext* jsdc, JSDAtom* atom)
{
    JSD_LOCK_ATOMS(jsdc);
    atom->refcount++;
    JSD_UNLOCK_ATOMS(jsdc);
    return atom;
}        

void
jsd_DropAtom(JSDContext* jsdc, JSDAtom* atom)
{
    JSD_LOCK_ATOMS(jsdc);
    if(! --atom->refcount)
    {
        JS_HashTableRemove(jsdc->atoms, atom->str);
        free(atom->str);
        free(atom);
    }
    JSD_UNLOCK_ATOMS(jsdc);
}        

