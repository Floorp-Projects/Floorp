/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/***************************************************************************
(C) Copyright 1996 Apple Computer, Inc., AT&T Corp., International             
Business Machines Corporation and Siemens Rolm Communications Inc.             
                                                                               
For purposes of this license notice, the term Licensors shall mean,            
collectively, Apple Computer, Inc., AT&T Corp., International                  
Business Machines Corporation and Siemens Rolm Communications Inc.             
The term Licensor shall mean any of the Licensors.                             
                                                                               
Subject to acceptance of the following conditions, permission is hereby        
granted by Licensors without the need for written agreement and without        
license or royalty fees, to use, copy, modify and distribute this              
software for any purpose.                                                      
                                                                               
The above copyright notice and the following four paragraphs must be           
reproduced in all copies of this software and any software including           
this software.                                                                 
                                                                               
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS AND NO LICENSOR SHALL HAVE       
ANY OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS OR       
MODIFICATIONS.                                                                 
                                                                               
IN NO EVENT SHALL ANY LICENSOR BE LIABLE TO ANY PARTY FOR DIRECT,              
INDIRECT, SPECIAL OR CONSEQUENTIAL DAMAGES OR LOST PROFITS ARISING OUT         
OF THE USE OF THIS SOFTWARE EVEN IF ADVISED OF THE POSSIBILITY OF SUCH         
DAMAGE.                                                                        
                                                                               
EACH LICENSOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED,       
INCLUDING BUT NOT LIMITED TO ANY WARRANTY OF NONINFRINGEMENT OR THE            
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR             
PURPOSE.                                                                       

The software is provided with RESTRICTED RIGHTS.  Use, duplication, or         
disclosure by the government are subject to restrictions set forth in          
DFARS 252.227-7013 or 48 CFR 52.227-19, as applicable.                         

***************************************************************************/

/*
 * doc: vobject and APIs to construct vobject, APIs pretty print 
 * vobject, and convert a vobject into its textual representation.
 */

#include "nsVCard.h"
#include "nsVCardObj.h"
#include "prmem.h"
#include "plstr.h"
#include "msgCore.h"
#include "prprf.h"
#define NOT_NULL(X)	X

/* debugging utilities */
#define DBG_(x)

#ifdef __cplusplus
extern "C" {
#endif

  char    **fieldedProp;

#ifdef __cplusplus
    };
#endif



static VObject* newVObject_(const char *id);
#if 0
static int vObjectValueType(VObject *o);
static void initVObjectIterator(VObjectIterator *i, VObject *o);
#endif

/*----------------------------------------------------------------------
   The following functions involve with memory allocation:
	newVObject
	deleteVObject
	dupStr
	deleteString
	newStrItem
	deleteStrItem
   ----------------------------------------------------------------------*/

static PRBool needsQuotedPrintable (const char *s)
{
    const unsigned char *p = (const unsigned char *)s;

	if (PL_strstr (s, MSG_LINEBREAK))
		return PR_TRUE;

	while (*p) {
		if (*p & 0x80)
			return PR_TRUE;
		p++;
	}

	return PR_FALSE;
}

VObject* newVObject_(const char *id)
{
    VObject *p = (VObject*) new(VObject);
    p->next = 0;
    p->id = id;
    p->prop = 0;
    VALUE_TYPE(p) = 0;
	ANY_VALUE_OF(p) = 0;
    return p;
}

VObject* newVObject(const char *id)
{
    return newVObject_(lookupStr(id));
}

void deleteVObject(VObject *p)
{
    unUseStr(p->id);
    PR_FREEIF (p);
}

char* dupStr(const char *s, unsigned int size)
{
    char *t;
    if  (size == 0) {
	size = PL_strlen(s);
	}
    t = (char*)PR_CALLOC(size+1);
	if (t) {
	memcpy(t,s,size);
	t[size] = 0;
	return t;
	}
    else {
	return (char*)0;
	}
}

static StrItem* newStrItem(const char *s, StrItem *next)
{
    StrItem *p = (StrItem*)PR_CALLOC(sizeof(StrItem));
    p->next = next;
    p->s = s;
    p->refCnt = 1;
    return p;
}

extern "C" 
void deleteString(char *p)
{
    if (p) 
      PR_Free ((void*)p);
}

extern "C"
void deleteStrItem(StrItem *p)
{
    if (p) 
      PR_FREEIF (p);
}



/*----------------------------------------------------------------------
  The following function provide accesses to VObject's value.
  ----------------------------------------------------------------------*/

const char* vObjectName(VObject *o)
{
    return NAME_OF(o);
}

void setVObjectName(VObject *o, const char* id)
{
    NAME_OF(o) = id;
}

const char* vObjectStringZValue(VObject *o)
{
    return STRINGZ_VALUE_OF(o);
}

void setVObjectStringZValue(VObject *o, const char *s)
{
    STRINGZ_VALUE_OF(o) = dupStr(s,0);
    VALUE_TYPE(o) = VCVT_STRINGZ;
}

void setVObjectStringZValue_(VObject *o, const char *s)
{
    STRINGZ_VALUE_OF(o) = s;
    VALUE_TYPE(o) = VCVT_STRINGZ;
}

const vwchar_t* vObjectUStringZValue(VObject *o)
{
    return USTRINGZ_VALUE_OF(o);
}

void setVObjectUStringZValue(VObject *o, const vwchar_t *s)
{
    USTRINGZ_VALUE_OF(o) = (vwchar_t*) dupStr((char*)s,(uStrLen(s)+1)*2);
    VALUE_TYPE(o) = VCVT_USTRINGZ;
}

void setVObjectUStringZValue_(VObject *o, const vwchar_t *s)
{
    USTRINGZ_VALUE_OF(o) = s;
    VALUE_TYPE(o) = VCVT_USTRINGZ;
}

