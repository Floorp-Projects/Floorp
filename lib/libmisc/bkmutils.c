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

#include "xp_mcom.h"
#include "client.h"
#include "hotlist.h"

static int32 hot_measure_Separator(HotlistStruct * item, int bLongFormat, int nIndent);


#define TEXT_INDENT 3
#define SEPATATOR_COUNT 20
/*
 * Measure a boring URL hotlist entry and return the length in bytes
 *
 * Short format:
 *  "     item->address\n"
 *    where there are nIndent number of spaces before the item
 *
 * Long format:
 *  int16 type
 *  item->name\n
 *  item->address\n
 *  int32 addition_date
 *  int32 last_visit_date
 *  item->description\0
 *
 * The item->description field is *NOT* \n terminated since it might
 *  be a multi-line string and is therefore \0 terminated
 */
PRIVATE int32
hot_measure_URL(HotlistStruct * item, int bLongFormat, int nIndent)
{

    int32 iSpace = 0;

    if(!item)
        return(0);

    if(bLongFormat) {

        /* type, addition_date, last_visit_date + 2 \n's + 1 \0 */
        iSpace += 2 + 4 + 4 + 2 + 1;

        if(item->name)
            iSpace += XP_STRLEN(item->name);
        if(item->description)
            iSpace += XP_STRLEN(item->description);

    } else {

        /* space indentation and '\n' terminator */
        iSpace = 1 + nIndent;

    }

    /* the address appears in both formats */
    if(item->address)
        iSpace += XP_STRLEN(item->address);

    return(iSpace);

}

/*
 * Measure a header entry and all its children
 *
 * Short format:
 *  "     item->name\n"
 *  "        child1->address\n"
 *  "        child2->address\n"
 *    where there are nIndent number of spaces before the item and
 *    TEXT_INDENT spaces between levels
 *
 * Long format:
 *  int16 type
 *  item->name\n
 *  int32 addition_date
 *  int32 number of children
 *  item->description\0
 *
 * The item->description field is *NOT* \n terminated since it might
 *  be a multi-line string and is therefore \0 terminated.  Note that
 *  the address field is *NOT* written for headers since its it meaningless
 */
PRIVATE int32
hot_measure_Header(HotlistStruct * item, int bLongFormat, int nIndent)
{

    XP_List * list;
    int32 iSpace = 0;

    if(!item)
        return(0);

    if(bLongFormat) {

        /* type, addition_date, last_visit_date + # children + 2 \n's + 1 \0 */
        iSpace += 2 + 4 + 4 + 2 + 1;

        if(item->description)
            iSpace += XP_STRLEN(item->description);

    } else {

        /* space indentation and '\n' terminator */
        iSpace = 1 + nIndent;

    }

    /* the name appears in both formats */
    if(item->name)
        iSpace += XP_STRLEN(item->name);


    /* if no children just return */
    if(!item->children)
        return(iSpace);

    /* measure the amount of space taken up by this item's children */
    for(list = item->children->next; list; list = list->next) {
        HotlistStruct * child = (HotlistStruct *) list->object;

        if(!child)
            continue;

        switch(child->type) {
        case HOT_URLType:
            iSpace += hot_measure_URL(child, bLongFormat, nIndent + TEXT_INDENT);
            break;
        case HOT_HeaderType:
            iSpace += hot_measure_Header(child, bLongFormat, nIndent + TEXT_INDENT); 
            break;
        case HOT_SeparatorType:
            iSpace += hot_measure_Separator(child, bLongFormat, nIndent + TEXT_INDENT); 
            break;
        default:
            break;
        }

    }

    return(iSpace);

}


/*
 * Measure a separator entry and return the length in bytes
 *
 * Short format:
 *  "     ---------------------\n"
 *    where there are nIndent number of spaces before the item
 *    and 20 dashes
 *
 * Long format:
 *  int16 type
 */
PRIVATE int32
hot_measure_Separator(HotlistStruct * item, int bLongFormat, int nIndent)
{

    int32 iSpace = 0;

    if(!item)
        return(0);

    if(bLongFormat) {

        /* type */
        iSpace = 2;

    } else {

        /* space indentation and '\n' terminator */
        iSpace = 1 + nIndent + 20;

    }

    return(iSpace);

}


/*
 * Write out a boring URL hotlist entry.  See comment at the top of
 *   hot_measure_URL for the format used.  Assume we start writing at
 *   the start of the buffer passed in.  Return a pointer to where the
 *   buffer ends when we get done.
 */
