/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
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
 * keyword.h
 * John Sun
 * 3/4/98 4:57:30 PM
 */

#include <unistring.h>
#include "jatom.h"
#include "nscalutilexp.h"

#ifndef __KEYWORD_H_
#define __KEYWORD_H_

/**
 * singleton class to contain all ICAL keywords
 */
class NS_CAL_UTIL JulianKeyword
{
private:
    static JulianKeyword * m_Instance;
    JulianKeyword();
        
public:
    static JulianKeyword * Instance();
    ~JulianKeyword();

    /* iCALENDAR KEYWORDS*/
    UnicodeString ms_sVCALENDAR;   
    JAtom ms_ATOM_VCALENDAR;

    /* iCALENDAR COMPONENTS KEYWORDS */
    UnicodeString ms_sVEVENT;      
    JAtom ms_ATOM_VEVENT; 

    UnicodeString ms_sVTODO;       
    JAtom ms_ATOM_VTODO;
    UnicodeString ms_sVJOURNAL;    
    JAtom ms_ATOM_VJOURNAL;
    UnicodeString ms_sVFREEBUSY;   
    JAtom ms_ATOM_VFREEBUSY;
    UnicodeString ms_sVTIMEZONE;   
    JAtom ms_ATOM_VTIMEZONE;
    UnicodeString ms_sVALARM;      
    JAtom ms_ATOM_VALARM;
    UnicodeString ms_sTZPART;
    JAtom ms_ATOM_TZPART;
    UnicodeString ms_sTIMEBASEDEVENT;
    UnicodeString ms_sNSCALENDAR;

    /* iCalendar COMPONENT PROPERTY KEYWORDS */
    UnicodeString ms_sATTENDEE;      
    JAtom ms_ATOM_ATTENDEE;
    
    UnicodeString ms_sATTACH;        
    JAtom ms_ATOM_ATTACH;

    UnicodeString ms_sCATEGORIES;
    JAtom ms_ATOM_CATEGORIES;

    UnicodeString ms_sCLASS;
    JAtom ms_ATOM_CLASS;

    UnicodeString ms_sCOMMENT;
    JAtom ms_ATOM_COMMENT;

    UnicodeString ms_sCOMPLETED;
    JAtom ms_ATOM_COMPLETED;

    UnicodeString ms_sCONTACT;
    JAtom ms_ATOM_CONTACT;

    UnicodeString ms_sCREATED;
    JAtom ms_ATOM_CREATED;

    UnicodeString ms_sDTEND;
    JAtom ms_ATOM_DTEND;

    UnicodeString ms_sDTSTART;
    JAtom ms_ATOM_DTSTART;

    UnicodeString ms_sDTSTAMP; 
    JAtom ms_ATOM_DTSTAMP;

    UnicodeString ms_sDESCRIPTION;
    JAtom ms_ATOM_DESCRIPTION;

    UnicodeString ms_sDUE;
    JAtom ms_ATOM_DUE;

    UnicodeString ms_sDURATION;
    JAtom ms_ATOM_DURATION;

    UnicodeString ms_sEXDATE;
    JAtom ms_ATOM_EXDATE;

    UnicodeString ms_sEXRULE;
    JAtom ms_ATOM_EXRULE;

    UnicodeString ms_sFREEBUSY;
    JAtom ms_ATOM_FREEBUSY;

    UnicodeString ms_sGEO;
    JAtom ms_ATOM_GEO;

    UnicodeString ms_sLASTMODIFIED;
    JAtom ms_ATOM_LASTMODIFIED;

    UnicodeString ms_sLOCATION;
    JAtom ms_ATOM_LOCATION;

    UnicodeString ms_sORGANIZER;
    JAtom ms_ATOM_ORGANIZER;

    UnicodeString ms_sPERCENTCOMPLETE;
    JAtom ms_ATOM_PERCENTCOMPLETE;

    UnicodeString ms_sPRIORITY;
    JAtom ms_ATOM_PRIORITY;

    UnicodeString ms_sRDATE;
    JAtom ms_ATOM_RDATE;

    UnicodeString ms_sRRULE;
    JAtom ms_ATOM_RRULE;

    UnicodeString ms_sRECURRENCEID;
    JAtom ms_ATOM_RECURRENCEID;

    /* UnicodeString ms_sRESPONSESEQUENCE = "RESPONSE-SEQUENCE"; */
    /* JAtom ms_ATOM_RESPONSESEQUENCE; */
    
    UnicodeString ms_sRELATEDTO;
    JAtom ms_ATOM_RELATEDTO;

    UnicodeString ms_sREPEAT;
    JAtom ms_ATOM_REPEAT;
 
    UnicodeString ms_sREQUESTSTATUS;
    JAtom ms_ATOM_REQUESTSTATUS;
    
    UnicodeString ms_sRESOURCES;
    JAtom ms_ATOM_RESOURCES;
 
    UnicodeString ms_sSEQUENCE;
    JAtom ms_ATOM_SEQUENCE;
 
    UnicodeString ms_sSTATUS;
    JAtom ms_ATOM_STATUS;
 
    UnicodeString ms_sSUMMARY;
    JAtom ms_ATOM_SUMMARY;
 
    UnicodeString ms_sTRANSP;
    JAtom ms_ATOM_TRANSP;
 
    UnicodeString ms_sTRIGGER;
    JAtom ms_ATOM_TRIGGER;
 
    UnicodeString ms_sUID;
    JAtom ms_ATOM_UID;
 
    UnicodeString ms_sURL;
    JAtom ms_ATOM_URL;
 
    UnicodeString ms_sTZOFFSET;
    JAtom ms_ATOM_TZOFFSET;

    UnicodeString ms_sTZOFFSETTO;
    JAtom ms_ATOM_TZOFFSETTO;
 
    UnicodeString ms_sTZOFFSETFROM;
    JAtom ms_ATOM_TZOFFSETFROM;
 
    UnicodeString ms_sTZNAME;
    JAtom ms_ATOM_TZNAME;
 
    UnicodeString ms_sDAYLIGHT;
    JAtom ms_ATOM_DAYLIGHT;
 
    UnicodeString ms_sSTANDARD;
    JAtom ms_ATOM_STANDARD;

    UnicodeString ms_sTZURL;
    JAtom ms_ATOM_TZURL;

    UnicodeString ms_sTZID;
    JAtom ms_ATOM_TZID;

    /* Boolean value KEYWORDS */
    UnicodeString ms_sTRUE;
    JAtom ms_ATOM_TRUE;
    
    UnicodeString ms_sFALSE;
    JAtom ms_ATOM_FALSE;
  
    /* ITIP METHOD NAME KEYWORDS */
    UnicodeString ms_sPUBLISH;
    JAtom ms_ATOM_PUBLISH;

    UnicodeString ms_sREQUEST;
    JAtom ms_ATOM_REQUEST;

    UnicodeString ms_sREPLY;
    JAtom ms_ATOM_REPLY;

    UnicodeString ms_sCANCEL;
    JAtom ms_ATOM_CANCEL;

