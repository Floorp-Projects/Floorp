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
 
The vCard/vCalendar C interface is implemented in the set 
of files as follows:

vcc.y, yacc source, and vcc.c, the yacc output you will use
implements the core parser

vobject.c implements an API that insulates the caller from
the parser and changes in the vCard/vCalendar BNF

port.h defines compilation environment dependent stuff

vcc.h and vobject.h are header files for their .c counterparts

vcaltmp.h and vcaltmp.c implement vCalendar "macro" functions
which you may find useful.

test.c is a standalone test driver that exercises some of
the features of the APIs provided. Invoke test.exe on a
VCARD/VCALENDAR input text file and you will see the pretty
print output of the internal representation (this pretty print
output should give you a good idea of how the internal 
representation looks like -- there is one such output in the 
following too). Also, a file with the .out suffix is generated 
to show that the internal representation can be written back 
in the original text format.

For more information on this API see the readme.txt file
which accompanied this distribution.

  Also visit:

		http://www.versit.com
		http://www.ralden.com

*/


#ifndef __VOBJECT_H__
#define __VOBJECT_H__ 1

#include "xp.h"

/*
Unfortunately, on the Mac (and possibly other platforms) with our current, out-dated
libraries (Plauger), |wchar_t| is defined incorrectly, which breaks vcards.

We can't fix Plauger because it doesn't come with source. Later, when we
upgrade to MSL, we can make this evil hack go away.  In the mean time,
vcards are not allowed to use the (incorrectly defined) |wchar_t| type.  Instead,
they will use an appropriately defined local type |vwchar_t|.
*/

#ifdef XP_MAC
        typedef uint16  vwchar_t; 
#else
        typedef wchar_t vwchar_t;
#endif