unsigned int vObjectIntegerValue(VObject *o)
{
    return INTEGER_VALUE_OF(o);
}

void setVObjectIntegerValue(VObject *o, unsigned int i)
{
    INTEGER_VALUE_OF(o) = i;
    VALUE_TYPE(o) = VCVT_UINT;
}

unsigned long vObjectLongValue(VObject *o)
{
    return LONG_VALUE_OF(o);
}

void setVObjectLongValue(VObject *o, unsigned long l)
{
    LONG_VALUE_OF(o) = l;
    VALUE_TYPE(o) = VCVT_ULONG;
}

void* vObjectAnyValue(VObject *o)
{
    return ANY_VALUE_OF(o);
}

void setVObjectAnyValue(VObject *o, void *t)
{
    ANY_VALUE_OF(o) = t;
    VALUE_TYPE(o) = VCVT_RAW;
}

VObject* vObjectVObjectValue(VObject *o)
{
    return VOBJECT_VALUE_OF(o);
}

void setVObjectVObjectValue(VObject *o, VObject *p)
{
    VOBJECT_VALUE_OF(o) = p;
    VALUE_TYPE(o) = VCVT_VOBJECT;
}

#if 0
int vObjectValueType(VObject *o)
{
    return VALUE_TYPE(o);
}
#endif


/*----------------------------------------------------------------------
  The following functions can be used to build VObject.
  ----------------------------------------------------------------------*/

VObject* addVObjectProp(VObject *o, VObject *p)
{
    /* circular link list pointed to tail */
    /*
    o {next,id,prop,val}
                V
	pn {next,id,prop,val}
             V
	    ...
	p1 {next,id,prop,val}
             V
	     pn
    -->
    o {next,id,prop,val}
                V
	pn {next,id,prop,val}
             V
	p {next,id,prop,val}
	    ...
	p1 {next,id,prop,val}
             V
	     pn
    */

    VObject *tail = o->prop;
    if (tail) {
	p->next = tail->next;
	o->prop = tail->next = p;
	}
    else {
	o->prop = p->next = p;
	}
    return p;
}

VObject* addProp(VObject *o, const char *id)
{
    return addVObjectProp(o,newVObject(id));
}

VObject* addProp_(VObject *o, const char *id)
{
    return addVObjectProp(o,newVObject_(id));
}

void addList(VObject **o, VObject *p)
{
    p->next = 0;
    if (*o == 0) {
	*o = p;
	}
    else {
	VObject *t = *o;
	while (t->next) {
	   t = t->next;
	   }
	t->next = p;
	}
}

VObject* nextVObjectInList(VObject *o)
{
    return o->next;
}

VObject* setValueWithSize_(VObject *prop, void *val, unsigned int size)
{
    VObject *sizeProp;
    setVObjectAnyValue(prop, val);
    sizeProp = addProp(prop,VCDataSizeProp);
    setVObjectLongValue(sizeProp, size);
    return prop;
}

VObject* setValueWithSize(VObject *prop, void *val, unsigned int size)
{
	void *p = dupStr((const char *)val,size);
    return setValueWithSize_(prop,p,p?size:0);
}

void initPropIterator(VObjectIterator *i, VObject *o)
{
    i->start = o->prop; 
    i->next = 0;
}

#if 0
void initVObjectIterator(VObjectIterator *i, VObject *o)
{
    i->start = o->next; 
    i->next = 0;
}
#endif

int moreIteration(VObjectIterator *i)
{ 
    return (i->start && (i->next==0 || i->next!=i->start));
}

VObject* nextVObject(VObjectIterator *i)
{
    if (i->start && i->next != i->start) {
	if (i->next == 0) {
	    i->next = i->start->next;
	    return i->next;
	    }
	else {
	    i->next = i->next->next;
	    return i->next;
	    }
	}
    else return (VObject*)0;
}

VObject* isAPropertyOf(VObject *o, const char *id)
{
    VObjectIterator i;
    initPropIterator(&i,o);
    while (moreIteration(&i)) {
	VObject *each = nextVObject(&i);
	if (!PL_strcasecmp(id,each->id))
	    return each;
	}
    return (VObject*)0;
}

VObject* addGroup(VObject *o, const char *g)
{
    /*
	a.b.c
	-->
	prop(c)
	    prop(VCGrouping=b)
		prop(VCGrouping=a)
     */
    char *dot = PL_strrchr(g,'.');
    if (dot) {
	VObject *p, *t;
	char *gs, *n = dot+1;
	gs = dupStr(g,0);	/* so we can write to it. */
	t = p = addProp_(o,lookupProp(n));
	dot = PL_strrchr(gs,'.');
	*dot = 0;
	do {
	    dot = PL_strrchr(gs,'.');
	    if (dot) {
		n = dot+1;
		*dot=0;
		}
	    else
		n = gs;
	    /* property(VCGroupingProp=n);
	     *	and the value may have VCGrouping property
	     */
	    t = addProp(t,VCGroupingProp);
	    setVObjectStringZValue(t,lookupProp_(n));
	    } while (n != gs);
	deleteString(gs);	
	return p;
	}
    else
	return addProp_(o,lookupProp(g));
}

VObject* addPropValue(VObject *o, const char *p, const char *v)
{
    VObject *prop;
    prop = addProp(o,p);
	if (v) {
		setVObjectUStringZValue_(prop, fakeUnicode(v,0));
		if (needsQuotedPrintable (v)) {
			if (PL_strcasecmp (VCCardProp, vObjectName(o)) == 0) 
				addProp (prop, VCQuotedPrintableProp);
			else
				addProp (o, VCQuotedPrintableProp);
		}
	}
	else
		setVObjectUStringZValue_(prop, fakeUnicode("",0));

    return prop;
}

VObject* addPropSizedValue_(VObject *o, const char *p, const char *v,
	unsigned int size)
{
    VObject *prop;
    prop = addProp(o,p);
    setValueWithSize_(prop, (void*)v, size);
    return prop;
}

