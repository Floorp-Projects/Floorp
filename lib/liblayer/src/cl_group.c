/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* 
 *  cl_group.c - Layer group API
 */


#include "prtypes.h"
#include "plhash.h"
#include "xp.h"
#include "layers.h"
#include "cl_priv.h"


/* 
 * BUGBUG For now we use this local version to has hash group 
 * names. Use the canned NSPR function when it gets checked in.
 */
static PRHashNumber
cl_hash_string(const void *key)
{
    PRHashNumber h;
    const unsigned char *s;

    h = 0;
    for (s = key; *s; s++)
        h = (h >> 28) ^ (h << 4) ^ *s;
    return h;
}

static int
cl_compare_strings(const void *key1, const void *key2)
{
    return strcmp(key1, key2) == 0;
}

static int
cl_compare_values(const void *value1, const void *value2)
{
    return value1 == value2;
}

PRBool 
CL_AddLayerToGroup(CL_Compositor *compositor,
                   CL_Layer *layer,
                   char *name)
{
    XP_List *group_list;
    PRBool ret_val = PR_FALSE;

    XP_ASSERT(compositor);
    XP_ASSERT(layer);
    XP_ASSERT(name);

    if (!compositor || !layer || !name)
        return PR_FALSE;
    
    if (compositor->group_table == NULL){
        compositor->group_table = PR_NewHashTable(CL_GROUP_HASH_TABLE_SIZE,
                                                  cl_hash_string,
                                                  cl_compare_strings,
                                                  cl_compare_values,
                                                  NULL, NULL);

        if (compositor->group_table == NULL)
            return PR_FALSE;
    }
    
    if ((group_list = (XP_List *)PR_HashTableLookup(compositor->group_table, 
                                                    name)) == NULL){
        group_list = XP_ListNew();
        if (group_list == NULL)
            return PR_FALSE;
        
        PR_HashTableAdd(compositor->group_table, name, group_list);
        ret_val = PR_TRUE;
    }
    
    XP_ListAddObject(group_list, layer);
    
    return ret_val;
}

void
CL_RemoveLayerFromGroup(CL_Compositor *compositor,
                        CL_Layer *layer,
                        char *name)
{
    XP_List *group_list;
    
    XP_ASSERT(compositor);
    XP_ASSERT(layer);
    XP_ASSERT(name);
    
    if (!compositor || !layer || !name)
        return;
    
    if (compositor->group_table == NULL)
        return;
    
    if ((group_list = (XP_List *)PR_HashTableLookup(compositor->group_table, 
                                                    name)) == NULL)
        return;
    
    /* Remove the layer from the group list */
    XP_ListRemoveObject(group_list, layer);

    /* If the list is empty, get rid of it */
    if (XP_ListIsEmpty(group_list)){
        PR_HashTableRemove(compositor->group_table, name);
        XP_ListDestroy(group_list);
    }
}

void
CL_DestroyLayerGroup(CL_Compositor *compositor,
                     char *name)
{
    XP_List *group_list;
    
    XP_ASSERT(compositor);
    XP_ASSERT(name);
 
    if (!compositor || !name)
        return;
    
    if (compositor->group_table == NULL)
        return;
    
    if ((group_list = (XP_List *)PR_HashTableLookup(compositor->group_table, 
                                                    name)) == NULL)
        return;
    
    /* Get rid of the list */
    PR_HashTableRemove(compositor->group_table, name);
    XP_ListDestroy(group_list);
}

    
/* 
 * BUGBUG CL_HideLayerGroup, CL_UnhideLayerGroup and 
 * CL_OffsetLayerGroup can all be implemented using
 * CL_ForEachLayerInGroup...and they should be. Cutting
 * and pasting was just the easy way out.
 */
void
CL_HideLayerGroup(CL_Compositor *compositor, char *name)
{
    XP_List *group_list;
    CL_Layer *layer;
    
    XP_ASSERT(compositor);
    XP_ASSERT(name);
    
    if (!compositor || !name)
        return;
    
    if (compositor->group_table == NULL)
        return;

    group_list = (XP_List *)PR_HashTableLookup(compositor->group_table, 
                                               name);
    if (group_list == 0)
        return;
    
    while ((layer = XP_ListNextObject(group_list)))
        CL_SetLayerHidden(layer, PR_TRUE);
}

void 
CL_OffsetLayerGroup(CL_Compositor *compositor, char *name, 
                    int32 x_offset, int32 y_offset)
{
    XP_List *group_list;
    CL_Layer *layer;
    
    XP_ASSERT(compositor);
    XP_ASSERT(name);
    
    if (!compositor || !name)
        return;
    
    if (compositor->group_table == NULL)
        return;

    group_list = (XP_List *)PR_HashTableLookup(compositor->group_table, 
                                               name);
    if (group_list == 0)
        return;
    
    while ((layer = XP_ListNextObject(group_list)))
        CL_OffsetLayer(layer, x_offset, y_offset);
}

void
CL_UnhideLayerGroup(CL_Compositor *compositor, char *name)
{
    XP_List *group_list;
    CL_Layer *layer;
    
    XP_ASSERT(compositor);
    XP_ASSERT(name);
    
    if (!compositor || !name)
        return;

    if (compositor->group_table == NULL)
        return;

    group_list = (XP_List *)PR_HashTableLookup(compositor->group_table, 
                                               name);
    if (group_list == 0)
        return;
    
    while ((layer = XP_ListNextObject(group_list)))
        CL_SetLayerHidden(layer, PR_FALSE);
}


PRBool
CL_ForEachLayerInGroup(CL_Compositor *compositor,
                       char *name,
                       CL_LayerInGroupFunc func,
                       void *closure)
{
    PRBool done;
    XP_List *group_list;
    CL_Layer *layer;
    
    XP_ASSERT(compositor);
    XP_ASSERT(name);
    
    if (!compositor || !name)
        return PR_FALSE;

    if (compositor->group_table == NULL)
        return PR_TRUE;

    group_list = (XP_List *)PR_HashTableLookup(compositor->group_table, 
                                               name);
    if (group_list == 0)
        return PR_TRUE;
    
    while ((layer = XP_ListNextObject(group_list))) {
        done = (PRBool)!(*func)(layer, closure);
        if (done) return PR_FALSE;
    }
    
    return PR_TRUE;
}