    UnicodeString ms_sREFRESH;
    JAtom ms_ATOM_REFRESH;

    UnicodeString ms_sCOUNTER;
    JAtom ms_ATOM_COUNTER;

    UnicodeString ms_sDECLINECOUNTER;
    JAtom ms_ATOM_DECLINECOUNTER;

    UnicodeString ms_sADD;
    JAtom ms_ATOM_ADD;

    /* not really actual method names */
    UnicodeString ms_sDELEGATEREQUEST;
    JAtom ms_ATOM_DELEGATEREQUEST;

    UnicodeString ms_sDELEGATEREPLY;
    JAtom ms_ATOM_DELEGATEREPLY;

    /* NSCALENDAR */
    /* NSCalendar PROPERTY NAMES KEYWORDS */

    UnicodeString ms_sPRODID;
    JAtom ms_ATOM_PRODID;

    UnicodeString ms_sVERSION;
    JAtom ms_ATOM_VERSION;

    UnicodeString ms_sMETHOD;
    JAtom ms_ATOM_METHOD;

    UnicodeString ms_sSOURCE;
    JAtom ms_ATOM_SOURCE;

    UnicodeString ms_sCALSCALE;
    JAtom ms_ATOM_CALSCALE;

    UnicodeString ms_sNAME;
    JAtom ms_ATOM_NAME;

    /* Valid calscale value KEYWORDS, also accepts Iana-scale */
    UnicodeString ms_sGREGORIAN;
    JAtom ms_ATOM_GREGORIAN;
  
    /* End NSCalendar */
  
    /* TimeBasedEvent */
    /* CLASS property Keywords */

    UnicodeString ms_sPUBLIC;
    JAtom ms_ATOM_PUBLIC;

    UnicodeString ms_sPRIVATE;
    JAtom ms_ATOM_PRIVATE;

    UnicodeString ms_sCONFIDENTIAL;
    JAtom ms_ATOM_CONFIDENTIAL;
  
    /* STATUS property Keywords */
    UnicodeString ms_sNEEDSACTION;
    JAtom ms_ATOM_NEEDSACTION;

    /*UnicodeString ms_sCOMPLETED;*/
    /*JAtom ms_ATOM_COMPLETED;*/

    UnicodeString ms_sINPROCESS;
    JAtom ms_ATOM_INPROCESS;

    UnicodeString ms_sTENTATIVE;
    JAtom ms_ATOM_TENTATIVE;

    UnicodeString ms_sCONFIRMED;
    JAtom ms_ATOM_CONFIRMED;

    UnicodeString ms_sCANCELLED;
    JAtom ms_ATOM_CANCELLED;

    UnicodeString ms_sDRAFT;
    JAtom ms_ATOM_DRAFT;

    UnicodeString ms_sFINAL;
    JAtom ms_ATOM_FINAL;


    /* TRANSP property Keywords */
    UnicodeString ms_sOPAQUE;
    JAtom ms_ATOM_OPAQUE;

    UnicodeString ms_sTRANSPARENT;
    JAtom ms_ATOM_TRANSPARENT;
  
    /* End TimeBasedEvent */

    /* ATTENDEE */
    /* parameter names as in the input stream. Define for possible future changes */
    UnicodeString ms_sROLE;
    JAtom ms_ATOM_ROLE;

    /* replaces TYPE */
    UnicodeString ms_sCUTYPE;
    JAtom ms_ATOM_CUTYPE;

    UnicodeString ms_sTYPE;
    JAtom ms_ATOM_TYPE;
    
    UnicodeString ms_sCN;
    JAtom ms_ATOM_CN;

    UnicodeString ms_sPARTSTAT;
    JAtom ms_ATOM_PARTSTAT;

    UnicodeString ms_sRSVP;
    JAtom ms_ATOM_RSVP;

    UnicodeString ms_sEXPECT;
    JAtom ms_ATOM_EXPECT;

    UnicodeString ms_sMEMBER;
    JAtom ms_ATOM_MEMBER;

    UnicodeString ms_sDELEGATED_TO;
    JAtom ms_ATOM_DELEGATED_TO;

    UnicodeString ms_sDELEGATED_FROM;
    JAtom ms_ATOM_DELEGATED_FROM;

    UnicodeString ms_sSENTBY;
    JAtom ms_ATOM_SENTBY;

    UnicodeString ms_sDIR;
    JAtom ms_ATOM_DIR;

    /* ROLE Keywords */
    /*UnicodeString ms_sATTENDEE;*/
    /*JAtom ms_ATOM_ATTENDEE;*/
    /*UnicodeString ms_sORGANIZER;*/
    /*JAtom ms_ATOM_ORGANIZER;*/
    /*UnicodeString ms_sOWNER;*/
    /*JAtom ms_ATOM_OWNER;*/
    /*UnicodeString ms_sDELEGATE;*/
    /*JAtom ms_ATOM_DELEGATE;*/
    UnicodeString ms_sCHAIR;
    JAtom ms_ATOM_CHAIR;
    
    /*UnicodeString ms_sPARTICIPANT;*/
    /*JAtom ms_ATOM_PARTICIPANT;*/
   
    UnicodeString ms_sREQ_PARTICIPANT;
    JAtom ms_ATOM_REQ_PARTICIPANT;
    
    UnicodeString ms_sOPT_PARTICIPANT;
    JAtom ms_ATOM_OPT_PARTICIPANT;

    UnicodeString ms_sNON_PARTICIPANT;
    JAtom ms_ATOM_NON_PARTICIPANT;
  
    /* TYPE Keywords */
    UnicodeString ms_sUNKNOWN;
    JAtom ms_ATOM_UNKNOWN;
    
    UnicodeString ms_sINDIVIDUAL;
    JAtom ms_ATOM_INDIVIDUAL;

    UnicodeString ms_sGROUP;
    JAtom ms_ATOM_GROUP;

    UnicodeString ms_sRESOURCE;
    JAtom ms_ATOM_RESOURCE;

    UnicodeString ms_sROOM;
    JAtom ms_ATOM_ROOM;

    /* STATUS Keywords */
    /*UnicodeString ms_sNEEDSACTION = "NEEDS-ACTION"; */
    /*JAtom ms_ATOM_NEEDSACTION;*/
    
    UnicodeString ms_sACCEPTED; 
    JAtom ms_ATOM_ACCEPTED;

    UnicodeString ms_sDECLINED;
    JAtom ms_ATOM_DECLINED;

    /*UnicodeString ms_sTENTATIVE;
    JAtom ms_ATOM_TENTATIVE;

    UnicodeString ms_sCONFIRMED;
    JAtom ms_ATOM_CONFIRMED;

    UnicodeString ms_sCOMPLETED;
    JAtom ms_ATOM_COMPLETED;*/

    UnicodeString ms_sDELEGATED;
    JAtom ms_ATOM_DELEGATED;

    /*UnicodeString ms_sCANCELLED;
    JAtom ms_ATOM_CANCELLED;*/

    UnicodeString ms_sVCALNEEDSACTION;
    JAtom ms_ATOM_VCALNEEDSACTION;
  