PRIVATE char *
hot_write_URL(char * buffer, HotlistStruct * item, int bLongFormat, int nIndent)
{

    int iLen;
    int32 lVal;
    int32 sVal;

    if(!item || !buffer)
        return(buffer);    

    if(bLongFormat) {                 

        /* copy the type */
        sVal = (int16) item->type;
        iLen = 2;
        XP_MEMCPY(buffer, &sVal, iLen);
        buffer += iLen;

        /* copy the name */
        if(item->name) {
            iLen = XP_STRLEN(item->name);
            XP_MEMCPY(buffer, item->name, iLen);
            buffer += iLen;
        }

        /* put the \n terminator on */
        *buffer++ = '\n';

        /* copy the address */
        if(item->address) {
            iLen = XP_STRLEN(item->address);
            XP_MEMCPY(buffer, item->address, iLen);
            buffer += iLen;
        }

        /* put the \n terminator on */
        *buffer++ = '\n';

        /* addition date */
        lVal = (int32) item->addition_date;
        iLen = 4;
        XP_MEMCPY(buffer, &lVal, iLen);
        buffer += iLen;

        /* last visit date */            
        lVal = (int32) item->last_visit;
        iLen = 4;
        XP_MEMCPY(buffer, &lVal, iLen);
        buffer += iLen;

        /* copy the description */
        if(item->description) {
            iLen = XP_STRLEN(item->description);
            XP_MEMCPY(buffer, item->description, iLen);
            buffer += iLen;
        }

        /* put the \n terminator on */
        *buffer++ = '\0';

    } else {

        XP_MEMSET(buffer, ' ', nIndent);
        buffer += nIndent;

        if(item->address) {
            XP_STRCPY(buffer, item->address);
            buffer += XP_STRLEN(item->address);
        }

        *buffer++ = '\n';

    }

    return(buffer);

}

/*
 * Write out a separator entry.  See comment at the top of
 *   hot_measure_Separator for the format used.  Assume we start writing at
 *   the start of the buffer passed in.  Return a pointer to where the
 *   buffer ends when we get done.
 */
PRIVATE char *
hot_write_Separator(char * buffer, HotlistStruct * item, int bLongFormat, int nIndent)
{

    int iLen;
#if 0
    int32 lVal;
#endif
    int32 sVal;

    if(!item || !buffer)
        return(buffer);    

    if(bLongFormat) {                 

        /* copy the type */
        sVal = (int16) item->type;
        iLen = 2;
        XP_MEMCPY(buffer, &sVal, iLen);
        buffer += iLen;

    } else {

        XP_MEMSET(buffer, ' ', nIndent);
        buffer += nIndent;

        XP_MEMSET(buffer, '-', SEPATATOR_COUNT);
        buffer += SEPATATOR_COUNT;
 
        *buffer++ = '\n';

    }

    return(buffer);

}


/*
 * Write out a hotlist header entry.  See comment at the top of
 *   hot_measure_Header for the format used.  Assume we start writing at
 *   the start of the buffer passed in.  Return a pointer to where the
 *   buffer ends when we get done.
 */
PRIVATE char *
hot_write_Header(char * buffer, HotlistStruct * item, int bLongFormat, int nIndent)
{

    XP_List * list;
    int iLen;
    int32 lVal;
    int32 sVal;

    if(!item || !buffer)
        return(buffer);    

    if(bLongFormat) {                 

        /* copy the type */
        sVal = (int16) item->type;
        iLen = 2;
        XP_MEMCPY(buffer, &sVal, iLen);
        buffer += iLen;

        /* copy the name */
        if(item->name) {
            iLen = XP_STRLEN(item->name);
            XP_MEMCPY(buffer, item->name, iLen);
            buffer += iLen;
        }

        /* put the \n terminator on */
        *buffer++ = '\n';

        /* addition date */
        lVal = (int32) item->addition_date;
        iLen = 4;
        XP_MEMCPY(buffer, &lVal, iLen);
        buffer += iLen;

        /* number of children */            
        lVal = XP_ListCount(item->children);
        iLen = 4;
        XP_MEMCPY(buffer, &lVal, iLen);
        buffer += iLen;

        /* copy the description */
        if(item->description) {
            iLen = XP_STRLEN(item->description);
            XP_MEMCPY(buffer, item->description, iLen);
            buffer += iLen;
        }

        /* put the \n terminator on */
        *buffer++ = '\0';

    } else {

        XP_MEMSET(buffer, ' ', nIndent);
        buffer += nIndent;

        if(item->name) {
            XP_STRCPY(buffer, item->name);
            buffer += XP_STRLEN(item->name);
        }

        *buffer++ = '\n';

    }

    /* if no children just get out now */
    if(!item->children)
        return(buffer);

    /* write out the children */
    for(list = item->children->next; list; list = list->next) {
        HotlistStruct * child = (HotlistStruct *) list->object;

        if(!child)
            continue;

        switch(child->type) {
        case HOT_URLType:
            buffer = hot_write_URL(buffer, child, bLongFormat, nIndent + TEXT_INDENT);
            break;
        case HOT_HeaderType:
            buffer = hot_write_Header(buffer, child, bLongFormat, nIndent + TEXT_INDENT); 
            break;
        case HOT_SeparatorType:
            buffer = hot_write_Separator(buffer, child, bLongFormat, nIndent + TEXT_INDENT); 
            break;
        default:
            break;
        }

    }

    return(buffer);

}