VObject* addPropSizedValue(VObject *o, const char *p, const char *v,
	unsigned int size)
{
    return addPropSizedValue_(o,p,dupStr(v,size),size);
}



/*----------------------------------------------------------------------
  The following pretty print a VObject
  ----------------------------------------------------------------------*/

static void indent(nsOutputFileStream *fp, int level)
{
  int i;
  for (i=0;i<level*4;i++) {
    fp->write(" ", 1);
  }
}

static void printValue(nsOutputFileStream *fp, VObject *o, int level)
{
  char *buf = nsnull;

  switch (VALUE_TYPE(o)) {
  case VCVT_USTRINGZ: {
    char c;
    char *t,*s;
    s = t = fakeCString(USTRINGZ_VALUE_OF(o));
    fp->write("'",1);
    while (c=*t,c) {
      fp->write(&c,1);
      if (c == '\n') indent(fp,level+2);
      t++;
    }
    fp->write("'",1);
    deleteString(s);
    break;
                      }
  case VCVT_STRINGZ: {
    char c;
    char *str = &c;
    const char *s = STRINGZ_VALUE_OF(o);
    fp->write("'",1);
    while (c=*s,c) {
      fp->write(str,1);
      if (c == '\n') indent(fp,level+2);
      s++;
    }
    fp->write("'",1);
    break;
                     }
  case VCVT_UINT:    
    buf = PR_smprintf("%d", INTEGER_VALUE_OF(o)); 
    if (buf)
    {
      fp->write(buf, nsCRT::strlen(buf));
      PR_FREEIF(buf);
    }
    break;

  case VCVT_ULONG:
    buf = PR_smprintf("%ld", LONG_VALUE_OF(o)); 
    if (buf)
    {
      fp->write(buf, nsCRT::strlen(buf));
      PR_FREEIF(buf);
    }
    break;

  case VCVT_RAW:
    fp->write("[raw data]", 10); 
    break;

  case VCVT_VOBJECT:
    fp->write("[vobject]\n", 11);
    printVObject_(fp,VOBJECT_VALUE_OF(o),level+1);
    break;

  case 0:
    fp->write("[none]", 6); 
    break;
  default:
    fp->write("[unknown]", 9); 
    break;
  }
}

static void printNameValue(nsOutputFileStream *fp,VObject *o, int level)
{
  char  *buf;

  indent(fp,level);
  if (NAME_OF(o)) 
  {
    buf = PR_smprintf("%s", NAME_OF(o)); 
    if (buf)
    {
      fp->write(buf, nsCRT::strlen(buf));
      PR_FREEIF(buf);
    }
  }

  if (VALUE_TYPE(o)) 
  {
    fp->write("=",1);
    printValue(fp,o, level);
  }

  fp->write("\n", 1);
}

void printVObject_(nsOutputFileStream *fp, VObject *o, int level)
{
  VObjectIterator t;
  if (o == 0) {
    fp->write("[NULL]\n", 7);
    return;
  }

  printNameValue(fp,o,level);
  initPropIterator(&t,o);

  while (moreIteration(&t)) 
  {
    VObject *eachProp = nextVObject(&t);
    printVObject_(fp,eachProp,level+1);
  }
}

void printVObject(nsOutputFileStream *fp,VObject *o)
{
  printVObject_(fp,o,0);
}

void printVObjectToFile(nsFileSpec *fname, VObject *o)
{
#if !defined(MOZADDRSTANDALONE)
  nsOutputFileStream *fp = new nsOutputFileStream(*fname, PR_WRONLY | PR_CREATE_FILE, 00600);

  if (fp) {
    printVObject(fp,o);
    fp->close();
  }
#else
  PR_ASSERT(PR_FALSE);
#endif
}

void printVObjectsToFile(nsFileSpec *fname,VObject *list)
{
#if !defined(MOZADDRSTANDALONE)
  nsOutputFileStream *fp = new nsOutputFileStream(*fname, PR_WRONLY | PR_CREATE_FILE, 00600);
  if (fp) {
    while (list) {
      printVObject(fp,list);
      list = nextVObjectInList(list);
    }
    fp->close();
  }
#else
  PR_ASSERT(PR_FALSE);
#endif
}


void cleanVObject(VObject *o)
{
    if (o == 0) return;
    if (o->prop) {
	/* destroy time: cannot use the iterator here.
	   Have to break the cycle in the circular link
	   list and turns it into regular NULL-terminated
	   list -- since at some point of destruction,
	   the reference entry for the iterator to work
	   will not longer be valid.
	   */
	VObject *p;
	p = o->prop->next;
	o->prop->next = 0;
	do {
	   VObject *t = p->next;
	   cleanVObject(p);
	   p = t;
	   } while (p);
	}
    switch (VALUE_TYPE(o)) {
	case VCVT_USTRINGZ:
	case VCVT_STRINGZ:
	case VCVT_RAW:
	    /* assume they are all allocated by malloc. */
	    if ((char*) STRINGZ_VALUE_OF(o)) 
        PR_Free ((char*)STRINGZ_VALUE_OF(o));
	    break;
	case VCVT_VOBJECT:
	    cleanVObject(VOBJECT_VALUE_OF(o));
	    break;
	}
    deleteVObject(o);
}

void cleanVObjects(VObject *list)
{
    while (list) {
	VObject *t = list;
	list = nextVObjectInList(list);
	cleanVObject(t);
	}
}

/*----------------------------------------------------------------------
  The following is a String Table Facilities.
  ----------------------------------------------------------------------*/

#define STRTBLSIZE 255

static StrItem *strTbl[STRTBLSIZE];

static unsigned int hashStr(const char *s)
{
    unsigned int h = 0;
    int i;
    for (i=0;s[i];i++) {
	h += s[i]*i;
	}
    return h % STRTBLSIZE;
}