#define VC7bitProp				"7bit"
#define VC8bitProp				"8bit"
#define VCAAlarmProp			"aalarm"
#define VCAdditionalNamesProp	"addn"
#define VCAdrProp				"adr"
#define VCAgentProp				"agent"
#define VCAIFFProp				"aiff"
#define VCAOLProp				"aol"
#define VCAppleLinkProp			"applelink"
#define VCAttachProp			"attach"
#define VCAttendeeProp			"attendee"
#define VCATTMailProp			"attmail"
#define VCAudioContentProp		"audiocontent"
#define VCAVIProp				"avi"
#define VCBase64Prop			"base64"
#define VCBBSProp				"bbs"
#define VCBirthDateProp			"bday"
#define VCBMPProp				"bmp"
#define VCBodyProp				"body"
#define VCBusinessRoleProp		"role"
#define VCCalProp				"vcalendar"
#define VCCaptionProp			"cap"
#define VCCardProp				"vcard"
#define VCCarProp				"car"
#define VCCategoriesProp		"categories"
#define VCCellularProp			"cell"
#define VCCGMProp				"cgm"
#define VCCharSetProp			"cs"
#define VCCIDProp				"cid"
#define VCCISProp				"cis"
#define VCCityProp				"l"
#define VCClassProp				"class"
#define VCCommentProp			"note"
#define VCCompletedProp			"completed"
#define VCContentIDProp			"content-id"
#define VCCountryNameProp		"c"
#define VCDAlarmProp			"dalarm"
#define VCDataSizeProp			"datasize"
#define VCDayLightProp			"daylight"
#define VCDCreatedProp			"dcreated"
#define VCDeliveryLabelProp     "label"
#define VCDescriptionProp		"description"
#define VCDIBProp				"dib"
#define VCDisplayStringProp		"displaystring"
#define VCDomesticProp			"dom"
#define VCDTendProp				"dtend"
#define VCDTstartProp			"dtstart"
#define VCDueProp				"due"
#define VCEmailAddressProp		"email"
#define VCEncodingProp			"encoding"
#define VCEndProp				"end"
#define VCEventProp				"vevent"
#define VCEWorldProp			"eworld"
#define VCExNumProp				"exnum"
#define VCExpDateProp			"exdate"
#define VCExpectProp			"expect"
#define VCExtAddressProp		"ext add"
#define VCFamilyNameProp		"f"
#define VCFaxProp				"fax"
#define VCFullNameProp			"fn"
#define VCGeoProp				"geo"
#define VCGeoLocationProp		"geo"
#define VCGIFProp				"gif"
#define VCGivenNameProp			"g"
#define VCGroupingProp			"grouping"
#define VCHomeProp				"home"
#define VCIBMMailProp			"ibmmail"
#define VCInlineProp			"inline"
#define VCInternationalProp		"intl"
#define VCInternetProp			"internet"
#define VCISDNProp				"isdn"
#define VCJPEGProp				"jpeg"
#define VCLanguageProp			"lang"
#define VCLastModifiedProp		"last-modified"
#define VCLastRevisedProp		"rev"
#define VCLocationProp			"location"
#define VCLogoProp				"logo"
#define VCMailerProp			"mailer"
#define VCMAlarmProp			"malarm"
#define VCMCIMailProp			"mcimail"
#define VCMessageProp			"msg"
#define VCMETProp				"met"
#define VCModemProp				"modem"
#define VCMPEG2Prop				"mpeg2"
#define VCMPEGProp				"mpeg"
#define VCMSNProp				"msn"
#define VCNamePrefixesProp		"npre"
#define VCNameProp				"n"
#define VCNameSuffixesProp		"nsuf"
#define VCNoteProp				"note"
#define VCOrgNameProp			"orgname"
#define VCOrgProp				"org"
#define VCOrgUnit2Prop			"oun2"
#define VCOrgUnit3Prop			"oun3"
#define VCOrgUnit4Prop			"oun4"
#define VCOrgUnitProp			"oun"
#define VCPagerProp				"pager"
#define VCPAlarmProp			"palarm"
#define VCParcelProp			"parcel"
#define VCPartProp				"part"
#define VCPCMProp				"pcm"
#define VCPDFProp				"pdf"
#define VCPGPProp				"pgp"
#define VCPhotoProp				"photo"
#define VCPICTProp				"pict"
#define VCPMBProp				"pmb"
#define VCPostalBoxProp			"box"
#define VCPostalCodeProp		"pc"
#define VCPostalProp			"postal"
#define VCPowerShareProp		"powershare"
#define VCPreferredProp			"pref"
#define VCPriorityProp			"priority"
#define VCProcedureNameProp		"procedurename"
#define VCProdIdProp			"prodid"
#define VCProdigyProp			"prodigy"
#define VCPronunciationProp		"sound"
#define VCPSProp				"ps"
#define VCPublicKeyProp			"key"
#define VCQPProp				"qp"
#define VCQuickTimeProp			"qtime"
#define VCQuotedPrintableProp	"quoted-printable"
#define VCRDateProp				"rdate"
#define VCRegionProp			"r"
#define VCRelatedToProp			"related-to"
#define VCRepeatCountProp		"repeatcount"
#define VCResourcesProp			"resources"
#define VCRNumProp				"rnum"
#define VCRoleProp				"role"
#define VCRRuleProp				"rrule"
#define VCRSVPProp				"rsvp"
#define VCRunTimeProp			"runtime"
#define VCSequenceProp			"sequence"
#define VCSnoozeTimeProp		"snoozetime"
#define VCStartProp				"start"
#define VCStatusProp			"status"
#define VCStreetAddressProp		"street"
#define VCSubTypeProp			"subtype"
#define VCSummaryProp			"summary"
#define VCTelephoneProp			"tel"
#define VCTIFFProp				"tiff"
#define VCTimeZoneProp			"tz"
#define VCTitleProp				"title"
#define VCTLXProp				"tlx"
#define VCTodoProp				"vtodo"
#define VCTranspProp			"transp"
#define VCUniqueStringProp		"uid"
#define VCURLProp				"url"
#define VCURLValueProp			"urlval"
#define VCValueProp				"value"
#define VCVersionProp			"version"
#define VCVideoProp				"video"
#define VCVoiceProp				"voice"
#define VCWAVEProp				"wave"
#define VCWMFProp				"wmf"
#define VCWorkProp				"work"
#define VCX400Prop				"x400"
#define VCX509Prop				"x509"
#define VCXRuleProp				"xrule"
#define VCCooltalk				"x-mozilla-cpt"
#define VCCooltalkAddress		"x-moxilla-cpadr"
#define	VCUseServer				"x-mozilla-cpsrv"
#define VCUseHTML				"x-mozilla-html"

/* return type of vObjectValueType: */
#define VCVT_NOVALUE	0
	/* if the VObject has no value associated with it. */
#define VCVT_STRINGZ	1
	/* if the VObject has value set by setVObjectStringZValue. */
#define VCVT_USTRINGZ	2
	/* if the VObject has value set by setVObjectUStringZValue. */
#define VCVT_UINT		3
	/* if the VObject has value set by setVObjectIntegerValue. */
#define VCVT_ULONG		4
	/* if the VObject has value set by setVObjectLongValue. */
#define VCVT_RAW		5
	/* if the VObject has value set by setVObjectAnyValue. */
#define VCVT_VOBJECT	6
	/* if the VObject has value set by setVObjectVObjectValue. */