    /* RSVP Keywords */
    /*UnicodeString ms_sFALSE;
    JAtom ms_ATOM_FALSE;

    UnicodeString ms_sTRUE;
    JAtom ms_ATOM_TRUE;*/

   /*  EXPECT Keywords*/
    UnicodeString ms_sFYI;
    JAtom ms_ATOM_FYI;
    
    UnicodeString ms_sREQUIRE;
    JAtom ms_ATOM_REQUIRE;

    /*UnicodeString ms_sREQUEST;
    JAtom ms_ATOM_REQUEST;*/

    UnicodeString ms_sIMMEDIATE;
    JAtom ms_ATOM_IMMEDIATE;

    /*End ATTENDEE*/

    /*RecurrenceID*/
    UnicodeString ms_sRANGE;
    JAtom ms_ATOM_RANGE;

    UnicodeString ms_sTHISANDPRIOR;
    JAtom ms_ATOM_THISANDPRIOR;

    UnicodeString ms_sTHISANDFUTURE;
    JAtom ms_ATOM_THISANDFUTURE;

    /*End RecurrenceID*/

    /* FREEBUSY */
    UnicodeString ms_sFBTYPE;
    JAtom ms_ATOM_FBTYPE;

    UnicodeString ms_sBSTAT; /* renamed from status */
    JAtom ms_ATOM_BSTAT;

    UnicodeString ms_sBUSY;
    JAtom ms_ATOM_BUSY;

    UnicodeString ms_sFREE;
    JAtom ms_ATOM_FREE;

    UnicodeString ms_sBUSY_UNAVAILABLE;
    JAtom ms_ATOM_BUSY_UNAVAILABLE;

    UnicodeString ms_sBUSY_TENTATIVE;
    JAtom ms_ATOM_BUSY_TENTATIVE;

    UnicodeString ms_sUNAVAILABLE;
    JAtom ms_ATOM_UNAVAILABLE;

    /*UnicodeString ms_sTENTATIVE;
    JAtom ms_ATOM_TENTATIVE;*/

    /*End FREEBUSY*/
  
    /* ALARM */
    /* Alarm prop keywords */
    UnicodeString ms_sACTION;        
    JAtom ms_ATOM_ACTION;

    /* Alarm Categories keywords */
    UnicodeString ms_sAUDIO;
    JAtom ms_ATOM_AUDIO;

    UnicodeString ms_sDISPLAY;
    JAtom ms_ATOM_DISPLAY;
    
    UnicodeString ms_sEMAIL;
    JAtom ms_ATOM_EMAIL;

    UnicodeString ms_sPROCEDURE;
    JAtom ms_ATOM_PROCEDURE;

    /* End ALARM */
    /*DESCRIPTION*/
    UnicodeString ms_sENCODING;
    JAtom ms_ATOM_ENCODING;

    UnicodeString ms_sCHARSET;
    JAtom ms_ATOM_CHARSET;

    UnicodeString ms_sQUOTED_PRINTABLE;
    JAtom ms_ATOM_QUOTED_PRINTABLE;

    /* End DESCRIPTION */

    /* PARAMETER NAME, VALUE KEYWORDS */
    /*UnicodeString ms_sENCODING;
    JAtom ms_ATOM_ENCODING;*/

    UnicodeString ms_sVALUE;
    JAtom ms_ATOM_VALUE;
    
    UnicodeString ms_sLANGUAGE;
    JAtom ms_ATOM_LANGUAGE;

    UnicodeString ms_sALTREP;
    JAtom ms_ATOM_ALTREP;

    /*UnicodeString ms_sSENTBY;
    JAtom ms_ATOM_SENTBY;*/

    UnicodeString ms_sRELTYPE;
    JAtom ms_ATOM_RELTYPE;

    UnicodeString ms_sFMTTYPE;
    JAtom ms_ATOM_FMTTYPE;

    UnicodeString ms_sRELATED;
    JAtom ms_ATOM_RELATED;

    /*UnicodeString ms_sTZID;
    JAtom ms_ATOM_TZID;*/

    /* Reltype*/
    UnicodeString ms_sPARENT;
    JAtom ms_ATOM_PARENT;
    
    UnicodeString ms_sCHILD;
    JAtom ms_ATOM_CHILD;
    
    UnicodeString ms_sSIBLING;
    JAtom ms_ATOM_SIBLING;

    /* Related*/
    UnicodeString ms_sSTART;
    JAtom ms_ATOM_START;
    
    UnicodeString ms_sEND;
    JAtom ms_ATOM_END;

    /* Encoding types*/
    UnicodeString ms_s8bit;
    JAtom ms_ATOM_8bit;
    
    UnicodeString ms_sBase64;
    JAtom ms_ATOM_Base64;

    UnicodeString ms_s7bit;
    JAtom ms_ATOM_7bit;

    UnicodeString ms_sQ;
    JAtom ms_ATOM_Q;

    UnicodeString ms_sB;
    JAtom ms_ATOM_B;
  

    /* Value types*/
    UnicodeString ms_sURI;
    JAtom ms_ATOM_URI;

    UnicodeString ms_sTEXT;
    JAtom ms_ATOM_TEXT;

    UnicodeString ms_sBINARY;
    JAtom ms_ATOM_BINARY;

    UnicodeString ms_sDATE;
    JAtom ms_ATOM_DATE;

    UnicodeString ms_sRECUR;
    JAtom ms_ATOM_RECUR;

    UnicodeString ms_sTIME;
    JAtom ms_ATOM_TIME;

    UnicodeString ms_sDATETIME;
    JAtom ms_ATOM_DATETIME;

    UnicodeString ms_sPERIOD;
    JAtom ms_ATOM_PERIOD;

    /*UnicodeString ms_sDURATION;
    JAtom ms_ATOM_DURATION;*/

    UnicodeString ms_sBOOLEAN;
    JAtom ms_ATOM_BOOLEAN;

    UnicodeString ms_sINTEGER;
    JAtom ms_ATOM_INTEGER;

    UnicodeString ms_sFLOAT;
    JAtom ms_ATOM_FLOAT;

    UnicodeString ms_sCALADDRESS;
    JAtom ms_ATOM_CALADDRESS;

    UnicodeString ms_sUTCOFFSET;
    JAtom ms_ATOM_UTCOFFSET;

    /*End PARAMETER NAME, VALUE KEYWORDS*/

    /*other useful strings*/
    UnicodeString ms_sMAILTO_COLON;

    UnicodeString ms_sBEGIN;
    JAtom ms_ATOM_BEGIN;
    
    UnicodeString ms_sBEGIN_WITH_COLON;
    JAtom ms_ATOM_BEGIN_WITH_COLON;

    /*UnicodeString ms_sEND;
    JAtom ms_ATOM_END;*/

    UnicodeString ms_sEND_WITH_COLON;
    JAtom ms_ATOM_END_WITH_COLON;

    UnicodeString ms_sBEGIN_VCALENDAR;
    JAtom ms_ATOM_BEGIN_VCALENDAR;