void unUseStr(const char *s)
{
    StrItem *t, *p;
    unsigned int h = hashStr(s);
    if ((t = strTbl[h]) != 0) {
	p = t;
	do {
	    if (PL_strcasecmp(t->s,s) == 0) {
		t->refCnt--;
		if (t->refCnt == 0) {
		    if (p == strTbl[h]) {
			strTbl[h] = t->next;
			}
		    else {
			p->next = t->next;
			}
		    deleteString((char *)t->s);
		    deleteStrItem(t);
		    return;
		    }
		}
	    p = t;
	    t = t->next;
	    } while (t);
	}
}

void cleanStrTbl()
{
    int i;
    for (i=0; i<STRTBLSIZE;i++) {
	StrItem *t = strTbl[i];
	while (t) {
	    StrItem *p;
	    deleteString((char *)t->s);
	    p = t;
	    t = t->next;
	    deleteStrItem(p);
	    } while (t) {}
	strTbl[i] = 0;
	}
}


struct PreDefProp {
    const char *name;
    const char *alias;
    const char** fields;
    unsigned int flags;
    };

/* flags in PreDefProp */
#define PD_BEGIN	0x1
#define PD_INTERNAL	0x2

static const char *adrFields[] = {
    VCPostalBoxProp,
    VCExtAddressProp,
    VCStreetAddressProp,
    VCCityProp,
    VCRegionProp,
    VCPostalCodeProp,
    VCCountryNameProp,
    0
};

static const char *nameFields[] = {
    VCFamilyNameProp,
    VCGivenNameProp,
    VCAdditionalNamesProp,
    VCNamePrefixesProp,
    VCNameSuffixesProp,
    NULL
    };

static const char *orgFields[] = {
    VCOrgNameProp,
    VCOrgUnitProp,
    VCOrgUnit2Prop,
    VCOrgUnit3Prop,
    VCOrgUnit4Prop,
    NULL
    };

static const char *AAlarmFields[] = {
    VCRunTimeProp,
    VCSnoozeTimeProp,
    VCRepeatCountProp,
    VCAudioContentProp,
    0
    };

static const char *coolTalkFields[] = {
    VCCooltalkAddress,
	VCUseServer,
    0
    };

/* ExDate -- has unamed fields */
/* RDate -- has unamed fields */

static const char *DAlarmFields[] = {
    VCRunTimeProp,
    VCSnoozeTimeProp,
    VCRepeatCountProp,
    VCDisplayStringProp,
    0
    };

static const char *MAlarmFields[] = {
    VCRunTimeProp,
    VCSnoozeTimeProp,
    VCRepeatCountProp,
    VCEmailAddressProp,
    VCNoteProp,
    0
    };

static const char *PAlarmFields[] = {
    VCRunTimeProp,
    VCSnoozeTimeProp,
    VCRepeatCountProp,
    VCProcedureNameProp,
    0
    };

