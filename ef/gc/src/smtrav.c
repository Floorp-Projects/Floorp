/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; -*-  
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


/*******************************************************************************
                            S P O R T   M O D E L    
                              _____
                         ____/_____\____
                        /__o____o____o__\        __
                        \_______________/       (@@)/
                         /\_____|_____/\       x~[]~
             ~~~~~~~~~~~/~~~~~~~|~~~~~~~\~~~~~~~~/\~~~~~~~~~

    Advanced Technology Garbage Collector
    Copyright (c) 1997 Netscape Communications, Inc. All rights reserved.
    Recovered by: Warren Harris
*******************************************************************************/

#include "smtrav.h"
#include "smheap.h"

SM_IMPLEMENT(PRWord)
SM_TraverseObjectFields(SMObject* obj,
                        SMTraverseFieldProc traverseProc, void* traverseClosure)
{
    SMClass* clazz = SM_OBJECT_CLASS(obj);
    if (clazz) {
        switch (SM_CLASS_KIND(clazz)) {
          case SMClassKind_ObjectClass: {
              SMFieldDesc* fieldDesc = SM_CLASS_GET_INST_FIELD_DESCS(clazz);
              PRUint16 i, fdCount = SM_CLASS_GET_INST_FIELDS_COUNT(clazz);
              for (i = 0; i < fdCount; i++) {
                  PRWord offset = fieldDesc->offset;
                  PRWord count = fieldDesc->count;
                  SMObject** base = SM_OBJECT_FIELDS(obj);
                  SMObject** ref = &base[offset];
                  SMObject* field;
                  while (count-- > 0) {
                      field = *ref++;
                      if (field) {
                          PRWord status = traverseProc(field, ref - base, traverseClosure);
                          if (status != 0)
                              return status;
                      }
                  }
              }
              break;
          }
          case SMClassKind_ArrayClass: {
              if (SM_IS_OBJECT_ARRAY_CLASS(clazz)) {
                  SMArray* arr = (SMArray*)SM_ENSURE(SMObject, obj);
                  PRUword size = SM_ARRAY_SIZE(arr);
                  SMObject** elementRefs = SM_ARRAY_ELEMENTS(arr);
                  SMObject** base = elementRefs;
                  SMObject* element;
                  while (size-- > 0) {
                      element = *elementRefs++;
                      if (element) {
                          PRWord status =
                              traverseProc(element, 
                                           elementRefs - base,
                                           traverseClosure);
                          if (status != 0)
                              return status;
                      }
                  }
              }
              break;
          }
          default:
            break;
        }
    }
    return 0;
}

SM_IMPLEMENT(PRWord)
SM_TraversePageObjects(SMPage* page,
                       SMTraverseObjectProc traverseProc, void* traverseClosure)
{
    SMPageCount pageNum = SM_PAGE_NUMBER(page);
    SMPageDesc* pd = &sm_PageMgr.pageTable[pageNum];
    if (SM_PAGEDESC_GEN(pd) != SMGenNum_Free) {
        SMBucket bucket = SM_PAGEDESC_BUCKET(pd);
        SMSmallObjCount objsPerPage = sm_ObjectsPerPage[bucket];
        SMSmallObjSize objSize = sm_ObjectSize[bucket];
        SMObjDesc* objTable = pd->objTable;
        SMGenNum genNum = SM_PAGEDESC_GEN(pd);
        SMSmallObjCount i;
        SMObjStruct* obj = (SMObjStruct*)SM_PAGE_BASE(page);
        for (i = 0; i < objsPerPage; i++) {
            SMObjDesc* od = &objTable[i];
            if (!SM_OBJDESC_WAS_FREE(od)) {
                PRWord status = traverseProc(SM_ENCRYPT_OBJECT(obj), i, objSize, traverseClosure);
                if (status != 0)
                    return status;
            }
            obj = (SMObjStruct*)((char*)obj + objSize);
        }
    }
    return 0;
}

SM_IMPLEMENT(PRWord)
SM_TraverseGenPages(SMGenNum genNum,
                    SMTraversePageProc traverseProc, void* traverseClosure)
{
    SMPageCount i;
    for (i = 0; i < sm_PageMgr.heapPageCount; i++) {
        SMPageDesc* pd = &sm_PageMgr.pageTableMem[i];
        if (SM_PAGEDESC_GEN(pd) == genNum) {
            SMPage* page = SM_PAGEDESC_PAGE(pd);
            PRWord status = traverseProc(page, traverseClosure);
            if (status != 0)
                return status;
        }
    }
    return 0;
}