    UnicodeString ms_sEND_VCALENDAR;
    JAtom ms_ATOM_END_VCALENDAR;

    UnicodeString ms_sBEGIN_VFREEBUSY;
    JAtom ms_ATOM_BEGIN_VFREEBUSY;

    UnicodeString ms_sEND_VFREEBUSY;
    JAtom ms_ATOM_END_VFREEBUSY;

    UnicodeString ms_sLINEBREAK;
    JAtom ms_ATOM_LINEBREAK;

    UnicodeString ms_sOK;
    JAtom ms_ATOM_OK;

    UnicodeString ms_sCOMMA_SYMBOL;
    JAtom ms_ATOM_COMMA_SYMBOL;

    UnicodeString ms_sCOLON_SYMBOL;
    JAtom ms_ATOM_COLON_SYMBOL;

    UnicodeString ms_sDOUBLEQUOTE_SYMBOL;
    UnicodeString ms_sSEMICOLON_SYMBOL;

    UnicodeString ms_sRRULE_WITH_SEMICOLON;

    UnicodeString ms_sDEFAULT_DELIMS;

    UnicodeString ms_sLINEBREAKSPACE;
    UnicodeString ms_sALTREPQUOTE;
    UnicodeString ms_sLINEFEEDSPACE;

    UnicodeString ms_sXPARAMVAL;
    /*  end other useful strings*/

   /*  recurrence keywords */
    UnicodeString ms_sUNTIL; 
    JAtom ms_ATOM_UNTIL;
    
    UnicodeString ms_sCOUNT;
    JAtom ms_ATOM_COUNT;

    UnicodeString ms_sINTERVAL;
    JAtom ms_ATOM_INTERVAL;

    UnicodeString ms_sFREQ;
    JAtom ms_ATOM_FREQ;

    UnicodeString ms_sBYSECOND;
    JAtom ms_ATOM_BYSECOND;

    UnicodeString ms_sBYMINUTE;
    JAtom ms_ATOM_BYMINUTE;

    UnicodeString ms_sBYHOUR;
    JAtom ms_ATOM_BYHOUR;

    UnicodeString ms_sBYDAY;
    JAtom ms_ATOM_BYDAY;

    UnicodeString ms_sBYMONTHDAY;
    JAtom ms_ATOM_BYMONTHDAY;

    UnicodeString ms_sBYYEARDAY;
    JAtom ms_ATOM_BYYEARDAY;

    UnicodeString ms_sBYWEEKNO;
    JAtom ms_ATOM_BYWEEKNO;

    UnicodeString ms_sBYMONTH;
    JAtom ms_ATOM_BYMONTH;

    UnicodeString ms_sBYSETPOS;
    JAtom ms_ATOM_BYSETPOS;

    UnicodeString ms_sWKST;
    JAtom ms_ATOM_WKST;

    /* frequency values*/

    UnicodeString ms_sSECONDLY;
    JAtom ms_ATOM_SECONDLY;
    UnicodeString ms_sMINUTELY;
    JAtom ms_ATOM_MINUTELY;
    UnicodeString ms_sHOURLY;
    JAtom ms_ATOM_HOURLY;
    UnicodeString ms_sDAILY;
    JAtom ms_ATOM_DAILY;
    UnicodeString ms_sWEEKLY;
    JAtom ms_ATOM_WEEKLY;
    UnicodeString ms_sMONTHLY;
    JAtom ms_ATOM_MONTHLY;
    UnicodeString ms_sYEARLY;
    JAtom ms_ATOM_YEARLY;

    /* day values */

    UnicodeString ms_sSU;
    JAtom ms_ATOM_SU;
    UnicodeString ms_sMO;
    JAtom ms_ATOM_MO;
    UnicodeString ms_sTU;
    JAtom ms_ATOM_TU;
    UnicodeString ms_sWE;
    JAtom ms_ATOM_WE;
    UnicodeString ms_sTH;
    JAtom ms_ATOM_TH;
    UnicodeString ms_sFR;
    JAtom ms_ATOM_FR;
    UnicodeString ms_sSA;
    JAtom ms_ATOM_SA;

    /* helperUnicodeStrings and atoms*/

    UnicodeString ms_sBYDAYYEARLY;
    JAtom ms_ATOM_BYDAYYEARLY;
    UnicodeString ms_sBYDAYMONTHLY;
    JAtom ms_ATOM_BYDAYMONTHLY;
    UnicodeString ms_sBYDAYWEEKLY;
    JAtom ms_ATOM_BYDAYWEEKLY;
    UnicodeString ms_sDEFAULT;
    JAtom ms_ATOM_DEFAULT;

    /* content-type */
    UnicodeString ms_sCONTENT_TRANSFER_ENCODING;
    
};
/**
 *  singleton class that contains JAtom ranges for parameter checking.
 */
class NS_CAL_UTIL JulianAtomRange
{
private:
    static JulianAtomRange * m_Instance;
    JulianAtomRange();

public:

    ~JulianAtomRange();
    static JulianAtomRange * Instance();

    /*
    calscale:                           x-token
    method:                             x-token
    prodid:                             x-token
    version:                            x-token
    attach:      value,encoding,fmttype x-token
    categories:          language       x-token
    class:                              x-token
    comment:     altrep, language       x-token
    description: altrep, language       x-token
    geo:                                x-token
    location:    altrep, language       x-token
    percent-complete:                   x-token
    priority:                           x-token
    resources:   altrep, language       x-token
    status:                             x-token
    summary:     altrep, language       x-token
    completed:                          x-token
    dtend:       value, tzid            x-token
    due:         value, tzid            x-token
    dtstart:     value, tzid            x-token
    duration:                           x-token
    freebusy:    fbtype                 x-token
    transp:                             x-token
    tzid:                               x-token
    tzname:              language       x-token
    tzoffsetfrom:                       x-token
    tzoffsetto:                         x-token
    tzurl:                              x-token
    attendee:    cutype, member,role,partstat,rsvp,delto,delfrom,sentby,cn,dir,language,x-token
    contact:     altrep, language       x-token
    organizer:   cutype,dir,sentby,language, x-token
    recid:       value, tzid, range     x-token
    relatedto:   reltype                x-token
    url:                                x-token
    uid:                                x-token
    exdate:      value, tzid            x-token
    exrule:                             x-token
    rdate:       value, tzid            x-token
    rrule:                              x-token
    action:                             x-token
    repeat:                             x-token
    rdate:       value,                 x-token
    created:                            x-token
    dtstamp:                            x-token
    lastmod:                            x-token
    sequence:                           x-token
    request-status:       language      x-token
    */

    /* ATOM RANGES for PARAMETERS */
    JAtom ms_asAltrepLanguageParamRange[2];
    t_int32 ms_asAltrepLanguageParamRangeSize;

    JAtom ms_asTZIDValueParamRange[2];
    t_int32 ms_asTZIDValueParamRangeSize;
    
    JAtom ms_asLanguageParamRange[1];
    t_int32 ms_asLanguageParamRangeSize;
    