static struct PreDefProp propNames[] = {
    { VC7bitProp, 0, 0, 0 },
	{ VC8bitProp, 0, 0, 0 },
    { VCAAlarmProp, 0, AAlarmFields, 0 },
    { VCAdditionalNamesProp, 0, 0, 0 },
    { VCAdrProp, 0, adrFields, 0 },
    { VCAgentProp, 0, 0, 0 },
    { VCAIFFProp, 0, 0, 0 },
    { VCAOLProp, 0, 0, 0 },
    { VCAppleLinkProp, 0, 0, 0 },
	{ VCAttachProp, 0, 0, 0 },
    { VCAttendeeProp, 0, 0, 0 },
    { VCATTMailProp, 0, 0, 0 },
    { VCAudioContentProp, 0, 0, 0 },
    { VCAVIProp, 0, 0, 0 },
    { VCBase64Prop, 0, 0, 0 },
    { VCBBSProp, 0, 0, 0 },
    { VCBirthDateProp, 0, 0, 0 },
    { VCBMPProp, 0, 0, 0 },
    { VCBodyProp, 0, 0, 0 },
    { VCBusinessRoleProp, 0, 0, 0 },
    { VCCalProp, 0, 0, PD_BEGIN },
    { VCCaptionProp, 0, 0, 0 },
    { VCCardProp, 0, 0, PD_BEGIN },
    { VCCarProp, 0, 0, 0 },
    { VCCategoriesProp, 0, 0, 0 },
    { VCCellularProp, 0, 0, 0 },
    { VCCGMProp, 0, 0, 0 },
    { VCCharSetProp, 0, 0, 0 },
    { VCCIDProp, VCContentIDProp, 0, 0 },
    { VCCISProp, 0, 0, 0 },
    { VCCityProp, 0, 0, 0 },
    { VCClassProp, 0, 0, 0 },
    { VCCommentProp, 0, 0, 0 },
    { VCCompletedProp, 0, 0, 0 },
    { VCContentIDProp, 0, 0, 0 },
    { VCCountryNameProp, 0, 0, 0 },
    { VCDAlarmProp, 0, DAlarmFields, 0 },
    { VCDataSizeProp, 0, 0, PD_INTERNAL },
	{ VCDayLightProp, 0, 0 ,0 },
    { VCDCreatedProp, 0, 0, 0 },
    { VCDeliveryLabelProp, 0, 0, 0 },
    { VCDescriptionProp, 0, 0, 0 },
    { VCDIBProp, 0, 0, 0 },
    { VCDisplayStringProp, 0, 0, 0 },
    { VCDomesticProp, 0, 0, 0 },
    { VCDTendProp, 0, 0, 0 },
    { VCDTstartProp, 0, 0, 0 },
    { VCDueProp, 0, 0, 0 },
    { VCEmailAddressProp, 0, 0, 0 },
    { VCEncodingProp, 0, 0, 0 },
    { VCEndProp, 0, 0, 0 },
    { VCEventProp, 0, 0, PD_BEGIN },
    { VCEWorldProp, 0, 0, 0 },
    { VCExNumProp, 0, 0, 0 },
    { VCExpDateProp, 0, 0, 0 },
    { VCExpectProp, 0, 0, 0 },
    { VCExtAddressProp, 0, 0, 0 },
    { VCFamilyNameProp, 0, 0, 0 },
    { VCFaxProp, 0, 0, 0 },
    { VCFullNameProp, 0, 0, 0 },
    { VCGeoLocationProp, 0, 0, 0 },
    { VCGeoProp, 0, 0, 0 },
    { VCGIFProp, 0, 0, 0 },
    { VCGivenNameProp, 0, 0, 0 },
    { VCGroupingProp, 0, 0, 0 },
    { VCHomeProp, 0, 0, 0 },
    { VCIBMMailProp, 0, 0, 0 },
    { VCInlineProp, 0, 0, 0 },
    { VCInternationalProp, 0, 0, 0 },
    { VCInternetProp, 0, 0, 0 },
    { VCISDNProp, 0, 0, 0 },
    { VCJPEGProp, 0, 0, 0 },
    { VCLanguageProp, 0, 0, 0 },
    { VCLastModifiedProp, 0, 0, 0 },
    { VCLastRevisedProp, 0, 0, 0 },
    { VCLocationProp, 0, 0, 0 },
    { VCLogoProp, 0, 0, 0 },
    { VCMailerProp, 0, 0, 0 },
    { VCMAlarmProp, 0, MAlarmFields, 0 },
    { VCMCIMailProp, 0, 0, 0 },
    { VCMessageProp, 0, 0, 0 },
    { VCMETProp, 0, 0, 0 },
    { VCModemProp, 0, 0, 0 },
    { VCMPEG2Prop, 0, 0, 0 },
    { VCMPEGProp, 0, 0, 0 },
    { VCMSNProp, 0, 0, 0 },
    { VCNamePrefixesProp, 0, 0, 0 },
    { VCNameProp, 0, nameFields, 0 },
    { VCNameSuffixesProp, 0, 0, 0 },
    { VCNoteProp, 0, 0, 0 },
    { VCOrgNameProp, 0, 0, 0 },
    { VCOrgProp, 0, orgFields, 0 },
    { VCOrgUnit2Prop, 0, 0, 0 },
    { VCOrgUnit3Prop, 0, 0, 0 },
    { VCOrgUnit4Prop, 0, 0, 0 },
    { VCOrgUnitProp, 0, 0, 0 },
    { VCPagerProp, 0, 0, 0 },
    { VCPAlarmProp, 0, PAlarmFields, 0 },
    { VCParcelProp, 0, 0, 0 },
    { VCPartProp, 0, 0, 0 },
    { VCPCMProp, 0, 0, 0 },
    { VCPDFProp, 0, 0, 0 },
    { VCPGPProp, 0, 0, 0 },
    { VCPhotoProp, 0, 0, 0 },
    { VCPICTProp, 0, 0, 0 },
    { VCPMBProp, 0, 0, 0 },
    { VCPostalBoxProp, 0, 0, 0 },
    { VCPostalCodeProp, 0, 0, 0 },
    { VCPostalProp, 0, 0, 0 },
    { VCPowerShareProp, 0, 0, 0 },
    { VCPreferredProp, 0, 0, 0 },
    { VCPriorityProp, 0, 0, 0 },
    { VCProcedureNameProp, 0, 0, 0 },
    { VCProdIdProp, 0, 0, 0 },
    { VCProdigyProp, 0, 0, 0 },
    { VCPronunciationProp, 0, 0, 0 },
    { VCPSProp, 0, 0, 0 },
    { VCPublicKeyProp, 0, 0, 0 },
    { VCQPProp, VCQuotedPrintableProp, 0, 0 },
    { VCQuickTimeProp, 0, 0, 0 },
    { VCQuotedPrintableProp, 0, 0, 0 },
    { VCRDateProp, 0, 0, 0 },
    { VCRegionProp, 0, 0, 0 },
    { VCRelatedToProp, 0, 0, 0 },
    { VCRepeatCountProp, 0, 0, 0 },
    { VCResourcesProp, 0, 0, 0 },
    { VCRNumProp, 0, 0, 0 },
    { VCRoleProp, 0, 0, 0 },
    { VCRRuleProp, 0, 0, 0 },
    { VCRSVPProp, 0, 0, 0 },
    { VCRunTimeProp, 0, 0, 0 },
    { VCSequenceProp, 0, 0, 0 },
    { VCSnoozeTimeProp, 0, 0, 0 },
    { VCStartProp, 0, 0, 0 },
    { VCStatusProp, 0, 0, 0 },
    { VCStreetAddressProp, 0, 0, 0 },
    { VCSubTypeProp, 0, 0, 0 },
    { VCSummaryProp, 0, 0, 0 },
    { VCTelephoneProp, 0, 0, 0 },
    { VCTIFFProp, 0, 0, 0 },
    { VCTimeZoneProp, 0, 0, 0 },
    { VCTitleProp, 0, 0, 0 },
    { VCTLXProp, 0, 0, 0 },
    { VCTodoProp, 0, 0, PD_BEGIN },
    { VCTranspProp, 0, 0, 0 },
    { VCUniqueStringProp, 0, 0, 0 },
    { VCURLProp, 0, 0, 0 },
    { VCURLValueProp, 0, 0, 0 },
    { VCValueProp, 0, 0, 0 },
    { VCVersionProp, 0, 0, 0 },
    { VCVideoProp, 0, 0, 0 },
    { VCVoiceProp, 0, 0, 0 },
    { VCWAVEProp, 0, 0, 0 },
    { VCWMFProp, 0, 0, 0 },
    { VCWorkProp, 0, 0, 0 },
    { VCX400Prop, 0, 0, 0 },
    { VCX509Prop, 0, 0, 0 },
    { VCXRuleProp, 0, 0, 0 },
	{ VCCooltalk, 0, coolTalkFields, 0 },
	{ VCCooltalkAddress, 0, 0, 0 },
	{ VCUseServer, 0, 0, 0 },
	{ VCUseHTML, 0, 0, 0 },
    { 0,0,0,0 }
    };