typedef struct SMTraversePageObjectsClosure {
    SMTraverseObjectProc        traverseObject;
    void*                       traverseData;
} SMTraversePageObjectsClosure;

static PRWord
sm_TraversePageObjects(SMPage* page, void* traverseClosure)
{
    SMTraversePageObjectsClosure* c = (SMTraversePageObjectsClosure*)traverseClosure;
    return SM_TraversePageObjects(page, c->traverseObject, c->traverseData);
}

SM_IMPLEMENT(PRWord)
SM_TraverseGenObjects(SMGenNum genNum,
                      SMTraverseObjectProc traverseProc, void* traverseClosure)
{
    SMTraversePageObjectsClosure c;
    c.traverseObject = traverseProc;
    c.traverseData = traverseClosure;
    return SM_TraverseGenPages(genNum, sm_TraversePageObjects, &c);
}

SM_IMPLEMENT(PRWord)
SM_TraverseAllPages(SMTraversePageProc traverseProc, void* traverseClosure)
{
    SMGenNum genNum;
    for (genNum = SMGenNum_Freshman; genNum <= SMGenNum_Malloc; genNum++) {
        PRWord status = SM_TraverseGenPages(genNum, traverseProc, traverseClosure);
        if (status != 0)
            return status;
    }
    return 0;
}

SM_IMPLEMENT(PRWord)
SM_TraverseAllObjects(SMTraverseObjectProc traverseProc, void* traverseClosure)
{
    SMTraversePageObjectsClosure c;
    c.traverseObject = traverseProc;
    c.traverseData = traverseClosure;
    return SM_TraverseAllPages(sm_TraversePageObjects, &c);
}

/******************************************************************************/

#ifdef SM_DUMP    /* define this if you want dump code in the final product */

static void
sm_DumpIndent(FILE* out, PRUword indent)
{
    while (indent--) {
        fputc(' ', out);
    }
}

static void
sm_DumpHex(FILE* out, void* addr, PRUword count, PRUword wordsPerLine,
           PRUword indent)
{
    void** p = (void**)addr;
    while (count > 0) {
        PRUword i = wordsPerLine;
        sm_DumpIndent(out, indent);
        if (i > count)
            i = count;
        count -= i;
        while (i--) {
            fprintf(out, "%.8x", *p++);
            if (i)
                fputc(' ', out);
        }
        fputc('\n', out);
    }
}

typedef struct SMDumpClosure {
    FILE* out;
    PRUword flags;
} SMDumpClosure;

static PRWord
sm_DumpObj(void* p, SMSmallObjCount index, PRUword objSize, void* closure)
{
    SMObject* obj = (SMObject*)p;
    SMObjStruct* dobj = SM_DECRYPT_OBJECT(obj);
    SMClass* clazz = SM_OBJECT_CLASS(obj);
    SMDumpClosure* c = (SMDumpClosure*)closure;
    SMPageCount pageNum = SM_PAGE_NUMBER(dobj);
    SMPageDesc* pd = &sm_PageMgr.pageTable[pageNum];
    SMGenNum genNum = SM_PAGEDESC_GEN(pd);
    SMObjDesc* od = SM_OBJECT_HEADER_FROM_PAGEDESC(dobj, pd);
    SMSmallObjCount pageOffset = (SMSmallObjCount)SM_PAGE_OFFSET(dobj);
    SMBucket bucket = SM_PAGEDESC_BUCKET(pd); 
    SMSmallObjCount objNum = SM_DIV(pageOffset, bucket); 
    SM_ASSERT(objNum == index);
    if (c->flags & SMDumpFlag_GCState && SM_OBJDESC_IS_UNMARKED(od)) {
        fprintf(c->out, "    %3u %08x size=%u UNMARKED\n", objNum, dobj, objSize);
    }
    else if (c->flags & SMDumpFlag_GCState && SM_OBJDESC_IS_UNTRACED(od)) {
        /* should never happen */
        fprintf(c->out, "    %3u %08x size=%u UNTRACED!!!\n", objNum, dobj, objSize);
    }
    else if (c->flags & SMDumpFlag_GCState && SM_OBJDESC_IS_FORWARDED(od)) {
        fprintf(c->out, "    %3u %08x size=%u FORWARDED to %08x\n",
                objNum, dobj, objSize, SM_OBJECT_FORWARDING_ADDR(obj));
    }
    else if (SM_OBJDESC_IS_FREE(od)) {
        return 0;
    }
    else if (genNum == SMGenNum_Malloc) {
        fprintf(c->out, "    %3u %08x size=%u\n", objNum, dobj, objSize);
    }
    else if (SM_OBJDESC_NEEDS_FINALIZATION(od)) {
        SM_ASSERT(SM_OBJDESC_IS_FINALIZABLE(od));
        fprintf(c->out, "    %3u %08x class=%08x needs finalization\n",
                objNum, dobj, SM_OBJECT_CLASS(obj));
    }
    else {
        fprintf(c->out, "    %3u %08x class=%08x%s%s%s\n",
                objNum, dobj, clazz, 
                SM_OBJDESC_IS_FINALIZABLE(od) ? " finalizable" : "", 
                SM_OBJDESC_IS_COPYABLE(od) ? " copyable" : "", 
                SM_OBJDESC_IS_PINNED(od) ? " pinned" : "");
    }
    if (c->flags & SMDumpFlag_Detailed) {
        sm_DumpHex(c->out, dobj, objSize >> SM_REF_BITS, 4, 8); 
    }
    return 0;
}