    JAtom ms_asEncodingValueParamRange[2];
    t_int32 ms_asEncodingValueParamRangeSize;

    JAtom ms_asEncodingValueFMTTypeParamRange[3];
    t_int32 ms_asEncodingValueFMTTypeParamRangeSize;

    JAtom ms_asSentByParamRange[1];
    t_int32 ms_asSentByParamRangeSize;

    JAtom ms_asReltypeParamRange[1];
    t_int32 ms_asReltypeParamRangeSize;

    JAtom ms_asRelatedValueParamRange[2];
    t_int32 ms_asRelatedValueParamRangeSize;

    JAtom ms_asBinaryURIValueRange[2];
    t_int32 ms_asBinaryURIValueRangeSize;

    JAtom ms_asDateDateTimeValueRange[2];
    t_int32 ms_asDateDateTimeValueRangeSize;

    JAtom ms_asDurationDateTimeValueRange[2];
    t_int32 ms_asDurationDateTimeValueRangeSize;

    JAtom ms_asDateDateTimePeriodValueRange[3];
    t_int32 ms_asDateDateTimePeriodValueRangeSize;

    JAtom ms_asRelTypeRange[3];
    t_int32 ms_iRelTypeRangeSize;

    JAtom ms_asRelatedRange[2];
    t_int32 ms_iRelatedRangeSize;

    JAtom ms_asParameterRange[5]; 
    t_int32 ms_iParameterRangeSize;
    JAtom ms_asIrregularProperties[4];
    t_int32 ms_iIrregularPropertiesSize;
    JAtom ms_asValueRange[14];
    t_int32 ms_iValueRangeSize;
    JAtom ms_asEncodingRange[2];
    t_int32 ms_iEncodingRangeSize;
};
/*---------------------------------------------------------------------*/

/**
 * Singleton class to contain JLog error messages.
 * For now, messages are not localized.
 * NOTE: TODO: Localize the log error message one day
 */
class NS_CAL_UTIL JulianLogErrorMessage
{

private:
    static JulianLogErrorMessage * m_Instance;
    JulianLogErrorMessage();

public:
    ~JulianLogErrorMessage();
    static JulianLogErrorMessage * Instance();
   
#if 0
    UnicodeString ms_sDTEndBeforeDTStart;
    UnicodeString ms_sExportError;
    UnicodeString ms_sNTimeParamError;
    UnicodeString ms_sInvalidPromptValue;
    UnicodeString ms_sSTimeParamError;
    UnicodeString ms_sISO8601ParamError;
    UnicodeString ms_sInvalidTimeStringError;
    UnicodeString ms_sTimeZoneParamError;
    UnicodeString ms_sLocaleParamError;
    UnicodeString ms_sLocaleNotFoundError;
    UnicodeString ms_sPatternParamError;
    UnicodeString ms_sInvalidPatternError;
    UnicodeString ms_sRRuleParamError;
    UnicodeString ms_sERuleParamError;
    UnicodeString ms_sBoundParamError;
    UnicodeString ms_sInvalidRecurrenceError;
    UnicodeString ms_sUnzipNullError;
    UnicodeString ms_sCommandNotFoundError;
    UnicodeString ms_sInvalidTimeZoneError;
    UnicodeString ms_sFileNotFound;
    UnicodeString ms_sInvalidPropertyName;
    UnicodeString ms_sInvalidPropertyValue;
    UnicodeString ms_sInvalidParameterName;
    UnicodeString ms_sInvalidParameterValue;
    UnicodeString ms_sMissingStartingTime;
    UnicodeString ms_sMissingEndingTime;
    UnicodeString ms_sEndBeforeStartTime;
    UnicodeString ms_sMissingSeqNo;
    UnicodeString ms_sMissingReplySeq;
    UnicodeString ms_sMissingURL;
    UnicodeString ms_sMissingDTStamp;
    UnicodeString ms_sMissingUID;
    UnicodeString ms_sMissingDescription;
    UnicodeString ms_sMissingMethodProvided;
    UnicodeString ms_sMissingOrganizer;
    UnicodeString ms_sMissingSummary;
    UnicodeString ms_sMissingAttendees;
    UnicodeString ms_sInvalidVersionNumber;
    UnicodeString ms_sUnknownUID;
    UnicodeString ms_sMissingUIDInReply;
    UnicodeString ms_sMissingValidDTStamp;
    UnicodeString ms_sDeclineCounterCalledByAttendee;
    UnicodeString ms_sPublishCalledByAttendee;
    UnicodeString ms_sRequestCalledByAttendee;
    UnicodeString ms_sCancelCalledByAttendee;
    UnicodeString ms_sCounterCalledByOrganizer;
    UnicodeString ms_sRefreshCalledByOrganizer;
    UnicodeString ms_sReplyCalledByOrganizer;
    UnicodeString ms_sAddReplySequenceOutOfRange;
    UnicodeString ms_sDuplicatedProperty;
    UnicodeString ms_sDuplicatedParameter;
    UnicodeString ms_sConflictMethodAndStatus;
    UnicodeString ms_sConflictCancelAndConfirmedTentative;
    UnicodeString ms_sMissingUIDToMatchEvent;
    UnicodeString ms_sInvalidNumberFormat;
    UnicodeString ms_sDelegateRequestError;
    UnicodeString ms_sInvalidRecurrenceIDRange;
    UnicodeString ms_sUnknownRecurrenceID;
    UnicodeString ms_sPropertyValueTypeMismatch;
    UnicodeString ms_sInvalidRRule;
    UnicodeString ms_sInvalidExRule;
    UnicodeString ms_sInvalidRDate;
    UnicodeString ms_sInvalidExDate;
    UnicodeString ms_sInvalidEvent;
    UnicodeString ms_sInvalidComponent;
    UnicodeString ms_sInvalidAlarm;
    UnicodeString ms_sInvalidTZPart;
    UnicodeString ms_sInvalidAlarmCategory;
    UnicodeString ms_sInvalidAttendee;
    UnicodeString ms_sInvalidFreebusy;
    UnicodeString ms_sDurationAssertionFailed;
    UnicodeString ms_sDurationParseFailed;
    UnicodeString ms_sPeriodParseFailed;
    UnicodeString ms_sPeriodStartInvalid;
    UnicodeString ms_sPeriodEndInvalid;
    UnicodeString ms_sPeriodEndBeforeStart;
    UnicodeString ms_sPeriodDurationZero;
    UnicodeString ms_sFreebusyPeriodInvalid;
    UnicodeString ms_sOptParamInvalidPropertyValue;
    UnicodeString ms_sOptParamInvalidPropertyName;
    UnicodeString ms_sInvalidOptionalParam;
    UnicodeString ms_sAbruptEndOfParsing;
    UnicodeString ms_sLastModifiedBeforeCreated;
    UnicodeString ms_sMultipleOwners;
    UnicodeString ms_sMultipleOrganizers;
    UnicodeString ms_sMissingOwner;
    UnicodeString ms_sMissingDueTime;
    UnicodeString ms_sCompletedPercentMismatch;
    UnicodeString ms_sMissingFreqTagRecurrence;
    UnicodeString ms_sFreqIntervalMismatchRecurrence;
    UnicodeString ms_sInvalidPercentCompleteValue;
    UnicodeString ms_sInvalidPriorityValue;
    UnicodeString ms_sInvalidByHourValue;
    UnicodeString ms_sInvalidByMinuteValue;
    UnicodeString ms_sByDayFreqIntervalMismatch;
    UnicodeString ms_sInvalidByMonthDayValue;
    UnicodeString ms_sInvalidByYearDayValue;
    UnicodeString ms_sInvalidBySetPosValue;
    UnicodeString ms_sInvalidByWeekNoValue;
    UnicodeString ms_sInvalidWeekStartValue;
    UnicodeString ms_sInvalidByMonthValue;
    UnicodeString ms_sInvalidByDayValue;
    UnicodeString ms_sInvalidFrequency;
    UnicodeString ms_sInvalidDayArg;
    UnicodeString ms_sVerifyZeroError;
    UnicodeString ms_sRoundedPercentCompleteTo100;
    UnicodeString ms_sRS200;
    UnicodeString ms_sRS201;
    UnicodeString ms_sRS202;
    UnicodeString ms_sRS203;
    UnicodeString ms_sRS204;
    UnicodeString ms_sRS205;
    UnicodeString ms_sRS206;
    UnicodeString ms_sRS207;
    UnicodeString ms_sRS208;
    UnicodeString ms_sRS209;
    UnicodeString ms_sRS210;
    UnicodeString ms_sRS300;
    UnicodeString ms_sRS301;
    UnicodeString ms_sRS302;
    UnicodeString ms_sRS303;
    UnicodeString ms_sRS304;
    UnicodeString ms_sRS305;
    UnicodeString ms_sRS306;
    UnicodeString ms_sRS307;
    UnicodeString ms_sRS308;
    UnicodeString ms_sRS309;
    UnicodeString ms_sRS310;
    UnicodeString ms_sRS311;
    UnicodeString ms_sRS312;
    UnicodeString ms_sRS400;
    UnicodeString ms_sRS500;
    UnicodeString ms_sRS501;
    UnicodeString ms_sRS502;
    UnicodeString ms_sRS503;
    UnicodeString ms_sMissingUIDGenerateDefault;
    UnicodeString ms_sMissingStartingTimeGenerateDefault;
    UnicodeString ms_sMissingEndingTimeGenerateDefault;
    UnicodeString ms_sNegativeSequenceNumberGenerateDefault;
    UnicodeString ms_sDefaultTBEDescription;
    UnicodeString ms_sDefaultTBEClass;
    UnicodeString ms_sDefaultTBEStatus;
    UnicodeString ms_sDefaultTBETransp;
    UnicodeString ms_sDefaultTBERequestStatus;
    UnicodeString ms_sDefaultTBESummary;
    UnicodeString ms_sDefaultRecIDRange;
    UnicodeString ms_sDefaultAttendeeRole;
    UnicodeString ms_sDefaultAttendeeType;
    UnicodeString ms_sDefaultAttendeeExpect;
    UnicodeString ms_sDefaultAttendeeStatus;
    UnicodeString ms_sDefaultAttendeeRSVP;
    UnicodeString ms_sDefaultAlarmRepeat;
    UnicodeString ms_sDefaultAlarmDuration;
    UnicodeString ms_sDefaultAlarmCategories;
    UnicodeString ms_sDefaultFreebusyStatus;
    UnicodeString ms_sDefaultFreebusyType;
    UnicodeString ms_sDefaultDuration;                        
    UnicodeString ms_sDefaultAlarmDescriptionString;
    UnicodeString ms_sDefaultAlarmSummaryString;
    UnicodeString ms_sXTokenParamIgnored;
    UnicodeString ms_sXTokenComponentIgnored;
#endif
    UnicodeString ms_sDefaultAlarmDescriptionString;
    UnicodeString ms_sDefaultAlarmSummaryString;
    UnicodeString ms_sRS202;