static struct PreDefProp* lookupPropInfo(const char* str)
{
    /* brute force for now, could use a hash table here. */
    int i;
	
    for (i = 0; propNames[i].name; i++) 
	if (PL_strcasecmp(str, propNames[i].name) == 0) {
	    return &propNames[i];
	    }
    
    return 0;
}


const char* lookupProp_(const char* str)
{
    int i;
	
    for (i = 0; propNames[i].name; i++)
	if (PL_strcasecmp(str, propNames[i].name) == 0) {
	    const char* s;
	    s = propNames[i].alias?propNames[i].alias:propNames[i].name;
	    return lookupStr(s);
	    }
    return lookupStr(str);
}


const char* lookupProp(const char* str)
{
    int i;
	
    for (i = 0; propNames[i].name; i++)
	if (PL_strcasecmp(str, propNames[i].name) == 0) {
	    const char *s;
	    fieldedProp = (char **)propNames[i].fields;
	    s = propNames[i].alias?propNames[i].alias:propNames[i].name;
	    return lookupStr(s);
	    }
    fieldedProp = 0;
    return lookupStr(str);
}


/*----------------------------------------------------------------------
  APIs to Output text form.
  ----------------------------------------------------------------------*/
#define OFILE_REALLOC_SIZE 256
/* typedef struct OFile {
    nsOutputFileStream *fp;
    char *s;
    int len;
    int limit;
    int alloc:1;
    int fail:1;
    } OFile; */

static void appendcOFile_(OFile *fp, char c)
{
  if (fp->fail) 
    return;
  if (fp->fp) {
    fp->fp->write(&c,1);
  }
  else {
stuff:
  if (fp->len+1 < fp->limit) {
    fp->s[fp->len] = c;
    fp->len++;
    return;
  }
  else if (fp->alloc) {
    fp->limit = fp->limit + OFILE_REALLOC_SIZE;
    fp->s = (char *)PR_Realloc(fp->s,fp->limit);
    if (fp->s) goto stuff;
  }
  if (fp->alloc)
    PR_FREEIF(fp->s);
  fp->s = 0;
  fp->fail = 1;
  }
}

static void appendcOFile(OFile *fp, char c)
{
/*  int i = 0; */
    if (c == '\n') {
	/* write out as <CR><LF> */
	/* for (i = 0; i < LINEBREAK_LEN; i++)
		appendcOFile_(fp,LINEBREAK [ i ]); */
	appendcOFile_(fp,0xd); 
	appendcOFile_(fp,0xa); 
	}
    else
	appendcOFile_(fp,c);
}

static void appendsOFile(OFile *fp, const char *s)
{
    int i, slen;
    slen  = PL_strlen (s);
    for (i=0; i<slen; i++) {
	appendcOFile(fp,s[i]);
	}
}

static void initOFile(OFile *fp, nsOutputFileStream *ofp)
{
    fp->fp = ofp;
    fp->s = 0;
    fp->len = 0;
    fp->limit = 0;
    fp->alloc = 0;
    fp->fail = 0;
}

static void initMemOFile(OFile *fp, char *s, int len)
{
    fp->fp = 0;
    fp->s = s;
    fp->len = 0;
    fp->limit = s?len:0;
    fp->alloc = s?0:1;
    fp->fail = 0;
}


static int writeBase64(OFile *fp, unsigned char *s, long len)
{
    long cur = 0;
    int i, numQuads = 0;
    unsigned long trip;
    unsigned char b;
    char quad[5];
#define PR_MAXQUADS 16

    quad[4] = 0;

    while (cur < len) {
	/* collect the triplet of bytes into 'trip' */
	trip = 0;
	for (i = 0; i < 3; i++) {
	    b = (cur < len) ? *(s + cur) : 0;
	    cur++;
	    trip = trip << 8 | b;
	    }
	/* fill in 'quad' with the appropriate four characters */
	for (i = 3; i >= 0; i--) {
	    b = (unsigned char)(trip & 0x3F);
	    trip = trip >> 6;
	    if ((3 - i) < (cur - len))
		quad[i] = '='; /* pad char */
	    else if (b < 26) quad[i] = (char)b + 'A';
	    else if (b < 52) quad[i] = (char)(b - 26) + 'a';
	    else if (b < 62) quad[i] = (char)(b - 52) + '0';
	    else if (b == 62) quad[i] = '+';
	    else quad[i] = '/';
	    }
	/* now output 'quad' with appropriate whitespace and line ending */
	appendsOFile(fp, (numQuads == 0 ? "    " : ""));
	appendsOFile(fp, quad);
	appendsOFile(fp, ((cur >= len)?"\n" :(numQuads==PR_MAXQUADS-1?"\n" : "")));
	numQuads = (numQuads + 1) % PR_MAXQUADS;
	}
    appendcOFile(fp,'\n');

    return 1;
}