static PRWord
sm_DumpPageObjects(SMPage* page, void* closure)
{
    SMDumpClosure* c = (SMDumpClosure*)closure;
    FILE* out = c->out;
    PRUword flags = c->flags;
    SMPageDesc* pd = SM_OBJECT_PAGEDESC(page);
    SMBucket bucket = SM_PAGEDESC_BUCKET(pd);
    SMSmallObjSize objSize = sm_ObjectSize[bucket];
    SMPageCount pageCnt = pd - sm_PageMgr.pageTableMem;
    if (SM_PAGEDESC_GEN(pd) == SMGenNum_Free) {
        fprintf(out, "  Page %u FREE\n", pageCnt);
        return 0;
    }
    if (bucket == SM_LARGE_OBJECT_BUCKET) {
        if (SM_PAGEDESC_IS_LARGE_OBJECT_START(pd)) {
            SMDumpClosure c2;
            if (flags & SMDumpFlag_Abbreviate) {
                /* turn off detailed dumps for large objects */
                c2.flags = flags & ~SMDumpFlag_Detailed;
                c2.out = out;
                closure = &c2;
            }
            fprintf(out, "  Page %u gen=%u objSize=%u (large object)",
                    pageCnt, SM_PAGEDESC_GEN(pd), SM_PAGE_WIDTH(pd->allocCount));
            if (flags & SMDumpFlag_GCState)
                fprintf(out, " objTable=%08x\n", pd->objTable);
            else 
                fputc('\n', out);
            sm_DumpObj(page, 0, SM_PAGE_WIDTH(pd->allocCount), closure);
        }
        /* else skip it -- we've already dumped the object */
        return 0;
    }
    else {
        fprintf(out, "  Page %u gen=%u objSize=%u allocCount=%u",
                pageCnt, SM_PAGEDESC_GEN(pd), objSize, pd->allocCount);
        if (flags & SMDumpFlag_GCState)
            fprintf(out, " objTable=%08x\n", pd->objTable);
        else 
            fputc('\n', out);
    }
    return SM_TraversePageObjects(page, sm_DumpObj, closure);
}

SM_IMPLEMENT(void)
SM_DumpMallocHeap(FILE* out, PRUword flags)
{
    SMDumpClosure closure;
    closure.out = out;
    closure.flags = flags;
    fprintf(out, "--SportModel-Malloc-Heap-Dump------------------------------\n");
    (void)SM_TraverseGenPages(SMGenNum_Malloc, sm_DumpPageObjects, &closure);
    fprintf(out, "-----------------------------------------------------------\n");
}

SM_IMPLEMENT(void)
SM_DumpGCHeap(FILE* out, PRUword flags)
{
    SMGenNum genNum;
    SMDumpClosure closure;
    closure.out = out;
    closure.flags = flags;
    fprintf(out, "--SportModel-GC-Heap-Dump----------------------------------\n");
    for (genNum = SMGenNum_Freshman; genNum <= SMGenNum_Static; genNum++) {
        fprintf(out, "Gen %u\n", genNum);
        (void)SM_TraverseGenPages(genNum, sm_DumpPageObjects, &closure);
    }
    fprintf(out, "-----------------------------------------------------------\n");
    return;
}