/*
 * Take a URL packed in a block the way hot_write_URL packs it.
 * Return the new item if we created one
 */
PRIVATE HotlistStruct *
hot_read_URL(char * buffer, HotlistStruct * pListParent, HotlistStruct * item, int bLongFormat, int32 * lBytesEaten)
{

    HotlistStruct * new_item = NULL;

    if(!buffer)
        return(NULL);
                              
    if(bLongFormat) {                 

        int32 addition, visit;

        /* get the name */
        char * name = buffer;
        char * address = strchr(name, '\n');
        char * description = NULL;
        char * ptr;
        if(!address)
            return(NULL);

        *address++ = '\0';

        /* get the address */
        ptr = strchr(address, '\n');
        if(!ptr)
            return(NULL);

        *ptr++ = '\0';

        /* addition date */
        XP_MEMCPY(&addition, ptr, 4);
        ptr += 4;

        /* visiting date */
        XP_MEMCPY(&visit, ptr, 4);
        ptr += 4;

        /* get the description (it should be NULL terminated) */
        description = ptr;

        /* we should really strip leading whitespace */
        new_item = HOT_CreateEntry(HOT_URLType, name, address, 0, visit);
        new_item->addition_date = addition;
        new_item->description = XP_STRDUP(description);
        *lBytesEaten = XP_STRLEN(description) + (description - buffer) + 1;

    } else {

        char * end = strchr(buffer, '\n');

        /* if there was a return NULL terminate the current string */
        if(end)
            *end++ = '\0';

        /* we should really strip leading whitespace */
        new_item = HOT_CreateEntry(HOT_URLType, buffer, buffer, 0, 0);
        new_item->addition_date = time ((time_t *) NULL);
        *lBytesEaten = XP_STRLEN(buffer) + 1;

    }

    if(item)
        HOT_InsertItemAfter(item, new_item);
    else
        HOT_InsertItemInHeaderOrAfterItem(pListParent, new_item);
        
    return(new_item);
               
}


/*
 * Take a separator and insert it
 */
PRIVATE HotlistStruct *
hot_read_Separator(char * buffer, HotlistStruct * pListParent, HotlistStruct * item, int bLongFormat, int32 * lBytesEaten)
{
#if 0
    int32 kids = 0;
    int16 sVal;
#endif
    HotlistStruct * new_item = NULL;

    if(!buffer)
        return(NULL);

    /* can only read long format headers */
    if(bLongFormat) {                 

        new_item = HOT_CreateEntry(HOT_SeparatorType, NULL, NULL, 0, 0);
        *lBytesEaten = 0;

        if(item)
            HOT_InsertItemAfter(item, new_item);
        else
            HOT_InsertItemInHeaderOrAfterItem(pListParent, new_item);

        return(new_item);

    }

    return(NULL);
               
}



/*
 * Take a header and children packed in a block the way hot_write_Header 
 * packs it.  Return the new header item if we created one
 */