    static t_int32 ms_iStaticErrorNumber;

    t_int32 ms_iDTEndBeforeDTStart;
    t_int32 ms_iExportError;
    t_int32 ms_iNTimeParamError;
    t_int32 ms_iInvalidPromptValue;
    t_int32 ms_iSTimeParamError;
    t_int32 ms_iISO8601ParamError;
    t_int32 ms_iInvalidTimeStringError;
    t_int32 ms_iTimeZoneParamError;
    t_int32 ms_iLocaleParamError;
    t_int32 ms_iLocaleNotFoundError;
    t_int32 ms_iPatternParamError;
    t_int32 ms_iInvalidPatternError;
    t_int32 ms_iRRuleParamError;
    t_int32 ms_iERuleParamError;
    t_int32 ms_iBoundParamError;
    t_int32 ms_iInvalidRecurrenceError;
    t_int32 ms_iUnzipNullError;
    t_int32 ms_iCommandNotFoundError;
    t_int32 ms_iInvalidTimeZoneError;
    t_int32 ms_iFileNotFound;
    t_int32 ms_iInvalidPropertyName;
    t_int32 ms_iInvalidPropertyValue;
    t_int32 ms_iInvalidParameterName;
    t_int32 ms_iInvalidParameterValue;
    t_int32 ms_iMissingStartingTime;
    t_int32 ms_iMissingEndingTime;
    t_int32 ms_iEndBeforeStartTime;
    t_int32 ms_iMissingSeqNo;
    t_int32 ms_iMissingReplySeq;
    t_int32 ms_iMissingURL;
    t_int32 ms_iMissingDTStamp;
    t_int32 ms_iMissingUID;
    t_int32 ms_iMissingDescription;
    t_int32 ms_iMissingMethodProvided;
    t_int32 ms_iMissingOrganizer;
    t_int32 ms_iMissingSummary;
    t_int32 ms_iMissingAttendees;
    t_int32 ms_iInvalidVersionNumber;
    t_int32 ms_iUnknownUID;
    t_int32 ms_iMissingUIDInReply;
    t_int32 ms_iMissingValidDTStamp;
    t_int32 ms_iDeclineCounterCalledByAttendee;
    t_int32 ms_iPublishCalledByAttendee;
    t_int32 ms_iRequestCalledByAttendee;
    t_int32 ms_iCancelCalledByAttendee;
    t_int32 ms_iCounterCalledByOrganizer;
    t_int32 ms_iRefreshCalledByOrganizer;
    t_int32 ms_iReplyCalledByOrganizer;
    t_int32 ms_iAddReplySequenceOutOfRange;
    t_int32 ms_iDuplicatedProperty;
    t_int32 ms_iDuplicatedParameter;
    t_int32 ms_iConflictMethodAndStatus;
    t_int32 ms_iConflictCancelAndConfirmedTentative;
    t_int32 ms_iMissingUIDToMatchEvent;
    t_int32 ms_iInvalidNumberFormat;
    t_int32 ms_iDelegateRequestError;
    t_int32 ms_iInvalidRecurrenceIDRange;
    t_int32 ms_iUnknownRecurrenceID;
    t_int32 ms_iPropertyValueTypeMismatch;
    t_int32 ms_iInvalidRRule;
    t_int32 ms_iInvalidExRule;
    t_int32 ms_iInvalidRDate;
    t_int32 ms_iInvalidExDate;
    t_int32 ms_iInvalidEvent;
    t_int32 ms_iInvalidComponent;
    t_int32 ms_iInvalidAlarm;
    t_int32 ms_iInvalidTZPart;
    t_int32 ms_iInvalidAlarmCategory;
    t_int32 ms_iInvalidAttendee;
    t_int32 ms_iInvalidFreebusy;
    t_int32 ms_iDurationAssertionFailed;
    t_int32 ms_iDurationParseFailed;
    t_int32 ms_iPeriodParseFailed;
    t_int32 ms_iPeriodStartInvalid;
    t_int32 ms_iPeriodEndInvalid;
    t_int32 ms_iPeriodEndBeforeStart;
    t_int32 ms_iPeriodDurationZero;
    t_int32 ms_iFreebusyPeriodInvalid;
    t_int32 ms_iOptParamInvalidPropertyValue;
    t_int32 ms_iOptParamInvalidPropertyName;
    t_int32 ms_iInvalidOptionalParam;
    t_int32 ms_iAbruptEndOfParsing;
    t_int32 ms_iLastModifiedBeforeCreated;
    t_int32 ms_iMultipleOwners;
    t_int32 ms_iMultipleOrganizers;
    t_int32 ms_iMissingOwner;
    t_int32 ms_iMissingDueTime;
    t_int32 ms_iCompletedPercentMismatch;
    t_int32 ms_iMissingFreqTagRecurrence;
    t_int32 ms_iFreqIntervalMismatchRecurrence;
    t_int32 ms_iInvalidPercentCompleteValue;
    t_int32 ms_iInvalidPriorityValue;
    t_int32 ms_iInvalidByHourValue;
    t_int32 ms_iInvalidByMinuteValue;
    t_int32 ms_iByDayFreqIntervalMismatch;
    t_int32 ms_iInvalidByMonthDayValue;
    t_int32 ms_iInvalidByYearDayValue;
    t_int32 ms_iInvalidBySetPosValue;
    t_int32 ms_iInvalidByWeekNoValue;
    t_int32 ms_iInvalidWeekStartValue;
    t_int32 ms_iInvalidByMonthValue;
    t_int32 ms_iInvalidByDayValue;
    t_int32 ms_iInvalidFrequency;
    t_int32 ms_iInvalidDayArg;
    t_int32 ms_iVerifyZeroError;
    t_int32 ms_iRoundedPercentCompleteTo100;
    t_int32 ms_iRS200;
    t_int32 ms_iRS201;
    t_int32 ms_iRS202;
    t_int32 ms_iRS203;
    t_int32 ms_iRS204;
    t_int32 ms_iRS205;
    t_int32 ms_iRS206;
    t_int32 ms_iRS207;
    t_int32 ms_iRS208;
    t_int32 ms_iRS209;
    t_int32 ms_iRS210;
    t_int32 ms_iRS300;
    t_int32 ms_iRS301;
    t_int32 ms_iRS302;
    t_int32 ms_iRS303;
    t_int32 ms_iRS304;
    t_int32 ms_iRS305;
    t_int32 ms_iRS306;
    t_int32 ms_iRS307;
    t_int32 ms_iRS308;
    t_int32 ms_iRS309;
    t_int32 ms_iRS310;
    t_int32 ms_iRS311;
    t_int32 ms_iRS312;
    t_int32 ms_iRS400;
    t_int32 ms_iRS500;
    t_int32 ms_iRS501;
    t_int32 ms_iRS502;
    t_int32 ms_iRS503;
    t_int32 ms_iMissingUIDGenerateDefault;
    t_int32 ms_iMissingStartingTimeGenerateDefault;
    t_int32 ms_iMissingEndingTimeGenerateDefault;
    t_int32 ms_iNegativeSequenceNumberGenerateDefault;
    t_int32 ms_iDefaultTBEDescription;
    t_int32 ms_iDefaultTBEClass;
    t_int32 ms_iDefaultTBEStatus;
    t_int32 ms_iDefaultTBETransp;
    t_int32 ms_iDefaultTBERequestStatus;
    t_int32 ms_iDefaultTBESummary;
    t_int32 ms_iDefaultRecIDRange;
    t_int32 ms_iDefaultAttendeeRole;
    t_int32 ms_iDefaultAttendeeType;
    t_int32 ms_iDefaultAttendeeExpect;
    t_int32 ms_iDefaultAttendeeStatus;
    t_int32 ms_iDefaultAttendeeRSVP;
    t_int32 ms_iDefaultAlarmRepeat;
    t_int32 ms_iDefaultAlarmDuration;
    t_int32 ms_iDefaultAlarmCategories;
    t_int32 ms_iDefaultFreebusyStatus;
    t_int32 ms_iDefaultFreebusyType;
    t_int32 ms_iDefaultDuration;                        
    t_int32 ms_iDefaultAlarmDescriptionString;
    t_int32 ms_iDefaultAlarmSummaryString;
    t_int32 ms_iXTokenParamIgnored;
    t_int32 ms_iXTokenComponentIgnored;
};