SM_IMPLEMENT(void)
SM_DumpPage(FILE* out, void* addr, PRUword flags)
{
    SMPage* page = SM_PAGE_BASE(addr);
    SMDumpClosure c;
    c.out = out;
    c.flags = flags;
    (void)sm_DumpPageObjects(page, &c);
#if 0
    SMPageCount pageNum = SM_PAGE_NUMBER(addr);
    SMPageDesc* pd = &sm_PageMgr.pageTable[pageNum];
    SMPageCount pageCnt = pd - sm_PageMgr.pageTableMem;
    if (SM_PAGEDESC_GEN(pd) == SMGenNum_Free) {
        fprintf(out, "Page %u FREE\n", pageCnt);
    }
    else {
        SMBucket bucket = SM_PAGEDESC_BUCKET(pd);
        SMSmallObjCount objsPerPage = sm_ObjectsPerPage[bucket];
        SMSmallObjSize objSize = sm_ObjectSize[bucket];
        SMObjDesc* objTable = pd->objTable;
        SMGenNum genNum = SM_PAGEDESC_GEN(pd);
        SMSmallObjCount i;
        SMObjStruct* obj = (SMObjStruct*)SM_PAGE_BASE(addr);
        SMDumpClosure c;
        c.out = out;
        c.flags = flags;
        fprintf(out, "Page %u objSize=%u gen=%u allocCount=%u\n",
                pageCnt, objSize, genNum, pd->allocCount);
        for (i = 0; i < objsPerPage; i++) {
            SMObjDesc od = objTable[i];
            (void)sm_DumpObj(SM_ENCRYPT_OBJECT(obj), i, objSize, &c);
            obj = (SMObjStruct*)((char*)obj + objSize);
        }
    }
#endif
}

SM_IMPLEMENT(void)
SM_DumpHeap(FILE* out, PRUword flags)
{
    SM_DumpMallocHeap(out, flags);
    SM_DumpGCHeap(out, flags);
}

/******************************************************************************/

#define SM_PRINT_WIDTH          64
#define SM_END_OF_PAGE(pos)     \
    (((pos) % SM_PRINT_WIDTH) == (SM_PRINT_WIDTH - 1))

SM_IMPLEMENT(void)
SM_DumpPageMap(FILE* out, PRUword flags)
{
    SMPageCount i;
    for (i = 0; i < sm_PageMgr.heapPageCount; i++) {
        SMPageDesc* pd = &sm_PageMgr.pageTableMem[i];
        SMBucket bucket = SM_PAGEDESC_BUCKET(pd);
        int isBlacklisted = SM_PAGEDESC_IS_BLACKLISTED(pd);
        if (bucket == SM_LARGE_OBJECT_BUCKET) {
            if (isBlacklisted)
                fputc('L', out);
            else
                fputc('l', out);
        }
        else {
            char c;
            SMGenNum genNum = SM_PAGEDESC_GEN(pd);
            if (SM_IS_GC_SPACE(genNum)) {
                if (isBlacklisted)
                    c = 'G';
                else
                    c = 'g';
            }
            else if (genNum == SMGenNum_Malloc) {
                if (isBlacklisted)
                    c = 'M';
                else
                    c = 'm';
            }
            else if (genNum == SMGenNum_Free) {
                if (isBlacklisted)
                    c = 'F';
                else
                    c = 'f';
            }
            else {      /* unknown */
                if (isBlacklisted)
                    c = 'X';
                else
                    c = 'x';
            }
            fprintf(out, "%c", c);
        }
        if (SM_END_OF_PAGE(i))
            fputc('\n', out);
    }    
    fputc('\n', out);
}

#endif /* SM_DUMP */

/******************************************************************************/

#if defined(DEBUG) || defined(SM_DUMP)

FILE* sm_DebugOutput = NULL;

SM_IMPLEMENT(void)
SM_SetTraceOutputFile(FILE* out)
{
    if (out == NULL && sm_DebugOutput) {
        fflush(sm_DebugOutput);
    }
    sm_DebugOutput = out;
}

#endif /* defined(DEBUG) || defined(SM_DUMP) */

/******************************************************************************/