static void writeQPString(OFile *fp, const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
	int current_column = 0;
	static const char hexdigits[] = "0123456789ABCDEF";
	PRBool white = PR_FALSE;
	PRBool contWhite = PR_FALSE;
	PRBool mb_p = PR_FALSE;

	if (needsQuotedPrintable (s)) 
	{
		while (*p) {
			if (*p == nsCRT::CR || *p == nsCRT::LF)
			{
				/* Whitespace cannot be allowed to occur at the end of the line.
				So we encode " \n" as " =\n\n", that is, the whitespace, a
				soft line break, and then a hard line break.
				*/

				if (white)
				{
					appendcOFile(fp,'=');
					appendcOFile(fp,'\n');
					appendcOFile(fp,'\t');
					appendsOFile(fp,"=0D");
					appendsOFile(fp,"=0A");
					appendcOFile(fp,'=');
					appendcOFile(fp,'\n');
					appendcOFile(fp,'\t');
				}
				else
				{
					appendsOFile(fp,"=0D");
					appendsOFile(fp,"=0A");
					appendcOFile(fp,'=');
					appendcOFile(fp,'\n');
					appendcOFile(fp,'\t');
					contWhite = PR_FALSE;
				}

				/* If its CRLF, swallow two chars instead of one. */
				if (*p == nsCRT::CR && *(p+1) == nsCRT::LF)
					p++;
				white = PR_FALSE;
				current_column = 0;
			}
			else
			{
				if ((*p >= 33 && *p <= 60) ||		/* safe printing chars */
					(*p >= 62 && *p <= 126) ||
					(mb_p && (*p == 61 || *p == 127 || *p == 0x1B)))
				{
					appendcOFile(fp,*p);
					current_column++;
					white = PR_FALSE;
					contWhite = PR_FALSE;
				}
				else if (*p == ' ' || *p == '\t')		/* whitespace */
				{
					if (contWhite) 
					{
						appendcOFile(fp,'=');
						appendcOFile(fp,hexdigits[*p >> 4]);
						appendcOFile(fp,hexdigits[*p & 0xF]);
						current_column += 3;
						contWhite = PR_FALSE;
					}
					else
					{
						appendcOFile(fp,*p);
						current_column++;
					}
					white = PR_TRUE;
				}
				else										/* print as =FF */
				{
					appendcOFile(fp,'=');
					appendcOFile(fp,hexdigits[*p >> 4]);
					appendcOFile(fp,hexdigits[*p & 0xF]);
					current_column += 3;
					white = PR_FALSE;
					contWhite = PR_FALSE;
				}

				PR_ASSERT(current_column <= 76); /* Hard limit required by spec */

				if (current_column >= 73 || ((*(p+1) == ' ') && (current_column + 3 >= 73)))		/* soft line break: "=\r\n" */
				{
					appendcOFile(fp,'=');
					appendcOFile(fp,'\n');
					appendcOFile(fp,'\t');
					current_column = 0;
					if (white)
						contWhite = PR_TRUE;
					else 
						contWhite = PR_FALSE;
					white = PR_FALSE;
				}
			}	
			p++;
		}  /* while */
	}  /* if */
	else 
	{
	    while (*p) {
			appendcOFile(fp,*p);
			p++;
		}
	}
}


static void writeValue(OFile *fp, VObject *o, unsigned long size)
{
    if (o == 0) return;
    switch (VALUE_TYPE(o)) {
	case VCVT_USTRINGZ: {
	    char *s = fakeCString(USTRINGZ_VALUE_OF(o));
	    writeQPString(fp, s);
	    deleteString(s);
	    break;
	    }
	case VCVT_STRINGZ: {
	    writeQPString(fp, STRINGZ_VALUE_OF(o));
	    break;
	    }
	case VCVT_UINT: {
	    char buf[16];
	    sprintf(buf,"%u", INTEGER_VALUE_OF(o));
	    appendsOFile(fp,buf);
	    break;
	    }
	case VCVT_ULONG: {
	    char buf[16];
	    sprintf(buf,"%lu", LONG_VALUE_OF(o));
	    appendsOFile(fp,buf);
	    break;
	    }
	case VCVT_RAW: {
	    appendcOFile(fp,'\n');
	    writeBase64(fp,(unsigned char*)(ANY_VALUE_OF(o)),size);
	    break;
	    }
	case VCVT_VOBJECT:
	    appendcOFile(fp,'\n');
	    writeVObject_(fp,VOBJECT_VALUE_OF(o));
	    break;
	}
}

static void writeAttrValue(OFile *fp, VObject *o, int* length)
{
	int ilen = 0;
    if (NAME_OF(o)) {
	struct PreDefProp *pi;
	pi = lookupPropInfo(NAME_OF(o));
	if (pi && ((pi->flags & PD_INTERNAL) != 0)) return;
	appendcOFile(fp,';');
	if (*length != -1)
		(*length)++;
	appendsOFile(fp,NAME_OF(o));
	if (*length != -1)
		(*length) += PL_strlen (NAME_OF(o));
	}
    else {
		appendcOFile(fp,';');
		(*length)++;
	}
    if (VALUE_TYPE(o)) {
	appendcOFile(fp,'=');
	if (*length != -1) {
		(*length)++;
		for (ilen = 0; ilen < MAXMOZPROPNAMESIZE - (*length); ilen++)
			appendcOFile(fp,' ');
	}
	writeValue(fp,o,0);
	}
}

static void writeGroup(OFile *fp, VObject *o)
{
    char buf1[256];
    char buf2[256];
    PL_strcpy(buf1,NAME_OF(o));
    while ((o=isAPropertyOf(o,VCGroupingProp)) != 0) {
	PL_strcpy(buf2,STRINGZ_VALUE_OF(o));
	PL_strcat(buf2,".");
	PL_strcat(buf2,buf1);
	PL_strcpy(buf1,buf2);
	}
    appendsOFile(fp,buf1);
}

static int inList(const char **list, const char *s)
{
    if (list == 0) return 0;
    while (*list) {
	if (PL_strcasecmp(*list,s) == 0) return 1;
	list++;
	}
    return 0;
}

