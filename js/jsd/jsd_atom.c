/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

static intN
_atom_smasher(JSHashEntry *he, intN i, void *arg)
{
    JS_ASSERT(he);
    JS_ASSERT(he->value);
    JS_ASSERT(((JSDAtom*)(he->value))->str);

    free(((JSDAtom*)(he->value))->str);
    free(he->value);
    he->value = NULL;
    he->key   = NULL;
    return HT_ENUMERATE_NEXT;
}

static intN
_compareAtomKeys(const void *v1, const void *v2)
{
    return 0 == strcmp((const char*)v1, (const char*)v2);
}        

static intN
_compareAtoms(const void *v1, const void *v2)
{
    return 0 == strcmp(((JSDAtom*)v1)->str, ((JSDAtom*)v2)->str);
}        


JSBool
jsd_CreateAtomTable(JSDContext* jsdc)
{
    jsdc->atoms = JS_NewHashTable(256, JS_HashString,
                                  _compareAtomKeys, _compareAtoms,
                                  NULL, NULL);
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
        JS_HashTableEnumerateEntries(jsdc->atoms, _atom_smasher, NULL);
        JS_HashTableDestroy(jsdc->atoms);
        jsdc->atoms = NULL;
    }
}

JSDAtom*
jsd_AddAtom(JSDContext* jsdc, const char* str)
{
    JSDAtom* atom;

    if(!str)
    {
        JS_ASSERT(0);
        return NULL;
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
                atom = NULL;
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