#define NAME_OF(o)				o->id
#define VALUE_TYPE(o)			o->valType
#define STRINGZ_VALUE_OF(o)		o->val.strs
#define USTRINGZ_VALUE_OF(o)	o->val.ustrs
#define INTEGER_VALUE_OF(o)		o->val.i
#define LONG_VALUE_OF(o)		o->val.l
#define ANY_VALUE_OF(o)			o->val.any
#define VOBJECT_VALUE_OF(o)		o->val.vobj

typedef struct VObject VObject;

typedef union ValueItem {
    const char *strs;
    const vwchar_t *ustrs;
    unsigned int i;
    unsigned long l;
    void *any;
    VObject *vobj;
    } ValueItem;

struct VObject {
    VObject *next;
    const char *id;
    VObject *prop;
    unsigned short valType;
    ValueItem val;
    }; 

typedef struct StrItem StrItem;

struct StrItem {
    StrItem *next;
    const char *s;
    unsigned int refCnt;
    };

typedef struct OFile {
    PRFileDesc *fp;
    char *s;
    int len;
    int limit;
    int alloc:1;
    int fail:1;
    } OFile;

typedef struct VObjectIterator {
    VObject* start;
    VObject* next;
    } VObjectIterator;


extern "C" VObject*		newVObject(const char *id);
void			deleteVObject(VObject *p);
extern "C" char*			dupStr(const char *s, unsigned int size);
extern "C" void			deleteStr(const char *p);
void			unUseStr(const char *s);

void			setVObjectName(VObject *o, const char* id);
void			setVObjectStringZValue(VObject *o, const char *s);
void			setVObjectStringZValue_(VObject *o, const char *s);
void			setVObjectUStringZValue(VObject *o, const vwchar_t *s);
void			setVObjectUStringZValue_(VObject *o, const vwchar_t *s);
void			setVObjectIntegerValue(VObject *o, unsigned int i);
void			setVObjectLongValue(VObject *o, unsigned long l);
void			setVObjectAnyValue(VObject *o, void *t);
extern "C" VObject*		setValueWithSize(VObject *prop, void *val, unsigned int size);
VObject*		setValueWithSize_(VObject *prop, void *val, unsigned int size);

extern "C" const char* vObjectName(VObject *o);
const char* vObjectStringZValue(VObject *o);
extern "C" const vwchar_t* vObjectUStringZValue(VObject *o);
unsigned int vObjectIntegerValue(VObject *o);
unsigned long vObjectLongValue(VObject *o);
extern "C" void* vObjectAnyValue(VObject *o);
VObject* vObjectVObjectValue(VObject *o);
void setVObjectVObjectValue(VObject *o, VObject *p);

VObject* addVObjectProp(VObject *o, VObject *p);
extern "C" VObject* addProp(VObject *o, const char *id);
VObject* addProp_(VObject *o, const char *id);
VObject* addPropValue(VObject *o, const char *p, const char *v);
VObject* addPropSizedValue_(VObject *o, const char *p, const char *v, unsigned int size);
VObject* addPropSizedValue(VObject *o, const char *p, const char *v, unsigned int size);
VObject* addGroup(VObject *o, const char *g);
extern "C" void addList(VObject **o, VObject *p);

extern "C" VObject* isAPropertyOf(VObject *o, const char *id);

extern "C" VObject* nextVObjectInList(VObject *o);
extern "C" void initPropIterator(VObjectIterator *i, VObject *o);
extern "C" int moreIteration(VObjectIterator *i);
extern "C" VObject* nextVObject(VObjectIterator *i);

extern void printVObject(PRFileDesc *fp,VObject *o);
void printVObject_(PRFileDesc * fp, VObject *o, int level);
extern void writeVObject(PRFileDesc * fp, VObject *o);
void writeVObject_(OFile *fp, VObject *o);
extern "C" char* writeMemVObject(char *s, int *len, VObject *o);
extern "C" char* writeMemVObjects(char *s, int *len, VObject *list);

const char* lookupStr(const char *s);

void cleanStrTbl();

extern "C" void cleanVObject(VObject *o);
void cleanVObjects(VObject *list);

const char* lookupProp(const char* str);
const char* lookupProp_(const char* str);

extern "C" vwchar_t* fakeUnicode(const char *ps, int *bytes);
int uStrLen(const vwchar_t *u);
extern "C" char* fakeCString(const vwchar_t *u);

void printVObjectToFile(char *fname,VObject *o);
void printVObjectsToFile(char *fname,VObject *list);
void writeVObjectToFile(char *fname, VObject *o);
void writeVObjectsToFile(char *fname, VObject *list);

#define MAXPROPNAMESIZE	256
#define MAXMOZPROPNAMESIZE 16

#endif /* __VOBJECT_H__ */