/*---------------------------------------------------------------------*/

/**
 * Singleton class that contains all formatting strings used
 * to print iCalendar object to output.
 */
class NS_CAL_UTIL JulianFormatString
{

private:
    static JulianFormatString * m_Instance;
    JulianFormatString();

public:
    
    static JulianFormatString * Instance();
    ~JulianFormatString();

    /* DateTime string*/
    UnicodeString ms_asDateTimePatterns[16];
    UnicodeString ms_sDateTimeISO8601Pattern;
    UnicodeString ms_sDateTimeISO8601LocalPattern;
    UnicodeString ms_sDateTimeISO8601TimeOnlyPattern;
    UnicodeString ms_sDateTimeISO8601DateOnlyPattern;
    UnicodeString ms_sDateTimeDefaultPattern;

    /* Attendee strings*/
    UnicodeString ms_sAttendeeAllMessage;
    UnicodeString ms_sAttendeeDoneActionMessage;
    UnicodeString ms_sAttendeeDoneDelegateToOnly;
    UnicodeString ms_sAttendeeDoneDelegateFromOnly;
    UnicodeString ms_sAttendeeNeedsActionMessage;
    UnicodeString ms_sAttendeeNeedsActionDelegateToOnly; 
    UnicodeString ms_sAttendeeNeedsActionDelegateFromOnly; 

    UnicodeString ms_AttendeeStrDefaultFmt;

    /* Organizer strings */
    UnicodeString ms_OrganizerStrDefaultFmt;

    /* Freebusy strings */
    UnicodeString ms_FreebusyStrDefaultFmt;

    /* TZPart strings */
    UnicodeString ms_TZPartStrDefaultFmt;
    UnicodeString ms_sTZPartAllMessage;