static void writeProp(OFile *fp, VObject *o)
{
	int length = -1;
	//int ilen = 0;

    if (NAME_OF(o)) {
	struct PreDefProp *pi;
	VObjectIterator t;
	const char **fields_ = 0;
	pi = lookupPropInfo(NAME_OF(o));
	if (pi && ((pi->flags & PD_BEGIN) != 0)) {
	    writeVObject_(fp,o);
	    return;
	    }
	if (isAPropertyOf(o,VCGroupingProp))
	    writeGroup(fp,o);
	else 
	    appendsOFile(fp,NAME_OF(o));
	if (pi) fields_ = pi->fields;
	initPropIterator(&t,o);
	while (moreIteration(&t)) {
	    const char *s;
	    VObject *eachProp = nextVObject(&t);
	    s = NAME_OF(eachProp);
	    if (PL_strcasecmp(VCGroupingProp,s) && !inList(fields_,s))
		writeAttrValue(fp,eachProp, &length);
	    }
	if (fields_) {
	    int i = 0, n = 0;
	    const char** fields = fields_;
	    /* output prop as fields */
	    appendcOFile(fp,':');
	    while (*fields) {
		VObject *tt = isAPropertyOf(o,*fields);
		i++;
		if (tt) n = i;
		fields++;
		}
	    fields = fields_;
	    for (i=0;i<n;i++) {
		writeValue(fp,isAPropertyOf(o,*fields),0);
		fields++;
		if (i<(n-1)) appendcOFile(fp,';');
		}
	    }
	}
	
    if (VALUE_TYPE(o)) {
	unsigned long size = 0;
        VObject *p = isAPropertyOf(o,VCDataSizeProp);
	if (p) size = LONG_VALUE_OF(p);
	appendcOFile(fp,':');
	writeValue(fp,o,size);
	}
    appendcOFile(fp,'\n');
}

void writeVObject_(OFile *fp, VObject *o)
{
  //int ilen = 0;
    if (NAME_OF(o)) {
	struct PreDefProp *pi;
	pi = lookupPropInfo(NAME_OF(o));

	if (pi && ((pi->flags & PD_BEGIN) != 0)) {
	    VObjectIterator t;
	    const char *begin = NAME_OF(o);
	    appendsOFile(fp,"begin:");
	    appendsOFile(fp,begin);
	    appendcOFile(fp,'\n');
	    initPropIterator(&t,o);
	    while (moreIteration(&t)) {
			VObject *eachProp = nextVObject(&t);
			writeProp(fp, eachProp);
		}
	    appendsOFile(fp,"end:");
	    appendsOFile(fp,begin);
	    appendsOFile(fp,"\n\n");
	    }
	}
}

void writeVObject(nsOutputFileStream *fp, VObject *o)
{
    OFile ofp;
    initOFile(&ofp,fp);
    writeVObject_(&ofp,o);
}

void writeVObjectToFile(nsFileSpec *fname, VObject *o)
{
#if !defined(MOZADDRSTANDALONE)
  nsOutputFileStream *fp = new nsOutputFileStream(*fname, PR_WRONLY | PR_CREATE_FILE, 00600);
  
  if (fp) {
    writeVObject(fp,o);
    fp->close();
  }
#else
  PR_ASSERT(PR_FALSE);
#endif
}

void writeVObjectsToFile(nsFileSpec *fname, VObject *list)
{
#if !defined(MOZADDRSTANDALONE)
  nsOutputFileStream *fp = new nsOutputFileStream(*fname, PR_WRONLY | PR_CREATE_FILE, 00600);
    if (fp) {
	while (list) {
	    writeVObject(fp,list);
	    list = nextVObjectInList(list);
	    }
	fp->close();
	}
#else
	PR_ASSERT(PR_FALSE);
#endif
}

char* writeMemVObject(char *s, int *len, VObject *o)
{
    OFile ofp;
    initMemOFile(&ofp,s,len?*len:0);
    writeVObject_(&ofp,o);
    if (len) *len = ofp.len;
    appendcOFile(&ofp,0);
    return ofp.s;
}

extern "C" 
char * writeMemoryVObjects(char *s, int *len, VObject *list, PRBool expandSpaces)
{
    OFile ofp;
    initMemOFile(&ofp,s,len?*len:0);
    while (list) {
	writeVObject_(&ofp,list);
	list = nextVObjectInList(list);
	}
    if (len) *len = ofp.len;
    appendcOFile(&ofp,0);
    return ofp.s;
}

/*----------------------------------------------------------------------
  APIs to do fake Unicode stuff.
  ----------------------------------------------------------------------*/
vwchar_t* fakeUnicode(const char *ps, int *bytes)
{
    vwchar_t *r, *pw;
    int len = strlen(ps)+1;

    pw = r = (vwchar_t*)PR_CALLOC(sizeof(vwchar_t)*len);
    if (bytes)
	*bytes = len * sizeof(vwchar_t);

    while (*ps) { 
	if (*ps == '\n')
	    *pw = (vwchar_t)0x2028;
	else if (*ps == '\r')
	    *pw = (vwchar_t)0x2029;
	else
	    *pw = (vwchar_t)(unsigned char)*ps;
	ps++; pw++;
	}				 
    *pw = (vwchar_t)0;
	
    return r;
}

int uStrLen(const vwchar_t *u)
{
    int i = 0;
    while (*u != (vwchar_t)0) { u++; i++; }
    return i;
}

char* fakeCString(const vwchar_t *u)
{
    char *s, *t;
    int len = uStrLen(u) + 1;
    t = s = (char*)PR_CALLOC(len);
    while (*u) {
	if (*u == (vwchar_t)0x2028)
	    *t = '\n';
	else if (*u == (vwchar_t)0x2029)
	    *t = '\r';
	else
	    *t = (char)*u;
	u++; t++;
	}
    *t = 0;
    return s;
}

const char* lookupStr(const char *s)
{
    StrItem *t;
    unsigned int h = hashStr(s);
    if ((t = strTbl[h]) != 0) {
	do {
	    if (PL_strcasecmp(t->s,s) == 0) {
		t->refCnt++;
		return t->s;
		}
	    t = t->next;
	    } while (t);
	}
    s = dupStr(s,0);
    strTbl[h] = newStrItem(s,strTbl[h]);
    return s;
}