PRIVATE HotlistStruct *
hot_read_Header(char * buffer, HotlistStruct * pListParent, HotlistStruct * item, int bLongFormat, int32 * lBytesEaten)
{

    int32 kids = 0;
    int16 sVal;
    HotlistStruct * new_item = NULL;

    if(!buffer)
        return(NULL);

    /* can only read long format headers */
    if(bLongFormat) {                 

        int32 addition;

        /* get the name */
        char * name = buffer;
        char * ptr = strchr(name, '\n');
        char * description = NULL;
        if(!ptr)
            return(NULL);

        /* skip over the \n but change it to a \0 so strcpy() will work */
        *ptr++ = '\0';

        /* addition date */
        XP_MEMCPY(&addition, ptr, 4);
        ptr += 4;

        /* number of children to read */
        XP_MEMCPY(&kids, ptr, 4);
        ptr += 4;

        /* get the description (it should be NULL terminated) */
        description = ptr;

        /* we should really strip leading whitespace */
        new_item = HOT_CreateEntry(HOT_HeaderType, name, NULL, 0, 0);
        new_item->addition_date = addition;
        new_item->description = XP_STRDUP(description);
        *lBytesEaten = XP_STRLEN(description) + (description - buffer) + 1;

        /* handle all of the kids now */
        if(kids) {

            int i;
            HotlistStruct * kid = NULL;

            new_item->children = XP_ListNew();
            buffer += *lBytesEaten;

            for(i = 0; i < kids; i++) {

                int32 lEat;

                /* determine the type of the next entry */
                sVal = 0;
                XP_MEMCPY(&sVal, buffer, 2);
                buffer += 2;
                *lBytesEaten += 2;

                switch(sVal) {
                case HOT_URLType:
                    kid = hot_read_URL(buffer, new_item, kid, bLongFormat, &lEat);
                    *lBytesEaten += lEat;
                    buffer += lEat;
                    break;
                case HOT_HeaderType:
                    kid = hot_read_Header(buffer, new_item, kid, bLongFormat, &lEat);
                    *lBytesEaten += lEat;
                    buffer += lEat;
                    break;
                case HOT_SeparatorType:
                    kid = hot_read_Separator(buffer, new_item, kid, bLongFormat, &lEat);
                    *lBytesEaten += lEat;
                    buffer += lEat;
                    break;
                default:
                    /* bogus type.  Who knows whats going on.  Just quit and get out */
                    break;
                }
            
            }        

        } else {

            /* no kids */
            new_item->children = NULL;

        }

    }

    if(item)
        HOT_InsertItemAfter(item, new_item);
    else
        HOT_InsertItemInHeaderOrAfterItem(pListParent, new_item);
        
    return(new_item);
               
}



/*
 * Allocate and return a string that contains the text representation of
 *   a list of hotlist entries (including headers and their contents).
 * The caller is responsible for freeing the string
 */
PUBLIC char *
HOT_ConvertSelectionsToBlock(HotlistStruct ** list, int iCount, int bLongFormat, int32 * lTotalLen)
{

    int i, j;
    int bSkip;
    int16 marker;
    int32 iSpace = 0;
    HotlistStruct * item;
    HotlistStruct * pParent;
    char * pString;
    char * pOriginal;
 
    for(i = 0; i < iCount; i++) {

        /* don't skip yet */
        bSkip = FALSE;

        /* make sure we have a valid item */
        item = list[i];
        if(!item)
            continue;

        /* 
         * if our parent is in the list don't add ourselves, we will have
         *   been added when our parent got added.  Ugh.  This is going to be
         *   n^2 over the number of selected items
         */
        for(pParent = item->parent; pParent && !bSkip; pParent = pParent->parent) {
            for(j = 0; (j < iCount) && (!bSkip) ; j++)
                if(list[j] == pParent)
                    bSkip = TRUE;
        }

        if(bSkip)
            continue;

        /* decide how much space we need */
        switch(item->type) {
        case HOT_URLType:
            iSpace += hot_measure_URL(item, bLongFormat, 0);
            break;
        case HOT_HeaderType:
            iSpace += hot_measure_Header(item, bLongFormat, 0);
            break;
        case HOT_SeparatorType:
            iSpace += hot_measure_Separator(item, bLongFormat, 0);
            break;
        default:
            break;
        }

    }

    /* leave room for end of list marker */
    if(bLongFormat)
        iSpace += 2;

    /* leave room for the termination character */
    iSpace++;

#ifdef XP_WIN16
    if(iSpace > 32000)
        return(NULL);
#endif

    /* allocate the string */
    pOriginal = pString = (char *) XP_ALLOC(iSpace * sizeof(char));
    if(!pString)
        return(NULL);

    /* Make a big string */
    for(i = 0; i < iCount; i++) {

        bSkip = FALSE;

        /* make sure we have a valid item */
        item = list[i];
        if(!item)
            continue;

        /* 
         * if our parent is in the list don't add ourselves, we will have
         *   been added when our parent got added.  Ugh.  This is going to be
         *   n^2 over the number of selected items
         */
        for(pParent = item->parent; pParent && !bSkip; pParent = pParent->parent) {
            for(j = 0; (j < iCount) && (!bSkip) ; j++)
                if(list[j] == pParent)
                    bSkip = TRUE;
        }

        if(bSkip)
            continue;

        /* write out the item */
        switch(item->type) {
        case HOT_URLType:
            pString = hot_write_URL(pString, item, bLongFormat, 0);
            break;
        case HOT_HeaderType:
            pString = hot_write_Header(pString, item, bLongFormat, 0);
            break;
        case HOT_SeparatorType:
            pString = hot_write_Separator(pString, item, bLongFormat, 0);
            break;
        default:
            break;
        }

    }

    /* stick the end of list marker on so that when we are decoding this */
    /* block we know when we are done                                    */
    if(bLongFormat) {
        marker = 12345;
        XP_MEMCPY(pString, &marker, 2);
        pString +=2;
    }

    /* end the string and return the total length to our caller */
    *pString++ = '\0';
    *lTotalLen = (pString - pOriginal);
    return(pOriginal);

}