    /* VAlarm strings */
    UnicodeString ms_VAlarmStrDefaultFmt;
    UnicodeString ms_sVAlarmAllMessage;

    /* VEvent strings */
    UnicodeString ms_VEventStrDefaultFmt;
    UnicodeString ms_sVEventAllPropertiesMessage;    
    UnicodeString ms_sVEventCancelMessage;
    UnicodeString ms_sVEventRequestMessage; 
    UnicodeString ms_sVEventRecurRequestMessage;
    UnicodeString ms_sVEventCounterMessage;
    UnicodeString ms_sVEventDeclineCounterMessage;
    UnicodeString ms_sVEventAddMessage;
    UnicodeString ms_sVEventRefreshMessage;
    UnicodeString ms_sVEventReplyMessage;
    UnicodeString ms_sVEventPublishMessage;
    UnicodeString ms_sVEventRecurPublishMessage;
    UnicodeString ms_sVEventDelegateRequestMessage;
    UnicodeString ms_sVEventRecurDelegateRequestMessage;

    /* VFreebusy strings */
    UnicodeString ms_sVFreebusyReplyMessage;
    UnicodeString ms_sVFreebusyPublishMessage;
    UnicodeString ms_sVFreebusyRequestMessage;
    UnicodeString ms_sVFreebusyAllMessage;
    UnicodeString ms_VFreebusyStrDefaultFmt;

    /* VJournal strings */
    UnicodeString ms_sVJournalAllPropertiesMessage;
    UnicodeString ms_sVJournalCancelMessage;
    UnicodeString ms_sVJournalRequestMessage;
    UnicodeString ms_sVJournalRecurRequestMessage;
    UnicodeString ms_sVJournalCounterMessage;
    UnicodeString ms_sVJournalDeclineCounterMessage;
    UnicodeString ms_sVJournalAddMessage;
    UnicodeString ms_sVJournalRefreshMessage;
    UnicodeString ms_sVJournalReplyMessage;
    UnicodeString ms_sVJournalPublishMessage;
    UnicodeString ms_sVJournalRecurPublishMessage;
    UnicodeString ms_VJournalStrDefaultFmt;

    /* VTodo strings */
    UnicodeString ms_sVTodoAllPropertiesMessage;
    UnicodeString ms_sVTodoCancelMessage;
    UnicodeString ms_sVTodoRequestMessage;
    UnicodeString ms_sVTodoRecurRequestMessage;
    UnicodeString ms_sVTodoCounterMessage;
    UnicodeString ms_sVTodoDeclineCounterMessage;
    UnicodeString ms_sVTodoAddMessage;
    UnicodeString ms_sVTodoRefreshMessage;
    UnicodeString ms_sVTodoReplyMessage;
    UnicodeString ms_sVTodoPublishMessage;
    UnicodeString ms_sVTodoRecurPublishMessage;
    UnicodeString ms_VTodoStrDefaultFmt;

    /* VTimeZone strings */
    UnicodeString ms_VTimeZoneStrDefaultFmt;
    UnicodeString ms_sVTimeZoneAllMessage;
};


/*----------------------------------------------------------
** Key letters used for formatting strings 
**----------------------------------------------------------*/  
const t_int32 ms_cAction 		  = 'l';
const t_int32 ms_cAlarms          = 'w';
const t_int32 ms_cAttach          = 'a';
const t_int32 ms_cAttendees       = 'v';
const t_int32 ms_cCategories      = 'k';
const t_int32 ms_cClass           = 'c';
const t_int32 ms_cComment         = 'K';
const t_int32 ms_cCompleted       = 'G';
const t_int32 ms_cContact         = 'H';
const t_int32 ms_cCreated         = 't';
const t_int32 ms_cDescription     = 'i';
const t_int32 ms_cDuration        = 'D';
const t_int32 ms_cDTEnd           = 'e';
const t_int32 ms_cDTStart         = 'B';
const t_int32 ms_cDTStamp         = 'C';
const t_int32 ms_cDue             = 'F';
const t_int32 ms_cExDate          = 'X';
const t_int32 ms_cExRule          = 'E';
const t_int32 ms_cGEO             = 'O';
const t_int32 ms_cLastModified    = 'M';
const t_int32 ms_cLocation        = 'L';
const t_int32 ms_cOrganizer       = 'J';
const t_int32 ms_cPercentComplete = 'P';
const t_int32 ms_cPriority        = 'p';
const t_int32 ms_cRDate           = 'x';
const t_int32 ms_cRRule           = 'y';
const t_int32 ms_cRecurrenceID    = 'R';
const t_int32 ms_cRelatedTo       = 'o';
const t_int32 ms_cRepeat          = 'A';
const t_int32 ms_cRequestStatus   = 'T';
const t_int32 ms_cResources       = 'r';
/*const t_int32 ms_cResponseSequence  = 'q';*/
const t_int32 ms_cSequence        = 's';
const t_int32 ms_cStatus          = 'g';
const t_int32 ms_cSummary         = 'S';
const t_int32 ms_cTransp          = 'h';
const t_int32 ms_cTrigger         = 'z';
const t_int32 ms_cUID             = 'U';
const t_int32 ms_cURL             = 'u';
const t_int32 ms_cXTokens         = 'Z';

const t_int32 ms_cTZOffsetTo      = 'd';
const t_int32 ms_cTZOffsetFrom    = 'f';
const t_int32 ms_cTZName          = 'n';
const t_int32 ms_cTZURL           = 'Q';
const t_int32 ms_cTZID            = 'I';
const t_int32 ms_cTZParts         = 'V';
/*const t_int32 ms_cDAYLIGHT      = 'd';*/


const t_int32 ms_cFreebusy        = 'Y';

/* ATTENDEE ONLY (see attendee.h) */
/*
const t_int32 ms_cAttendeeName            = 'N';
const t_int32 ms_cAttendeeRole            = 'R';
const t_int32 ms_cAttendeeStatus        = 'S';  repeat 
const t_int32 ms_cAttendeeRSVP            = 'V';
const t_int32 ms_cAttendeeType            = 'T';
const t_int32 ms_cAttendeeExpect          = 'E';
const t_int32 ms_cAttendeeDelegatedTo     = 'D';
const t_int32 ms_cAttendeeDelegatedFrom   = 'd';
const t_int32 ms_cAttendeeMember          = 'M';
const t_int32 ms_cAttendeeDir             = 'l'; 'el' 
const t_int32 ms_cAttendeeSentBy          = 's';
const t_int32 ms_cAttendeeCN              = 'C';
const t_int32 ms_cAttendeeDisplayName     = 'z';
*/

/* FREEBUSY ONLY (see freebusy.h) */
/*
const t_int32 ms_cFreebusyType            = 'T';
const t_int32 ms_cFreebusyStatus          = 'S';
const t_int32 ms_cFreebusyPeriod          = 'P';
*/

const t_int32 ms_iMAX_LINE_LENGTH = 76;


#endif /* __KEYWORD_H_ */