/*
 * Take a block of memory formatted by HOT_ConvertSelectionsToBlock and insert
 *   the items it represents into the hotlist following 'item'.  If item is 
 *   NULL insert at the beginning of the hotlist.
 */
PUBLIC void
HOT_InsertBlockAt(char * pOriginalBlock, HotlistStruct * item, int bLongFormat, int32 lTotalLen)
{

    int16 sVal;              /* 16-bit scratch variable */
    int32 lBytesEaten = 0;   /* total number of bytes eaten */
    int32 lEat;              /* number of bytes eaten on this item */
    char * pCurrentPos;
    char * pBlock;
    HotlistStruct * pParent = HOT_GetHotlist();

    if(!pOriginalBlock)
        return;

    /* make a copy of the string we can write into */    
    pCurrentPos = pBlock = (char *) XP_ALLOC(lTotalLen + 1);
    if(!pBlock)
        return;    

    /* copy the data over and make sure we are NULL terminated to make life easier */    
    XP_MEMCPY(pBlock, pOriginalBlock, lTotalLen);
    pBlock[lTotalLen] = '\0';

    /* 
     * if our very first element is a header then we really want to insert
     *   everything into the header and not after it --- I think.
     */
    if(item && item->type == HOT_HeaderType) {
        pParent = item;
        item = NULL;

        /* 
         * gack! if we are inserting under a header make sure its set up
         *  to accept children
         */
        if(!pParent->children)
            pParent->children = XP_ListNew();
    }

    /* long format can have all kinds of different types of things in it */
    if(bLongFormat) {    
 
        while(lBytesEaten < lTotalLen) {

            /* determine the type of the next entry */
            sVal = 0;
            XP_MEMCPY(&sVal, pCurrentPos, 2);
            pCurrentPos += 2;
            lBytesEaten += 2;

            switch(sVal) {
            case HOT_URLType:
                item = hot_read_URL(pCurrentPos, pParent, item, bLongFormat, &lEat);
                lBytesEaten += lEat;
                pCurrentPos += lEat;
                break;
            case HOT_HeaderType:
                item = hot_read_Header(pCurrentPos, pParent, item, bLongFormat, &lEat);
                lBytesEaten += lEat;
                pCurrentPos += lEat;
                break;
            case HOT_SeparatorType:
                item = hot_read_Separator(pCurrentPos, pParent, item, bLongFormat, &lEat);
                lBytesEaten += lEat;
                pCurrentPos += lEat;
                break;
            default:
                /* bogus type.  Who knows whats going on.  Just quit and get out */
                return;
                break;
            }        

        }

    } else {

        /* short format is just a list of URLs separated by \n's */
        while(lBytesEaten < lTotalLen) {

            item = hot_read_URL(pCurrentPos, pParent, item, bLongFormat, &lEat);
            lBytesEaten += lEat;
            pCurrentPos += lEat;

            /* if we just walked over a \0 we are done */
            if(pOriginalBlock[lBytesEaten - 1] == '\0')
                lBytesEaten = lTotalLen;

        }

    }

    /* mark the bookmark list as changed and clean up */
    HOT_SetModified();
    XP_FREE(pBlock);

}
