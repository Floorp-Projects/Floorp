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

#include "jdefines.h"
#include "julnstr.h"
#include "nscal.h"
#include "keyword.h"

#include "form.h"
#include "formFactory.h"

#include "gregocal.h"
#include "time.h"
#include "smpdtfmt.h"
#include "simpletz.h"
#include "datetime.h"
#include "jutility.h"

#ifndef MOZ_TREX
#include "allxpstr.h"
#endif

JulianFormFactory::JulianFormFactory()
{
	firstCalendar = nil;
	thisICalComp = nil;
    serverICalComp = nil;
	SetDetail(FALSE);
    s = nil;
	jf = nil;
	ServerProxy = nil;
    maxical = 0;
	cb = nil;

    SlotsPerHour = 0;
    displayTimeZone = FALSE;
    scaleType = 0;
    slotsacross = 0;
    hoursToDisplay = 0; 
    state = Free_State;
    new_state = Free_State;
    count = 0;
    TimeHour_HTML = nil;
    default_tz = TimeZone::createDefault();
    p = nil;
    fb = nil;
    fbIndex = 0;
    pIndex = 0;
    pv = nil;
    fbv = nil;
    aref_count = 0;
    aref2_count = 0;
    outStream = nil;

    isEvent = FALSE;
	isFreeBusy = FALSE;
}

JulianFormFactory::JulianFormFactory(NSCalendar& imipCal, JulianServerProxy* jsp)
{
	firstCalendar = nil;
	thisICalComp = nil;
    serverICalComp = nil;
	SetDetail(FALSE);
    s = nil;
	jf = nil;
	ServerProxy = nil;
    maxical = 0;
	cb = nil;

    SlotsPerHour = 0;
    displayTimeZone = FALSE;
    scaleType = 0;
    slotsacross = 0;
    hoursToDisplay = 0; 
    state = Free_State;
    new_state = Free_State;
    count = 0;
    TimeHour_HTML = nil;
    default_tz = TimeZone::createDefault();
    p = nil;
    fb = nil;
    fbIndex = 0;
    pIndex = 0;
    pv = nil;
    fbv = nil;
    aref_count = 0;
    aref2_count = 0;
    outStream = nil;

    isEvent = FALSE;
	isFreeBusy = FALSE;

    firstCalendar = &imipCal;
	ServerProxy = jsp;
}

JulianFormFactory::JulianFormFactory(NSCalendar& imipCal, JulianForm& hostForm, pJulian_Form_Callback_Struct callbacks)
{
	firstCalendar = nil;
	thisICalComp = nil;
    serverICalComp = nil;
	SetDetail(FALSE);
    s = nil;
	jf = nil;
	ServerProxy = nil;
    maxical = 0;
	cb = nil;

    SlotsPerHour = 0;
    displayTimeZone = FALSE;
    scaleType = 0;
    slotsacross = 0;
    hoursToDisplay = 0; 
    state = Free_State;
    new_state = Free_State;
    count = 0;
    TimeHour_HTML = nil;
    default_tz = TimeZone::createDefault();
    p = nil;
    fb = nil;
    fbIndex = 0;
    pIndex = 0;
    pv = nil;
    fbv = nil;
    aref_count = 0;
    aref2_count = 0;
    outStream = nil;

    isEvent = FALSE;
	isFreeBusy = FALSE;

    firstCalendar = &imipCal;
	jf = &hostForm;
	cb = callbacks;
}

JulianFormFactory::~JulianFormFactory()
{
    // if (default_tz) delete default_tz;
    PR_FREEIF(TimeHour_HTML);

#ifdef LIBJULIAN_USE_XPSTRS
    if (cb && cb->GetString)
    {
        PR_FREEIF(error_Header_HTML);        		            
        PR_FREEIF(publish_Header_HTML);         		        
        PR_FREEIF(publishFB_Header_HTML);       		    	
        PR_FREEIF(replyFB_Header_HTML);         			    
        PR_FREEIF(request_Header_HTML);         			    
        PR_FREEIF(request_FB_Header_HTML);      			    
        PR_FREEIF(request_D_FB_Header_HTML);    		    
        PR_FREEIF(eventreply_Header_HTML);      	    	
        PR_FREEIF(eventcancel_Header_HTML);         			
        PR_FREEIF(eventrefreshreg_Header_HTML);     		
        PR_FREEIF(eventcounterprop_Header_HTML);    		
        PR_FREEIF(eventdelinecounter_Header_HTML);  	

        PR_FREEIF(String_What);                      
        PR_FREEIF(String_When);                       
        PR_FREEIF(String_Location);               
        PR_FREEIF(String_Organizer);              
        PR_FREEIF(String_Status);                     
        PR_FREEIF(String_Priority);         
        PR_FREEIF(String_Categories);         
        PR_FREEIF(String_Resources);          
        PR_FREEIF(String_Attachments);    
        PR_FREEIF(String_Alarms);                     
        PR_FREEIF(String_Created);                
        PR_FREEIF(String_Last_Modified);      
        PR_FREEIF(String_Sent);                   
        PR_FREEIF(String_UID);
    }
#endif
}


XP_Bool
JulianFormFactory::getHTML(JulianString* OutString, NET_StreamClass *this_stream, XP_Bool want_detail)
{
    outStream = this_stream;
    return getHTML(OutString, want_detail);
}

/*
** getHTML
**
** Call after setting firstCalendar. It will produce an Unicode
** string with html that should be inserted into a real html file.
** On any errors an empty string is returened.
**
*/
XP_Bool
JulianFormFactory::getHTML(JulianString* htmlstorage, XP_Bool want_detail)
{
JulianString  ny("Not Yet");
JulianString  im("<B>Invalid method</B>");
int32 icalIndex = 0;	// Index into venent or vfreebusy array
int32 icalMaxIndex;
char*		temp = nil;
char*		urlcallback = nil;

	// Set up the storage for the html
	s = htmlstorage;

	// Return an empty string on any error.
	// This case: firstCalendar isn't set. It's needed to
	// find what vevents, freebusy etc to parse.
	if (firstCalendar == nil) return TRUE;

    if(!jf->isFoundNLSDataDirectory())
    {
        if (0 != firstCalendar->getLog())
        {
            // TODO: need to move errorID and comment to XP-Resources
            t_int32 errorID;
            UnicodeString comment;
            errorID = 123456789;
            comment = "ERROR: Cannot find NLS Data Directory";
            firstCalendar->getLog()->logError(errorID, comment);
        }        
    }

	JulianPtrArray * e = firstCalendar->getEvents();
	JulianPtrArray * f = firstCalendar->getVFreebusy();

	// Look for only for one type of ICalComponent.
	// Start with vEvents and work down from there.
	// There is a bool for each ICalComponent class
	// that is handled.
	// If there isn't anything good here, set thisICalComp
	// to nil.
	if (e)
	{
        if ((maxical = icalMaxIndex = e->GetSize() - 1) >= 0)
            isEvent = TRUE;
	} else
	if (f)
	{
        if ((maxical = icalMaxIndex = f->GetSize() - 1) >= 0)
		isFreeBusy = TRUE;
	} else
	{
        icalMaxIndex = 0;
		thisICalComp = nil;
	}

	SetDetail(want_detail);

	/*
	** To get callback to work
	*/
	if (cb && cb->callbackurl)
	{
		urlcallback = (*cb->callbackurl)((NET_CallbackURLFunc) (want_detail ? jf_detail_callback : jf_callback), (void *)jf);
        jf->refcount++;
        temp = PR_smprintf(Start_HTML, urlcallback);
	} else
		temp = Start_HTML;
	doAddHTML(temp);
	if (cb && cb->callbackurl && temp)
	{
		// delete temp;
        PR_DELETE(temp);
	}

    // In the case that the ical was so bad that there isn't any thing left
    if (!isEvent && !isFreeBusy)
    {
        HandleError();
        doAddHTML( End_HTML );
        return TRUE;
    }

    if (icalMaxIndex > 0)
	{
		doHeaderMuiltStart();
	}

    int32 loopcount = icalMaxIndex > 0 && detail ? 2 : 1;
    for (int32 loopx = 0; loopx < loopcount; loopx++)
    {
        int32 ICAL_LoopCount = MIN(icalMaxIndex, jff_clip_events);
        icalIndex = 0;

	    while (icalIndex <= ICAL_LoopCount)
	    {
 	        // thisICalComp needs to be set to something. It's a problem if it's nil.
		    if (isEvent)
		    {
			    thisICalComp = (VEvent *)e->GetAt(icalIndex++);
		    } else 
		    if (isFreeBusy)
		    {
			    thisICalComp = (VFreebusy *)f->GetAt(icalIndex++);
            } else
                icalIndex++;

		    // If in detail mode, check to see if there is a server version of this icalcomponent.
		    // Otherwise set it to nil to show that this is the first time this has been seen.
		    if (detail && ServerProxy)
		    {
			    serverICalComp = ServerProxy->getByUid(nil);
		    } else {
			    serverICalComp = nil;
		    }

		    if (icalMaxIndex > 0 && loopx == 0)
		    {
			    doHeaderMuilt();
		    }

 		    // Find the right html form based on the method
            if ((icalMaxIndex > 0 && loopx == 1) || icalMaxIndex == 0)
		    switch (firstCalendar->getMethod())
		    {
			    case NSCalendar::METHOD_PUBLISH:
					    if (isEvent) 
					    {
						    HandlePublishVEvent();
					    }

					    if (isFreeBusy)
					    {
						    HandlePublishVFreeBusy(TRUE);
					    }
					    break;	

			    case NSCalendar::METHOD_REQUEST:
			    case NSCalendar::METHOD_ADD:

					    if (isEvent) 
					    {
						    HandleRequestVEvent();
					    }

					    if (isFreeBusy)
					    {
						    HandleRequestVFreeBusy();
					    }
					    break;

			    case NSCalendar::METHOD_REPLY: 
					    
					    if (isEvent) 
					    {
						    HandleEventReplyVEvent();
					    }

					    if (isFreeBusy)
					    {
						    HandlePublishVFreeBusy(FALSE);
					    }

					    break;

			    case NSCalendar::METHOD_CANCEL: 
				    
					    if (isEvent) 
					    {
						    HandleEventCancelVEvent();
					    }
					    
					    break;

			    case NSCalendar::METHOD_REFRESH:
				    
					    if (isEvent) 
					    {
						    HandleEventRefreshRequestVEvent();
					    }

					    if (isFreeBusy)
					    {
						    doAddHTML ( ny );
					    }
					    break;

			    case NSCalendar::METHOD_COUNTER: 
						    
					    if (isEvent) 
					    {
						    HandleEventCounterPropVEvent();
					    }
					    break;

			    case NSCalendar::METHOD_DECLINECOUNTER: 

					    if (isEvent) 
					    {
						    HandleDeclineCounterVEvent();
					    }
					    break;

			    case NSCalendar::METHOD_INVALID: 
			    default:
					    doAddHTML ( im );
					    break;	
		    }
	    }

	    if (icalMaxIndex > 1)
	    {
		    doHeaderMuiltEnd();
 		    doAddGroupButton(doCreateButton(Buttons_Accept_Type, Buttons_Accept_Label));
            if (!detail)
            {
		        doAddGroupButton(doCreateButton(Buttons_Details_Type, Buttons_Details_Label));
            }
	    }
    }

	doAddHTML( End_HTML );
    flush_stream();

	return TRUE;
}

int32
JulianFormFactory::flush_stream()
{
    int32 ret = 0;

    if (outStream && (s->GetStrlen() > 0))
    {
        ret = (*(outStream)->put_block)(outStream->data_object, s->GetBuffer(), s->GetStrlen());
        *s = ""; // empty it.
    }

    return ret;
} 

void
JulianFormFactory::doHeaderMuiltStart()
{
	char* m1 = nil;

    if (MuiltEvent.GetStrlen() > 0)
    {
	JulianString	tempstring;

		doHeader(MuiltEvent_Header_HTML);
	
		doAddHTML( nbsp );

		// Get This messages contains 2 events
        m1 = PR_smprintf(MuiltEvent.GetBuffer(), maxical + 1);

		doAddHTML( m1 );
		PR_FREEIF( m1 );

        if (maxical + 1 > jff_clip_events)
        {
            m1 = PR_smprintf(TooManyEvents.GetBuffer(), jff_clip_events);
 		    if (m1)
            {
                doAddHTML( Line_Break_HTML );
                doAddHTML( nbsp );
                doAddHTML( m1 );
		        PR_DELETE( m1 );
            }
       }

		doAddHTML( Line_3_HTML );

		doAddHTML( Props_Head_HTML );

			doAddHTML( FBT_NewRow_HTML );
				doAddHTML( Cell_Start_HTML );
					doAddHTML(*doFont(*doItalic(*doBold(WhenStr , &tempstring ) , &tempstring ) , &tempstring) );
				doAddHTML( Cell_End_HTML );

				doAddHTML( Cell_Start_HTML );
					doFont(*doItalic(*doBold(WhatStr , &tempstring ) , &tempstring ) , &tempstring );
					doAddHTML( nbsp );
					doAddHTML(tempstring);
				doAddHTML( Cell_End_HTML );
			doAddHTML( FBT_EndRow_HTML );

    }
}

void
JulianFormFactory::doHeaderMuiltEnd()
{
	doAddHTML( FBT_End_HTML );
}


/*
** Muilable icalcomponents.
*/
void
JulianFormFactory::doHeaderMuilt()
{
	JulianString  tempstring;
	char*  dtstart_string = "%B";
	char*  summary_string = "%S";
    char   a_temp[256] = "<A HREF=#A%d>";
    char   a_end_temp[256] = "</A>";
    char*  temp;
    UnicodeString u;
        
	doAddHTML( FBT_NewRow_HTML );

        doAddHTML( Cell_Start_HTML );

		doAddHTML( Font_Fixed_Start_HTML );
			doPreProcessing(dtstart_string);
		doAddHTML( Font_Fixed_End_HTML );

		doAddHTML( Cell_End_HTML );

		doAddHTML( Cell_Start_HTML );
		doAddHTML( Start_BIG_Font );
        
        if (detail)
        {
            temp = PR_smprintf(a_temp, aref2_count++);
            doAddHTML(temp);
            PR_FREEIF(temp);
        }
        u = summary_string;
        uTemp = temp = thisICalComp->toStringFmt(u).toCString("");
        PR_FREEIF(temp);
        doAddHTML(uTemp);
        if (detail)
        {
            doAddHTML(a_end_temp);
        }

		doAddHTML( End_Font );

		doAddHTML( Cell_End_HTML );

	doAddHTML( FBT_EndRow_HTML );
}

/*
** Cool "When:" formating.
*/
void
JulianFormFactory::doPreProcessing(char*  icalpropstr)
{
t_int32 ICalPropChar = 	(t_int32)(icalpropstr[(TextOffset)1]);

	if (ICalPropChar == ms_cDTStart)
	{
		DateTime		start, end;
		XP_Bool allday = isEvent ? (XP_Bool)((TimeBasedEvent *)thisICalComp)->isAllDayEvent() : FALSE;
        JulianString temp_js;

		if (isEvent)
		{
			start = ((VEvent *)thisICalComp)->getDTStart();
			end = ((VEvent *)thisICalComp)->getDTEnd();
		} else
		if (isFreeBusy)
		{
			start = ((VFreebusy *)thisICalComp)->getDTStart();
			end = ((VFreebusy *)thisICalComp)->getDTEnd();
		}

        temp_js = JulianString(icalpropstr);
		doPreProcessingDateTime(temp_js, allday, start, end, *thisICalComp);
	} else
	if (ICalPropChar == ms_cAttendees)
	{
		doPreProcessingAttend(*thisICalComp);
	} else
	if (	ICalPropChar == ms_cOrganizer)
	{
		doPreProcessingOrganizer(*thisICalComp);
	}
}

void
JulianFormFactory::doPreProcessingAttend(ICalComponent& ic)
{
XP_Bool			lastOne = FALSE;
char*           temp;	
char*       	uni_comma	= ",";
char*       	uni_AttendName	= "%(^N)v";
char*       	attendStatusStr = "%(^S)v";
UnicodeString   usAttendStatusStr = attendStatusStr;
UnicodeString   usUni_AttendName = uni_AttendName;
char*       	attendStatus = (ic.toStringFmt(usAttendStatusStr)).toCString("");
char*       	attendNames = (ic.toStringFmt(usUni_AttendName)).toCString("");
UnicodeString usAttendStatus = attendStatus;
UnicodeString usAttendNames = attendNames;

UnicodeString	tempString;
JulianString	tempString2;
JulianString	tempString3;
JulianString	tempStringA;
JulianString	tempStringB;
UnicodeString	tempString4;
TextOffset		status_start = 0,
				status_end = 0,
				name_start = 0,
				name_end = 0;

    // Check that there is something in the attendNames string.

    if (XP_STRLEN(attendNames) == 0)
    {
        // Should not get here, but just in case don't loop
        lastOne = TRUE;
    }

	while (!lastOne)
	{
		status_end = JulianString(attendStatus).IndexOf(uni_comma, (int32)status_start);
		name_end   = JulianString(attendNames).IndexOf(uni_comma, (int32)name_start);

		// Hit the last name yet?
		if (status_end == -1)
		{
			lastOne = TRUE;
			status_end = XP_STRLEN(attendStatus);
			name_end = XP_STRLEN(attendNames);
		}

		// Add a comma after each name execpt for the last name.
		if (status_start > 0)
		{
			doAddHTML( uni_comma );
			doAddHTML( nbsp );
		}

		usAttendStatus.extractBetween(status_start, status_end, tempString);
		if (tempString == nsCalKeyword::Instance()->ms_sACCEPTED )
		{
			doAddHTML( Accepted_Gif_HTML );
		} else
		if (tempString == nsCalKeyword::Instance()->ms_sDECLINED )
		{
			doAddHTML( Declined_Gif_HTML );
		} else
		if (tempString == nsCalKeyword::Instance()->ms_sDELEGATED )
		{
			doAddHTML( Delegated_Gif_HTML );
		} else
		if (tempString == nsCalKeyword::Instance()->ms_sVCALNEEDSACTION )
		{
			doAddHTML( NeedsAction_Gif_HTML );
		} else
			doAddHTML( Question_Gif_HTML );

		// Get the email address mailto:xxx@yyy.com
		// Convert to <A HREF="mailto:xxx@yyy.com"><FONT ...>xxx@yyy.com</FONT></A>&nbsp
		usAttendNames.extractBetween(name_start, name_end, tempString);
		tempString.extractBetween(7, tempString.size(), tempString4);
        tempStringA = JulianString(temp = tempString4.toCString(""));
        if (temp) delete temp;
        tempStringB = JulianString(temp = tempString.toCString(""));
        if (temp) delete temp;
		doAddHTML(
			doARef(
				*doFont( tempStringA, &tempString2 ),
				tempStringB,
				&tempString3 )
			);

		status_start = status_end + 1;
		name_start   = name_end + 1;
		
		// Set it to an empty string
		tempString2 = "";
		tempString3 = "";
	}

    if (attendNames) delete attendStatus;
    if (attendNames) delete attendNames;
}

void
JulianFormFactory::doPreProcessingOrganizer(ICalComponent& ic)
{
char*       	uni_OrgName	= "%(^N)J";
UnicodeString usUni_OrgName = uni_OrgName;
char*           temp = ic.toStringFmt(usUni_OrgName).toCString("");
JulianString	attendNames = temp;
JulianString	tempString;
JulianString	tempString2;
JulianString	tempString3;
JulianString	tempString4;
TextOffset		name_start = 0,
				name_end = 0;

    PR_FREEIF(temp);
	name_end = attendNames.GetStrlen();

	// Get the email address mailto:xxx@yyy.com
	// Convert to <A HREF="mailto:xxx@yyy.com"><FONT ...>xxx@yyy.com</FONT></A>&nbsp
	tempString = attendNames(name_start, name_end);
	tempString4 = tempString(7, tempString.GetStrlen());
	doAddHTML(
		doARef(
			*doFont( tempString4, &tempString2 ),
			tempString,
			&tempString3 )
		);
}

JulianString*
JulianFormFactory::doARef(JulianString& refText, JulianString& refTarget, JulianString* outputString)
{
	JulianString  *Saved_S = s;
	s = outputString;

	doAddHTML( Aref_Start_HTML );
	doAddHTML( refTarget );
	doAddHTML( Aref_End_HTML );
	doAddHTML( refText );
	doAddHTML( ArefTag_End_HTML );

	s = Saved_S;

	return outputString;
}


#if 0
JulianString*
JulianFormFactory::doFontFixed(JulianString& fontText, JulianString* outputString)
{
	JulianString  *Saved_S = s;
	JulianString	out1;
	s = &out1;

	doAddHTML( Font_Fixed_Start_HTML );
	doAddHTML( fontText );
	doAddHTML( Font_Fixed_End_HTML );

	s = Saved_S;
	*outputString = out1;

	return outputString;
}
#endif

JulianString*
JulianFormFactory::doFont(JulianString& fontText, JulianString* outputString)
{
	JulianString  *Saved_S = s;
	JulianString	out1;
	s = &out1;

	doAddHTML( Start_Font );
	doAddHTML( fontText );
	doAddHTML( End_Font );

	s = Saved_S;
	*outputString = out1;

	return outputString;
}

JulianString*
JulianFormFactory::doItalic(JulianString& ItalicText, JulianString* outputString)
{
	JulianString  *Saved_S = s;
	JulianString	out1;
	s = &out1;

	doAddHTML( Italic_Start_HTML );
	doAddHTML( ItalicText );
	doAddHTML( Italic_End_HTML );

	s = Saved_S;
	*outputString = out1;

	return outputString;
}

JulianString*
JulianFormFactory::doBold(JulianString& BoldText, JulianString* outputString)
{
	JulianString  *Saved_S = s;
	JulianString	out1;
	s = &out1;

	doAddHTML( Bold_Start_HTML );
	doAddHTML( BoldText );
	doAddHTML( Bold_End_HTML );

	s = Saved_S;
	*outputString = out1;

	return outputString;
}

JulianString*
JulianFormFactory::doBold(char* BoldText, JulianString* outputString)
{
	JulianString  *Saved_S = s;
	JulianString	out1;
	s = &out1;

	doAddHTML( Bold_Start_HTML );
	doAddHTML( BoldText );
	doAddHTML( Bold_End_HTML );

	s = Saved_S;
	*outputString = out1;

	return outputString;
}

void
JulianFormFactory::doPreProcessingDateTime(JulianString& icalpropstr, XP_Bool allday, DateTime &start, DateTime &end, ICalComponent &ic)
{
	JulianString  dtstartI	= "%B";
	UnicodeString dtstart	= "%(EEE MMM dd, yyyy hh:mm a)B";
	UnicodeString dtend		= "%(EEE MMM dd, yyyy hh:mm a z)e";

	UnicodeString dtstartSD = "%(EEE MMM dd, yyyy hh:mm a)B - ";
	UnicodeString dtstartAD = "%(EEE MMM dd, yyyy)B - ";
	UnicodeString dtstartMD = "%(EEE MMM dd, yyyy hh:mm a z)B";
	UnicodeString dtendSD	= "%(hh:mm a z)e";
	UnicodeString dtdurSD	= " ( %D )";
	UnicodeString endstr	= ic.toStringFmt(dtend);
    UnicodeString uTemp;

	// All Day event. May spane more then the one day
	// Display as May 01, 1997 ( Day Event )
	// Note: fix this up for a single day event and for more then one day event
	if (allday)
	{
        uTemp = ic.toStringFmt(dtstartAD);
		doAddHTML(uTemp);

        doAddHTML(	Text_AllDay					);
	} else
	// Moment in time date, ie dtstart == dtend?
	// Display as Begins on May 01, 1997 12:00 pm
	if (start == end)
	{
		doAddHTML(	Text_StartOn				);

        uTemp = ic.toStringFmt(dtstartMD);
        doAddHTML(uTemp);
        doAddHTML(nbsp);
	
    } else
	// Both dates on the same day?
	// Display as May 01, 1997 12:00 pm - 3:00 pm ( 3 Hours )
	if (start.sameDMY(&end, default_tz))
	{
        uTemp = ic.toStringFmt(dtstartSD);
        doAddHTML(uTemp);
		
        uTemp = ic.toStringFmt(dtendSD);
        doAddHTML(uTemp);
 		
        uTemp = ic.toStringFmt(dtdurSD);
        doAddHTML(uTemp);
 	}
	
	// Default Case.
	// Display both dates in full. ie May 01, 1997 12:00 pm - May 02, 1997 2:00 pm
	else
	if (icalpropstr.IndexOf(dtstartI.GetBuffer(), (int32)0) >= 0)
	{

        uTemp = ic.toStringFmt(dtstart);
        doAddHTML(uTemp);
		
        if (endstr.size() > 6)
		{
			doAddHTML( Text_To );
			doAddHTML( endstr );
		}
	}
}

/*
**
** Assumed that serverICalComp is good.
*/
void
JulianFormFactory::doDifferenceProcessingAttendees()
{
XP_Bool			lastOne = FALSE,
				need_comma = FALSE;	
JulianString	uni_comma	= ",";
char*           uni_x	= "%(^N)v";
UnicodeString usUni_x = uni_x;
char*           temp;
char*           temp1;
JulianString	NewAttendNames = temp = thisICalComp->toStringFmt(usUni_x).toCString("");
JulianString	OldAttendNames = temp1 = serverICalComp->toStringFmt(usUni_x).toCString("");
JulianString	tempString;
JulianString	tempString2;
JulianString	tempString3;
JulianString	tempString4;
TextOffset		new_name_start = 0,
				new_name_end = 0,
				old_name_start = 0,
				old_name_end = 0;

// Work and the "Added" section. These are email address that are in thisICalComp and
// not in serverICalComp. The general idea here is to march down the attends in NewAttendNames
// and see if they are in OldAttendNames

	doAddHTML( Props_HTML_Before_Label );
	doAddHTML( Start_Font );
		doAddHTML( Italic_Start_HTML );
			doAddHTML( " Added: " );
		doAddHTML( Italic_End_HTML );
	doAddHTML( End_Font );
	doAddHTML( Props_HTML_After_Label );

	while (!lastOne)
	{
		new_name_end   = NewAttendNames.IndexOf(uni_comma.GetBuffer(), (int32)new_name_start);

		// Hit the last name yet?
		if (new_name_end == -1)
		{
			lastOne = TRUE;
			new_name_end = NewAttendNames.GetStrlen();
		}

		// Add a comma after each name execpt for the last name.
		if (need_comma)
		{
			doAddHTML( uni_comma );
			doAddHTML( nbsp );
			need_comma = FALSE;
		}

		// Get the email address mailto:xxx@yyy.com
		// Convert to <A HREF="mailto:xxx@yyy.com"><FONT ...>xxx@yyy.com</FONT></A>&nbsp
		tempString = NewAttendNames(new_name_start, new_name_end);	// Get "mailto:xxx@yyy.com"
		tempString4 = tempString(7, tempString.GetStrlen());	    // Remove "mailto:"

		// tempString4 now holds "xxx@yyy.com". See if it is in OldAttendNames
		if (OldAttendNames.IndexOf(tempString4.GetBuffer(), (int32)0) == -1)
		{
			// Not there so this must be a new one. Format it for output
			doAddHTML( doARef( *doFont( tempString4, &tempString2 ), tempString, &tempString3 ) );
			need_comma = TRUE;
		}

		new_name_start   = new_name_end + 1;
		
		// Set it to an empty string
		tempString2 = "";
		tempString3 = "";
		tempString4 = "";
	}
	doAddHTML( Props_HTML_After_Data );	

// Work and the "Removed" section. These are email address that are in thisICalComp and
// not in serverICalComp. The general idea here is to march down the attends in NewAttendNames
// and see if they are in OldAttendNames

	doAddHTML( Props_HTML_Before_Label );
	doAddHTML( Start_Font );
		doAddHTML( Italic_Start_HTML );
			doAddHTML( " Removed: " );
		doAddHTML( Italic_End_HTML );
	doAddHTML( End_Font );
	doAddHTML( Props_HTML_After_Label );

	need_comma = lastOne = FALSE;
	new_name_start = 0;
	while (!lastOne)
	{
		new_name_end   = OldAttendNames.IndexOf(uni_comma.GetBuffer(), (int32)new_name_start);

		// Hit the last name yet?
		if (new_name_end == -1)
		{
			lastOne = TRUE;
			new_name_end = OldAttendNames.GetStrlen();
		}

		// Add a comma after each name execpt for the last name.
		if (need_comma)
		{
			doAddHTML( uni_comma );
			doAddHTML( nbsp );
			need_comma = FALSE;
		}

		// Get the email address mailto:xxx@yyy.com
		// Convert to <A HREF="mailto:xxx@yyy.com"><FONT ...>xxx@yyy.com</FONT></A>&nbsp
		tempString = OldAttendNames(new_name_start, new_name_end);	        // Get "mailto:xxx@yyy.com"
		tempString4 = tempString(7, tempString.GetStrlen());				// Remove "mailto:"

		// tempString4 now holds "xxx@yyy.com". See if it is in OldAttendNames
		if (NewAttendNames.IndexOf(tempString4.GetBuffer(), (int32)0) == -1)
		{
			// Not there so this must be a removed one. Format it for output
			doAddHTML( doARef( *doFont( tempString4, &tempString2 ), tempString, &tempString3 ) );
		}

		new_name_start   = new_name_end + 1;
		
		// Set it to an empty string
		tempString2 = "";
		tempString3 = "";
		tempString4 = "";
	}
	doAddHTML( Props_HTML_After_Data );

    PR_FREEIF(temp);
    PR_FREEIF(temp1);
}

/*
** Cool diff formating.
*/
void
JulianFormFactory::doDifferenceProcessing(JulianString  icalpropstr)
{
	// First look to see if there is a server version of this.
    // Also diff is only useful for events.
	if (isEvent && serverICalComp)
	{
		JulianString  itipstr;
		JulianString  serverstr;

		if (isPreProcessed(icalpropstr))
		{
			t_int32 ICalPropChar = 	(t_int32)((icalpropstr.GetBuffer())[(TextOffset)1]);

			// Attendees are different. Look at each one and see who has been add or deleted.
			// Right now do not show anything if just their role has changed.
			if (ICalPropChar == ms_cAttendees)
			{
				doDifferenceProcessingAttendees();
			} else
			// Normal case is just diff the string that would be printed.
			{
			JulianString*   Saved_S = s;
			ICalComponent*	IC_Saved = thisICalComp;

				s = &itipstr;
				// Set itipstr to the formated version
				doPreProcessing(icalpropstr.GetBuffer());

				s = &serverstr;
				thisICalComp = serverICalComp;
				// Set serverstr to the formated version
				doPreProcessing(icalpropstr.GetBuffer());
				thisICalComp = IC_Saved;

				s = Saved_S;
			}
		} else {
            char* temp;
            UnicodeString usIcalPropStr = icalpropstr.GetBuffer();
			itipstr	  = JulianString(temp = (thisICalComp->toStringFmt(usIcalPropStr)).toCString(""));
            PR_FREEIF(temp);
			serverstr = JulianString(temp = (serverICalComp->toStringFmt(usIcalPropStr)).toCString(""));
            PR_FREEIF(temp);
		}

		if (itipstr != serverstr)
		{
			doAddHTML( Props_HTML_Before_Label );
			doAddHTML( Start_Font );
				doAddHTML( Italic_Start_HTML );
					doAddHTML( Text_Was );
				doAddHTML( Italic_End_HTML );
			doAddHTML( End_Font );

			doAddHTML( Props_HTML_After_Label );

			doAddHTML( Start_Font );
				doAddHTML( Italic_Start_HTML );
					doAddHTML( serverstr );
				doAddHTML( Italic_End_HTML );
			doAddHTML( End_Font );
			doAddHTML( Props_HTML_After_Data );	
		}
	}
}

void
JulianFormFactory::doProps(int32 labelCount, JulianString  labels[], int32 dataCount, JulianString  data[])
{
    int32 x;
    UnicodeString uDataX;
    JulianString temp_js;
    char* temp;

	doAddHTML( Props_Head_HTML );

    for (x = 0; x < dataCount; x ++)
	{
        uDataX = data[x].GetBuffer();
        uTemp = JulianString(temp = (thisICalComp->toStringFmt(uDataX)).toCString(""));
        PR_FREEIF(temp);
        if (uTemp.GetStrlen() > 0 || isPreProcessed(data[x]))
        {
            doAddHTML( Props_HTML_Before_Label );
		    doAddHTML( Start_Font );
		    if (x < labelCount)
			    doAddHTML( labels[x] );
		    else
			    doAddHTML( Props_HTML_Empty_Label );
		    doAddHTML( End_Font );
		    doAddHTML( Props_HTML_After_Label );
		    doAddHTML( Start_Font );
            temp_js = JulianString(data[x]);
 		    isPreProcessed(temp_js) ? doPreProcessing(data[x].GetBuffer()) : doAddHTML(uTemp);
		    doAddHTML( End_Font );
		    doAddHTML( Props_HTML_After_Data );	
		    doDifferenceProcessing(data[x]);
        }
	}
}

void
JulianFormFactory::doHeader(JulianString  HeaderText)
{
	doAddHTML( General_Header_Start_HTML );
	doAddHTML( Start_Font );
	doAddHTML( HeaderText );
	doAddHTML( End_Font );

	if (detail)
	{
        char* temp;
        char a_temp[256] = "<A NAME=A%d>";
        char a_end_temp[256] = "</A>";

        temp = PR_smprintf(a_temp, aref_count++);
        doAddHTML(temp);
        doAddHTML(a_end_temp);
		doAddHTML( General_Header_Status_HTML );
		doAddHTML( Start_Font );
		// Only can handle new or not new
		doAddHTML( serverICalComp ? "(Update)" : "(New)" );
		doAddHTML( End_Font );
        PR_FREEIF(temp);
	}

	doAddHTML( General_Header_End_HTML );
}

void
JulianFormFactory::doClose()
{
	if (isDetail())
	{ 
		doAddHTML( Head2_HTML );
		doAddButton(doCreateButton(Buttons_Close_Type, Buttons_Close_Label));
	}
}

char*
JulianFormFactory::getJulianErrorString(int32 ErrorNum)
{
   JulianString* errortext = nil;

    if ((t_int32)ErrorNum == nsCalLogErrorMessage::Instance()->ms_iDTEndBeforeDTStart) errortext = &error1; else
    if ((t_int32)ErrorNum == nsCalLogErrorMessage::Instance()->ms_iInvalidPromptValue) errortext = &error2; else
    if ((t_int32)ErrorNum == nsCalLogErrorMessage::Instance()->ms_iInvalidTimeStringError) errortext = &error3; else
    if ((t_int32)ErrorNum == nsCalLogErrorMessage::Instance()->ms_iInvalidRecurrenceError) errortext = &error4; else
    if ((t_int32)ErrorNum == nsCalLogErrorMessage::Instance()->ms_iInvalidPropertyValue) errortext = &error5; else
    if ((t_int32)ErrorNum == nsCalLogErrorMessage::Instance()->ms_iInvalidPropertyName) errortext = &error6; else
    if ((t_int32)ErrorNum == nsCalLogErrorMessage::Instance()->ms_iInvalidParameterName) errortext = &error7; else
    errortext = &error0;

    return errortext->GetBuffer();
}

void
JulianFormFactory::doStatus()
{
    XP_Bool startSpacer = TRUE;
    int32 x = 0;
    JulianPtrArray * errors = 0;
    char * cjst = 0;
    JulianString jst3;
    JulianString jst4;
    //JulianPtrArray *errorLog = firstCalendar->getLog()->GetErrorVector();
    
    // Start with status items that only can happen in detail mode.
	// Only suport vevents.
	if (isDetail() && isEvent)
	{
        JulianString temp_js = JulianString(EventNote);
        JulianString temp_js1 = JulianString(EventInSchedule);
        JulianString temp_js2 = JulianString(EventNotInSchedule);

        // Add event allready on Calendar Server or not.
		doSingleTableLine(temp_js, serverICalComp ? temp_js1 : temp_js2, startSpacer);

		// See if it's not on the server, see if it overlaps any other event
		// doSingleTableLine(&EventConflict, &EventTest, FALSE);
        startSpacer = FALSE;
	}
        
    if (!jf->isFoundNLSDataDirectory())
    {
        // XXX: bad hack
        t_int32 errorID;
        JulianString u;
        jst3 = EventError;
        errorID = 123456789;
        u = "ERROR: Cannot find NLS Data Directory";
        doSingleTableLine(jst3, u, TRUE);
    }
    errors = thisICalComp ? firstCalendar->getLogVector(thisICalComp) : firstCalendar->getLogVector();
    if (errors != nil)
    {
        nsCalLogError * element;
        int32 error_max = MIN(5, errors->GetSize());
        int32 lost_errors = errors->GetSize() - error_max;
        JulianString u;

        for (x=0; x < error_max; x++)
        {
            char* errorprefix;

            element = (nsCalLogError *)errors->GetAt(x);
            u = "";
		    
            cjst = element->getShortReturnStatusCode().toCString("");
            if (cjst != 0)
            {
                u += cjst;
                delete [] cjst; cjst = 0;
            }
            u += " ";
            if ((errorprefix = getJulianErrorString(element->getErrorID())) != nil)
            {
                u += errorprefix;   
                u += ": ";
            }
            u += "(";
            cjst = element->getOffendingData().toCString("");
            if (cjst != 0)
            {
                u += cjst;
                delete [] cjst; cjst = 0;
            }
            u += ")";
            jst3 = EventError;
            doSingleTableLine(jst3, u, startSpacer);
            startSpacer = FALSE;
        }

        if (lost_errors > 0)
        {
            char* temp = PR_smprintf(MoreErrors.GetBuffer(), lost_errors);

            if (temp)
            {
                jst3 = EventError;
                jst4 = temp;
                doSingleTableLine(jst3, jst4, startSpacer);
                PR_DELETE(temp);
            }
        }
    }
        
}

void
JulianFormFactory::doSingleTableLine(JulianString& labelString, JulianString& dataString, XP_Bool addSpacer)
{
	if (addSpacer) doAddHTML( Head2_HTML );
	doAddHTML( Props_HTML_Before_Label );
	if (labelString.GetStrlen() == 0) // Is empty?
	{
		doAddHTML( Props_HTML_Empty_Label );
	} else {
		doAddHTML( Start_Font );
		doAddHTML( labelString );
		doAddHTML( End_Font );
	}
	doAddHTML( Props_HTML_After_Label );
	doAddHTML( Start_Font );
	doAddHTML( dataString );
	doAddHTML( End_Font );
	doAddHTML( Props_HTML_After_Data );	
}

void
JulianFormFactory::doCommentText()
{
	// Comments Field
	doAddHTML( Head2_HTML );
	doAddHTML( Text_Label_Start_HTML );
	doAddHTML( Start_Font );
	doAddHTML( Text_Label );
	doAddHTML( End_Font );
	doAddHTML( Text_Label_End_HTML );
	doAddHTML( Text_Field_HTML );
}

//
// To add a group button on it's own line
void
JulianFormFactory::doAddGroupButton(JulianString GroupButton_HTML)
{
	doAddHTML( Start_Font );
	doAddHTML( GroupButton_HTML );
	doAddHTML( End_Font );
}

//
// To add a single button on it's own line
void
JulianFormFactory::doAddButton(JulianString  SingleButton_HTML)
{
	doAddHTML( Buttons_GroupSingleStart_HTML );
	doAddGroupButton(SingleButton_HTML);
	doAddHTML( Buttons_GroupSingleEnd_HTML );
}

//
// To add a single button on it's own line
JulianString
JulianFormFactory::doCreateButton(JulianString InputType, JulianString ButtonName, XP_Bool addTextField)
{
	JulianString  t = "";
//	char*	url_string = cb ? cb->callbackurl( (void *)nil, InputType->toCString("")) : "";
	char*	url_string =  "";
	char*	temp = nil;
	char*	temp2 = nil;

    if (addTextField && ButtonName.GetStrlen())
    {
        char* temp3 = ButtonName.GetBuffer();

        if (temp3)
        {
            temp2 = PR_smprintf(Buttons_Text_End_HTML, temp3);
        }
    }

	temp = PR_smprintf(Buttons_Single_Start_HTML, url_string);
	t += Start_Font;
	if (temp) t += temp;
	t += InputType;
	t += Buttons_Single_Mid_HTML;
	t += ButtonName;
	t += addTextField ? temp2 : Buttons_Single_End_HTML;
	t += End_Font;
    PR_FREEIF(temp);
    PR_FREEIF(temp2);

	return t;
}

void
JulianFormFactory::addLegend()
{
	doAddHTML(FBT_Legend_Start_HTML);
	
    doAddHTML(FBT_Legend_Text1_HTML);
	doAddHTML(Start_Font);
	doAddHTML(FBT_Legend_Title);
	doAddHTML(End_Font);
	doAddHTML(FBT_Legend_TextEnd_HTML);
	
    doAddHTML(FBT_Legend_Text2_HTML);
	doAddHTML(Start_Font);
	doAddHTML(FBT_Legend_Free);
	doAddHTML(End_Font);
	doAddHTML(FBT_Legend_TextEnd_HTML);
	
    doAddHTML(FBT_Legend_Text3_HTML);
	doAddHTML(Start_Font);
	doAddHTML(FBT_Legend_Busy);
	doAddHTML(End_Font);
	doAddHTML(FBT_Legend_TextEnd_HTML);
    
    doAddHTML(FBT_Legend_Text4_HTML);
	doAddHTML(Start_Font);
	doAddHTML(FBT_Legend_Unknown);
	doAddHTML(End_Font);
	doAddHTML(FBT_Legend_TextEnd_HTML);

    doAddHTML(FBT_Legend_Text5_HTML);
	doAddHTML(Start_Font);
	doAddHTML(FBT_Legend_xparam);
	doAddHTML(End_Font);
	doAddHTML(FBT_Legend_TextEnd_HTML);

	doAddHTML(FBT_Legend_End_HTML);
}

void
JulianFormFactory::addTimeZone()
{
    if (displayTimeZone)
    {
		doAddHTML( Start_Font );
		doAddHTML(FBT_TimeHead_HTML);
		doAddHTML(tz);
		doAddHTML(FBT_TimeHeadEnd_HTML);
		doAddHTML( End_Font );
		doAddHTML( nbsp );
    } else
        doAddHTML( nbsp );
}

void
JulianFormFactory::addMajorScale()
{
    int32   x;
    char ss[10];
    UnicodeString usTemp;
    switch (scaleType)
    {
    case 1:     // Days 1 to 31
        doAddHTML(FBT_TDOffsetCell_HTML);
		for (x = 1; x < 32; x++)
        {

 			sprintf(ss, "%d", x);
			doAddHTML(TimeHour_HTML);
			doAddHTML(Start_Font);
			doAddHTML(FBT_TD_HourColor_HTML);
            if (x < 10) doAddHTML(nbsp);
			doAddHTML(ss);
			doAddHTML(FBT_TD_HourColorEnd_HTML);
			doAddHTML(End_Font);
			doAddHTML(FBT_TimeHourEnd_HTML);
        }
        break;
 
    case 2:
		for (x = start_hour; x < end_hour; x++)
		{
		    int32 y = (x == 0) ? 12 : (x > 12) ? (x - 12) : (x); // Translate midnight to 12, convert 24h to 12h 

			sprintf(ss, "%d", y);
			doAddHTML(TimeHour_HTML);
			doAddHTML(Start_Font);
			doAddHTML(FBT_TD_HourColor_HTML);
			doAddHTML(ss);
			switch (x)
			{
				case 0:		doAddHTML(FBT_AM); break;
				case 12:	doAddHTML(FBT_PM); break;
				default: 
					if ( x == start_hour) 
                    { 
                        char*  temps = " a";
                        char*  temp;
                        usTemp = temps;
                        uTemp = JulianString(temp = (start.strftime(usTemp)).toCString(""));
                        PR_FREEIF(temp);
                        doAddHTML(uTemp);
                    };
			}
			doAddHTML(FBT_TD_HourColorEnd_HTML);
			doAddHTML(End_Font);
			doAddHTML(FBT_TimeHourEnd_HTML);
		}
        break;
    }

//	doAddHTML(FBT_TDOffsetCell_HTML);
	doAddHTML(FBT_EndRow_HTML);
}

void
JulianFormFactory::addMinorScale()
{
    int32 x;

    if (scaleType == 2)
    {
	    // Minutes 00 or 30
	    doAddHTML(FBT_NewRow_HTML);

        JulianString  tickarray[] = {FBT_TickMark1, FBT_TickMark2, FBT_TickMark3, FBT_TickMark4};
	    int32 yoffset = (SlotsPerHour == 4) ? 1 : 2;
	    for (x = start_hour; x < end_hour; x ++)
	    {
		    for (int32 y = 0; y < SlotsPerHour; y ++)
		    {
			    doAddHTML(FBT_TimeMin_HTML);
			    doAddHTML(Start_Font);
			    doAddHTML(FBT_TD_MinuteColor_HTML);
			    doAddHTML(tickarray[y * yoffset]);
			    doAddHTML(FBT_TD_MinuteColorEnd_HTML);
			    doAddHTML(End_Font);
			    doAddHTML(FBT_TimeMinEnd_HTML);
		    }
	    }
//	    doAddHTML(FBT_TDOffsetCell_HTML);
	    doAddHTML(FBT_EndRow_HTML);
    } else
    {
 		doAddHTML(FBT_NewRow_HTML);
		doAddHTML(FBT_EndRow_HTML);
    }
}

void
JulianFormFactory::addTicksScale()
{
#if 0
    int32 x;
    
    if (detail && scaleType == 2)
    {
		doAddHTML(FBT_NewRow_HTML);
		doAddHTML(FBT_TDOffsetCell_HTML);
		for (x = start_hour; x < end_hour; x ++)
		{
			for (int32 y = 0; y < SlotsPerHour; y ++)
			{
				doAddHTML(FBT_TimeMin_HTML);
				doAddHTML(Start_Font);
				doAddHTML(y & 1 ? FBT_TickShort_HTML : FBT_TickLong_HTML);
				doAddHTML(End_Font);
				doAddHTML(FBT_TimeMinEnd_HTML);
			}
		}
//		doAddHTML(FBT_TDOffsetCell_HTML);
		doAddHTML(FBT_EndRow_HTML);
    }
#endif
}

#if 0
void
JulianFormFactory::emptyRow()
{
    char* temp;
    char* temp2 = FBT_DayXColEmptyCell_HTML.toCString("");

    doAddHTML(FBT_NewRow_HTML);
	doAddHTML(FBT_DayEmptyCell_HTML); // No Title
	doAddHTML(FBT_DayEmptyCell_HTML); // offset to first timeslot

	// All of the time slots
    temp = PR_smprintf(temp2, slotsacross);
    doAddHTML(temp);
    doAddHTML(FBT_EndRow_HTML);
    PR_FREEIF( temp2 );
    PR_FREEIF( temp );
}
#endif

void
JulianFormFactory::LineInc(DateTime* dtTime)
{
    switch (scaleType)
    {
        case 2: DaysLineInc(dtTime); break;
        case 1: MonthLineInc(dtTime); break;
    }
}

void
JulianFormFactory::DaysLineInc(DateTime* dtTime)
{
    dtTime->nextDay();
}

void
JulianFormFactory::MonthLineInc(DateTime* dtTime)
{
    dtTime->nextMonth();
    dtTime->setDayOfMonth(1);
}

// Return TRUE if this line is still within it's time range
int32
JulianFormFactory::LineCheck(DateTime& baseTime, DateTime& checkTime, DateTime& checkTimeEnd)
{

    if (checkTime > ((VFreebusy *)thisICalComp)->getDTEnd())
    {
        new_state = Empty_State;
        return FALSE;
    }

    if (checkTimeEnd < start)
    {
        new_state = Empty_State;
        return FALSE;
    }

    switch (scaleType)
    {
    case 1:
        if ((baseTime.getMonth(default_tz) != checkTime.getMonth(default_tz)))
            new_state = Empty_State;
        return (baseTime.getMonth(default_tz) == checkTime.getMonth(default_tz));
        break;
    default:
        return TRUE;
        break;
    }

    return TRUE;
}

void
JulianFormFactory::enterState(FB_State newState, XP_Bool forse = FALSE)
{
	if (!forse &&
        newState == state)
        count++;
    else
    {
        char* temp = nil;
        char* temp2;

        switch (state)
        {
            case Free_State:    temp2 = FBT_DayXColFreeCell_HTML; break;
            case Busy_State:    temp2 = FBT_DayXColBusyCell_HTML; break;
            case Empty_State:   temp2 = FBT_DayXColEmptyCell_HTML; break;
            case XParam_State:  temp2 = FBT_DayXXParamCell_HTML; break;
            default: temp2 = "";
        }

        state = newState;
        
        if (count > 0)
        {
            temp = PR_smprintf(temp2, count);
            doAddHTML(temp);
        }

        count = forse ? 0 : 1;

        PR_FREEIF(temp);
    }
}

/*
** Set p to the next period.
**
** Returns a Period or nil when there are no more periods
*/
Period*
JulianFormFactory::getNextPeriod()
{
	if (pIndex >= pv->GetSize())	// Have more periods in this freebusy?
	{
		// No more periods in this freebusy, try the next freebusy
		if (fbIndex >= fbv->GetSize())
		{
			p = (Period *)nil;	// Ran out of periods and freebusys all togeather
			pv = (JulianPtrArray *)nil;
		} else
		{
			fb = (Freebusy *)fbv->GetAt(fbIndex++);
			pv = fb->getPeriod(); pIndex = 0;
		}
    } else {
        assert(FALSE);
    }
	if (pv) p = (Period *)pv->GetAt(pIndex++);

#ifdef DEBUG_eyork
    if (p)
    {
        char sbuf[1024];
        sprintf(sbuf,"%s", p->toString().toCString(""));
    }
#endif

    return p;
}


/*
** Return the FB_State of the current Freebusy, fb.
*/
int32
JulianFormFactory::getFBType()
{
int32 x = (int32)Empty_State;

    if (fb)
    {
        if (fb->getType() == Freebusy::FB_TYPE_BUSY ||
            fb->getType() == Freebusy::FB_TYPE_BUSY_UNAVAILABLE ||
            fb->getType() == Freebusy::FB_TYPE_BUSY_TENTATIVE)
            x = (int32)Busy_State;
        else
            if (fb->getType() == Freebusy::FB_TYPE_XPARAMVAL)
                x = (int32)XParam_State;
            else
                x = (int32)Free_State;
    }

    return x;
}

/*
** Look at the current period to see if it's time to go 
** onto the next one.
**
** There are three cases here:
**
** 1. The current period didn't expire in the last time slot
** 
** 2. The current period did expire in the last time slot
**
** 3. The current period is the last one.
**
** When moving on to the next period, check to see if more then
** one period over laps the next time slot. If so, set status to
** busy if any of them are busy. Set p to the last period.
*/
void
JulianFormFactory::checkPeriodBounds(DateTime& startofslot, DateTime& endofslot)
{
	if (p)
    {
 		DateTime PeriodEnd;
        XP_Bool a = TRUE;
 
        new_state = (FB_State)getFBType();    // Reset new_state to the state from the last period

        // Check to see if this is the last Period that
        // over laps this time slot. The current
        // period, p, will be set to the last period
        // that over laps.
        while (a & (p != nil))
        {
            // First case test. See if this time slot has made it
            // to this period. May not be the case on the very
            // first period.
            if (p->getStart() > endofslot) // there yet?
            {
                new_state = Free_State;
                break;
            }

            p->getEndingTime(PeriodEnd);
            // Is this new slot passed the current period?
            if (!endofslot.beforeDateTime(PeriodEnd))
            {
                DateTime PeriodStart;
	            
                getNextPeriod(); // need a new period. Changes p and fb.

                if (p)
                {
                    FB_State next_state = (FB_State)getFBType();

                    PeriodStart = p->getStart();
                    if ((new_state < next_state) ||                   // Once new_state goes busy in this time slot, it stays that way.
                       (startofslot.equalsDateTime(PeriodStart)))     // But change states if the new period starts right on this startofslot
                    {
                        new_state = next_state;
                    }
                }
            } else
                a = FALSE;
        }
    } else
        new_state = Free_State; // No periods but in time slot range assume free
}

void
JulianFormFactory::makeHourFBTable()
{
    UnicodeString usFBT_TickSetting = FBT_TickSetting;
    MinutesPerSlot = nsCalDuration(usFBT_TickSetting);
	SlotsPerHour = (60 / MinutesPerSlot.getMinute());
    displayTimeZone = TRUE;
    scaleType = 2;
    hoursToDisplay = 24;
    slotsacross = SlotsPerHour * hoursToDisplay;
    lineFormat = FBT_DayFormat;
}

void
JulianFormFactory::makeDaysFBTable()
{
    UnicodeString usFBT_TickDaySetting = FBT_TickDaySetting;
    MinutesPerSlot = nsCalDuration(usFBT_TickDaySetting);
	SlotsPerHour = 1;
    displayTimeZone = FALSE;
    scaleType = 1;
    hoursToDisplay = 0;
    slotsacross = 31;
    lineFormat = FBT_MonthFormat;
}

//
// Make a FreeBusy Table
//
void
JulianFormFactory::doMakeFreeBusyTable()
{
	if (isFreeBusy)
	{
		JulianString	timezoneformat = "z";
        nsCalDuration tempdur;
        int32           x;
		char*           temp2 = nil;
        char*           temp;
        UnicodeString usFBT_HourStart;
        UnicodeString usFBT_HourEnd;
        UnicodeString usTimeZoneFormat;
        UnicodeString usFBT_TickOffset;
        UnicodeString usLineFormat;
        UnicodeString tempuni;

        new_state = Empty_State;
        // set start and end (to remove warnings John Sun 5-1-98)
        start =         ((VFreebusy *)thisICalComp)->getDTStart();
		end =           ((VFreebusy *)thisICalComp)->getDTEnd();
		
        // normalize end. must stop on an even day/time.
        end.setDMY(end.getDate() + 1, end.getMonth(), end.getYear());
        end.set(Calendar::HOUR_OF_DAY, 0);
        end.set(Calendar::MINUTE, 0);
        end.set(Calendar::SECOND, 0);

        DateTime::getDuration(start, end, tempdur);

        tempdur.getMonth() >= 2 ? makeDaysFBTable() : makeHourFBTable();
        PR_FREEIF(TimeHour_HTML);
		TimeHour_HTML = PR_smprintf(temp2 = FBT_TimeHour_HTML, SlotsPerHour);

		// This is the Hour row ie 6am to 7 pm etc
        usFBT_HourStart = FBT_HourStart;
        usFBT_HourEnd = FBT_HourEnd;

		start_hour = DateTime(usFBT_HourStart).getHour(default_tz);
		end_hour = DateTime(usFBT_HourEnd).getHour(default_tz);
        usTimeZoneFormat = timezoneformat.GetBuffer();

		if (DateTime(usFBT_HourEnd).getMinute(default_tz) != 0) end_hour++; // Only show full hours. This is to get 11pm to work
		tz = JulianString(temp = (start.strftime(usTimeZoneFormat, default_tz)).toCString(""));
        PR_FREEIF(temp);

		// Legend
        addLegend();

        doAddHTML(FBT_Start_HTML);
		doAddHTML(FBT_NewRow_HTML);

        // This is the TimeZone Heading
        addTimeZone();

        // Scale
        addMajorScale();
        addMinorScale();
        addTicksScale();

		fbv = isFreeBusy ? ((VFreebusy *)thisICalComp)->getFreebusy() : nil;
		pv = nil;
		fb = nil;
		p  = nil;
		fbIndex = 0;
		pIndex = 0;

        if (fbv) fb = (Freebusy *)fbv->GetAt(fbIndex++);
		if (fb) pv = fb->getPeriod(); pIndex = 0;
		if (pv) p = (Period *)pv->GetAt(pIndex++);

        usFBT_TickOffset = FBT_TickOffset;
        // Each day or month now. So this this each line in the table
        for (DateTime currentday = start; currentday < end; LineInc(&currentday))
		{
            char* temp;

		    nsCalDuration d_offset = nsCalDuration(usFBT_TickOffset);
			// int32 slot_count;
			DateTime StartOfSlot, EndOfSlot;

            StartOfSlot = DateTime(     currentday.getYear(default_tz),
                                        currentday.getMonth(default_tz),
                                        MinutesPerSlot.getDay() > 0 ? 1 : currentday.getDate(default_tz),
                                        MinutesPerSlot.getDay() > 0 ? 0 : start_hour,
                                        0);
			EndOfSlot = DateTime(StartOfSlot);
			EndOfSlot.add(MinutesPerSlot);

			doAddHTML(FBT_NewRow_HTML);
			doAddHTML(FBT_DayStart_HTML);

            usLineFormat = lineFormat.GetBuffer();
            tempuni = currentday.strftime(usLineFormat);
            uTemp = JulianString(temp = tempuni.toCString(""));
            PR_FREEIF(temp);
			doAddHTML(Start_Font);
			doAddHTML(uTemp);
            doAddHTML(End_Font);
            doAddHTML(FBT_DayEnd_HTML);
 
			// Loop for the number of timeslots, each slot is 30 minutes or 1 day.
            // slot_count = MinutesPerSlot.getDay() > 0 ? 31 : (end_hour - start_hour) * SlotsPerHour;

            // In Free State = 0, Busy State = 1, Empty State = 2
            // This is each column in the table
			for (x = 0, state = Free_State, count = 0; x < slotsacross;  x++, StartOfSlot.add(MinutesPerSlot), EndOfSlot.add(MinutesPerSlot))
			{
#ifdef DEBUG_eyork
                char sbuf[1024];
                sprintf(sbuf,"%s", StartOfSlot.toString().toCString(""));
                sprintf(sbuf,"%s", EndOfSlot.toString().toCString(""));
#endif

				// Do either free, busy, or not in this time slot
                // First make sure that this slot hasn't left the range for this line. ie Feb 28 should not go to Mar 1 on one line

                // Did the month end on this line? or Still have to finish this line, did go pass dtend?
                // Sets new_state if needed
                if (LineCheck(currentday, StartOfSlot, EndOfSlot)) 
                    checkPeriodBounds(StartOfSlot, EndOfSlot);

                enterState(new_state); 
			}

            enterState(state, TRUE);            // End of line, force output.

            // Padding at the end of each line, etc
			doAddHTML(FBT_DayEnd_HTML);
			doAddHTML(FBT_TimeMin_HTML);
			// doAddHTML(FBT_DayEmptyCell_HTML);
			doAddHTML(FBT_TimeMinEnd_HTML);
			doAddHTML(FBT_EndRow_HTML);

        }

		doAddHTML(FBT_End_HTML);
	}
}

void
JulianFormFactory::HandlePublishVEvent()
{
	int32	data_length = isDetail() ? publish_D_Fields_Data_HTML_Length : publish_Fields_Data_HTML_Length;
	int32	label_length = isDetail() ? publish_D_Fields_Labels_Length : publish_Fields_Labels_Length;

	doHeader(publish_Header_HTML);
	doProps(label_length, isDetail() ? publish_D_Fields_Labels : publish_Fields_Labels, data_length, isDetail() ? publish_D_Fields_Data_HTML : publish_Fields_Data_HTML);
	doStatus();

	if (isDetail())
	{
		if (do_capi_login) doAddButton(doCreateButton(Buttons_Add_Type, Buttons_Add_Label));
		doClose();
	} else {
		doAddButton(doCreateButton(Buttons_Details_Type, Buttons_Details_Label));
		if (do_capi_login) 
        {
            doAddHTML( Head2_HTML );
		    doAddButton(doCreateButton(Buttons_Add_Type, Buttons_Add_Label));
        }
	}
	doAddHTML( Props_End_HTML );
	doAddHTML( publish_End_HTML );
}

void
JulianFormFactory::HandleError()
{
	int32	data_length = error_Fields_Data_HTML_Length;
	int32	label_length = error_Fields_Labels_Length;

	doHeader(error_Header_HTML);
    doAddHTML(Props_Head_HTML);
	doStatus();

	doAddHTML( Props_End_HTML );
	doAddHTML( error_End_HTML );
}

void
JulianFormFactory::HandlePublishVFreeBusy(XP_Bool isPublish)
{
	int32	data_length = isDetail() ? publishFB_D_Fields_Data_HTML_Length : publishFB_Fields_Data_HTML_Length;
	int32	label_length = isDetail() ? publishFB_D_Fields_Labels_Length : publishFB_Fields_Labels_Length;

	doHeader(isPublish ? publishFB_Header_HTML : replyFB_Header_HTML);
	doProps(label_length, isDetail() ? publishFB_D_Fields_Labels : publishFB_Fields_Labels, data_length, isDetail() ? publishFB_D_Fields_Data_HTML : publishFB_Fields_Data_HTML);
	doStatus();

	if (isDetail())
	{
		doClose();
	} else {
		doAddButton(doCreateButton(Buttons_Details_Type, Buttons_Details_Label));
		doAddHTML( Head2_HTML );
	}
	doAddHTML( Props_End_HTML );

	doMakeFreeBusyTable();

	doAddHTML( publishFB_End_HTML );
}

void
JulianFormFactory::HandleRequestVEvent()
{
	int32	data_length = isDetail() ? request_D_Fields_Data_HTML_Length : request_Fields_Data_HTML_Length;
	int32	label_length = isDetail() ? request_D_Fields_Labels_Length : request_Fields_Labels_Length;

	doHeader(request_Header_HTML);
	doProps(label_length, isDetail() ? request_D_Fields_Labels : request_Fields_Labels, data_length, isDetail() ? request_D_Fields_Data_HTML : request_Fields_Data_HTML);
	if (!isDetail()) doAddButton(doCreateButton(Buttons_Details_Type, Buttons_Details_Label));
	doStatus();
	doCommentText();
	if (isDetail()) doAddButton(Buttons_SaveDel_HTML);

	doAddHTML( Buttons_GroupStart_HTML );
	doAddGroupButton(doCreateButton(Buttons_Accept_Type, Buttons_Accept_Label));
	doAddGroupButton(doCreateButton(Buttons_Decline_Type, Buttons_Decline_Label));
	if (isDetail())
	{
		// Add Save copy here
		doAddGroupButton(doCreateButton(Buttons_Tentative_Type, Buttons_Tentative_Label));
		doAddGroupButton(doCreateButton(Buttons_DelTo_Type, Buttons_DelTo_Label, TRUE));
	} else {
		doAddGroupButton(doCreateButton(Buttons_DelTo_Type, Buttons_DelTo_Label, TRUE));
	}
	doAddHTML( Buttons_GroupEnd_HTML );

	doClose();
	doAddHTML( Props_End_HTML );
	doAddHTML( request_End_HTML );
}

void
JulianFormFactory::HandleRequestVFreeBusy( )
{
	int32	data_length = isDetail() ? request_D_FB_Fields_Data_HTML_Length : request_FB_Fields_Data_HTML_Length;
	int32	label_length = isDetail() ? request_D_FB_Fields_Labels_Length : request_FB_Fields_Labels_Length;

	doHeader(request_FB_Header_HTML);
	doProps(label_length, isDetail() ? request_D_FB_Fields_Labels : request_FB_Fields_Labels, data_length, isDetail() ? request_D_FB_Fields_Data_HTML : request_FB_Fields_Data_HTML);
	if (!isDetail()) doAddButton(doCreateButton(Buttons_Details_Type, Buttons_Details_Label));
	doStatus();

	doAddHTML( Head2_HTML );
	if (isDetail()) { doAddHTML( Props_End_HTML ); doMakeFreeBusyTable(); doAddHTML( Props_Head_HTML ); doAddHTML( Head2_HTML ); }

	if (do_capi_login) doAddButton(doCreateButton(Buttons_SendFB_Type, Buttons_SendFB_Label));
	doClose();
	doAddHTML( Props_End_HTML );

	doAddHTML( request_End_HTML );
}

void
JulianFormFactory::HandleEventReplyVEvent()
{
	int32	data_length = isDetail() ? eventreply_D_Fields_Data_HTML_Length : eventreply_Fields_Data_HTML_Length;
	int32	label_length = isDetail() ? eventreply_D_Fields_Labels_Length : eventreply_Fields_Labels_Length;

	doHeader(eventreply_Header_HTML);
	doProps(label_length, isDetail() ? eventreply_D_Fields_Labels : eventreply_Fields_Labels, data_length, isDetail() ? eventreply_D_Fields_Data_HTML : eventreply_Fields_Data_HTML);
	doStatus();

	if (isDetail())
	{
		if (do_capi_login) doAddButton(doCreateButton(Buttons_Add_Type, Buttons_Update_Label));
		doClose();
	} else {
		doAddButton(doCreateButton(Buttons_Details_Type, Buttons_Details_Label));
		if (do_capi_login)
        {
            doAddHTML( Head2_HTML );
		    doAddButton(doCreateButton(Buttons_Add_Type, Buttons_Update_Label));
        }
	}
	doAddHTML( Props_End_HTML );
	doAddHTML( eventreply_End_HTML );
}

void
JulianFormFactory::HandleEventCancelVEvent()
{
	int32	data_length = isDetail() ? eventcancel_D_Fields_Data_HTML_Length : eventcancel_Fields_Data_HTML_Length;
	int32	label_length = isDetail() ? eventcancel_D_Fields_Labels_Length : eventcancel_Fields_Labels_Length;

	doHeader(eventcancel_Header_HTML);
	doProps(label_length, isDetail() ? eventcancel_D_Fields_Labels : eventcancel_Fields_Labels, data_length, isDetail() ? eventcancel_D_Fields_Data_HTML : eventcancel_Fields_Data_HTML);
	doStatus();

	if (isDetail())
	{
		if (do_capi_login) doAddButton(doCreateButton(Buttons_Add_Type, Buttons_Add_Label));
		doClose();
	} else {
		doAddButton(doCreateButton(Buttons_Details_Type, Buttons_Details_Label));
		if (do_capi_login) 
        {
            doAddHTML( Head2_HTML );
		    doAddButton(doCreateButton(Buttons_Add_Type, Buttons_Update_Label));
        }
	}
	doAddHTML( Props_End_HTML );
	doAddHTML( eventcancel_End_HTML );
}

void
JulianFormFactory::HandleEventRefreshRequestVEvent()
{
	int32	data_length = isDetail() ? eventrefreshreg_D_Fields_Data_HTML_Length : eventrefreshreg_Fields_Data_HTML_Length;
	int32	label_length = isDetail() ? eventrefreshreg_D_Fields_Labels_Length : eventrefreshreg_Fields_Labels_Length;

	doHeader(eventrefreshreg_Header_HTML);
	doProps(label_length, isDetail() ? eventrefreshreg_D_Fields_Labels : eventrefreshreg_Fields_Labels, data_length, isDetail() ? eventrefreshreg_D_Fields_Data_HTML : eventrefreshreg_Fields_Data_HTML);
	doStatus();
	doCommentText();

	if (isDetail())
	{
		if (do_capi_login) doAddButton(doCreateButton(Buttons_Add_Type, Buttons_Add_Label));
		if (do_capi_login) doAddButton(doCreateButton(Buttons_SendRefresh_Type, Buttons_SendRefresh_Label));
		doClose();
	} else {
		doAddButton(doCreateButton(Buttons_Details_Type, Buttons_Details_Label));
		if (do_capi_login) 
        {
            doAddHTML( Head2_HTML );
		    doAddButton(doCreateButton(Buttons_SendRefresh_Type, Buttons_SendRefresh_Label));
        }
	}
	doAddHTML( Props_End_HTML );
	doAddHTML( eventrefreshreg_End_HTML );
}

void
JulianFormFactory::HandleEventCounterPropVEvent()
{
	int32	data_length = isDetail() ? request_D_Fields_Data_HTML_Length : request_Fields_Data_HTML_Length;
	int32	label_length = isDetail() ? request_D_Fields_Labels_Length : request_Fields_Labels_Length;

	doHeader(eventcounterprop_Header_HTML);
	doProps(label_length, isDetail() ? request_D_Fields_Labels : request_Fields_Labels, data_length, isDetail() ? request_D_Fields_Data_HTML : request_Fields_Data_HTML);
	if (!isDetail()) doAddButton(doCreateButton(Buttons_Details_Type, Buttons_Details_Label));
	doStatus();
	doCommentText();
	if (isDetail()) doAddButton(Buttons_SaveDel_HTML);

	doAddHTML( Buttons_GroupStart_HTML );
	doAddGroupButton(doCreateButton(Buttons_Accept_Type, Buttons_Accept_Label));
	doAddGroupButton(doCreateButton(Buttons_Decline_Type, Buttons_Decline_Label));
	doAddHTML( Buttons_GroupEnd_HTML );

	doClose();
	doAddHTML( Props_End_HTML );
	doAddHTML( request_End_HTML );
}

void
JulianFormFactory::HandleDeclineCounterVEvent()
{
	int32	data_length = eventdelinecounter_Fields_Data_HTML_Length;
	int32	label_length = eventdelinecounter_Fields_Labels_Length;

	doHeader(eventdelinecounter_Header_HTML);
	doProps(label_length, eventdelinecounter_Fields_Labels, data_length, eventdelinecounter_Fields_Data_HTML);
	doStatus();
	doAddHTML( Props_End_HTML );
	doAddHTML( request_End_HTML );
}

/*
** General HTML
*/
char*         JulianFormFactory::Start_HTML					= "<FORM ACTION=%s METHOD=GET><P><TABLE BORDER=1 CELLPADDING=10 CELLSPACING=0><TR><TD>\n";
char*         JulianFormFactory::Props_Head_HTML			= "<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0 colspacing=5 colpadding=5>\n";
char*         JulianFormFactory::Props_HTML_Before_Label	= "<TR><TD WIDTH=110 ALIGN=RIGHT VALIGN=TOP><P ALIGN=RIGHT><B>";
char*         JulianFormFactory::Props_HTML_After_Label		= "</B>&nbsp;&nbsp;</TD><TD>";
char*         JulianFormFactory::Props_HTML_After_Data		= "&nbsp;</TD></TR>\n";
char*         JulianFormFactory::Props_HTML_Empty_Label		= "<TR><TD WIDTH=110>&nbsp;</TD><TD>&nbsp;</TD></TR><TR><TD WIDTH=110><DIV ALIGN=right>&nbsp;</DIV></TD>";
char*         JulianFormFactory::Props_End_HTML				= "</TABLE>";
char*         JulianFormFactory::End_HTML					= "</TD></TR></TABLE></FORM>";
char*         JulianFormFactory::General_Header_Start_HTML	= "<TABLE WIDTH=100% BORDER=0 CELLPADDING=0 CELLSPACING=0 BGCOLOR=#006666><TR><TD>&nbsp;<FONT COLOR=#FFFFFF><B>";
char*         JulianFormFactory::General_Header_Status_HTML	= "</B></FONT></TD><TD><P ALIGN=RIGHT><FONT COLOR=#FFFFFF><B>";
char*         JulianFormFactory::General_Header_End_HTML	= "</B></FONT></TD></TR></TABLE>\n";
char*         JulianFormFactory::Head2_HTML					= "<TR><TD COLSPAN=\"2\"><HR></TD></TR>\n";
char*         JulianFormFactory::Italic_Start_HTML			= "<I>";
char*         JulianFormFactory::Italic_End_HTML			= "</I>";
char*         JulianFormFactory::Bold_Start_HTML			= "<B>";
char*         JulianFormFactory::Bold_End_HTML				= "</B>";
char*         JulianFormFactory::Aref_Start_HTML			= "<A HREF=\"";
char*         JulianFormFactory::Aref_End_HTML				= "\">";
char*         JulianFormFactory::ArefTag_End_HTML			= "</A>";
char*         JulianFormFactory::nbsp						= "&nbsp;";
char*         JulianFormFactory::Accepted_Gif_HTML			= "+ ";
char*         JulianFormFactory::Declined_Gif_HTML			= "- ";
char*         JulianFormFactory::Delegated_Gif_HTML			= "> ";
char*         JulianFormFactory::NeedsAction_Gif_HTML		= "* ";
char*         JulianFormFactory::Question_Gif_HTML			= "? ";
char*         JulianFormFactory::Line_3_HTML				= "<HR ALIGN=CENTER SIZE=3>";
char*         JulianFormFactory::Cell_Start_HTML			= "<TD>";
char*         JulianFormFactory::Cell_End_HTML				= "</TD>";
char*         JulianFormFactory::Font_Fixed_Start_HTML		= "<TT><FONT SIZE=2>";
char*         JulianFormFactory::Font_Fixed_End_HTML		= "</FONT></TT>";
char*         JulianFormFactory::Line_Break_HTML		    = "<BR>";  

/*
** Free/Busy Table
*/
char*         JulianFormFactory::FBT_Start_HTML				= "<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=2 BGCOLOR=#FFFFFF>\n";
char*         JulianFormFactory::FBT_End_HTML				= "</TABLE>";
char*         JulianFormFactory::FBT_NewRow_HTML			= "<TR>\n";
char*         JulianFormFactory::FBT_EndRow_HTML			= "</TR>\n";
char*         JulianFormFactory::FBT_TimeHead_HTML			= "<TH ALIGN=RIGHT VALIGN=TOP ROWSPAN=2 BGCOLOR=#666666>\n";
char*         JulianFormFactory::FBT_TimeHeadEnd_HTML		= "</TH>";
char*         JulianFormFactory::FBT_TimeHour_HTML			= "<TD COLSPAN=%d BGCOLOR=#666666>";
char*         JulianFormFactory::FBT_TimeHourEnd_HTML		= "</TD>\n";
char*         JulianFormFactory::FBT_TimeMin_HTML			= "<TD WIDTH=5 BGCOLOR=#666666>";
char*         JulianFormFactory::FBT_TimeMinEnd_HTML		= "</TD>\n";
char*         JulianFormFactory::FBT_TD_HourColor_HTML		= "<TT><FONT COLOR=#FFFFFF><FONT SIZE=-1>";
char*         JulianFormFactory::FBT_TD_HourColorEnd_HTML	= "</TT></FONT></FONT>";
char*         JulianFormFactory::FBT_TD_MinuteColor_HTML	= "<TT><FONT COLOR=#C0C0C0><FONT SIZE=-2>";
char*         JulianFormFactory::FBT_TD_MinuteColorEnd_HTML	= "</TT></FONT></FONT>";
char*         JulianFormFactory::FBT_TDOffsetCell_HTML		= "<TD BGCOLOR=#666666>&nbsp;</TD>";
char*         JulianFormFactory::FBT_TickLong_HTML			= "<TD VALIGN=TOP WIDTH=5 HEIGHT=10 BGCOLOR=#666666><IMG SRC=\"tickmark.GIF\" BORDER=0 HEIGHT=10 WIDTH=1 ALIGN=BOTTOM></TD>";
char*         JulianFormFactory::FBT_TickShort_HTML			= "<TD VALIGN=TOP WIDTH=5 HEIGHT=10 BGCOLOR=#666666><IMG SRC=\"tickmark.GIF\" BORDER=0 HEIGHT=5 WIDTH=1 ALIGN=BOTTOM></TD>";
char*         JulianFormFactory::FBT_HourStart				= "19971101T000000";
char*         JulianFormFactory::FBT_HourEnd				= "19971101T235959";

char*         JulianFormFactory::FBT_DayStart_HTML			= "<TD NOWRAP ALIGN=LEFT BGCOLOR=#666666><TT><FONT COLOR=#FFFFFF><FONT SIZE=-1>";
char*         JulianFormFactory::FBT_DayEnd_HTML			= "</FONT></FONT></TT></TD>";
char*         JulianFormFactory::FBT_DayEmptyCell_HTML		= "<TD ALIGN=CENTER COLSPAN=1 BGCOLOR=#666666><CENTER>&nbsp;</CENTER></TD>";
char*         JulianFormFactory::FBT_DayFreeCell_HTML		= "<TD ALIGN=CENTER COLSPAN=1 BGCOLOR=#FFFFFF><CENTER>&nbsp;</CENTER></TD>";
char*         JulianFormFactory::FBT_DayBusyCell_HTML		= "<TD ALIGN=CENTER COLSPAN=1 BGCOLOR=#C0C0C0><CENTER>&nbsp;</CENTER></TD>";
char*         JulianFormFactory::FBT_DayXParamCell_HTML		= "<TD ALIGN=CENTER COLSPAN=1 BGCOLOR=#CC0000><CENTER>&nbsp;</CENTER></TD>";
char*         JulianFormFactory::FBT_DayXColFreeCell_HTML   = "<TD ALIGN=CENTER COLSPAN=%d BGCOLOR=#FFFFFF><CENTER>&nbsp;</CENTER></TD>";
char*         JulianFormFactory::FBT_DayXColBusyCell_HTML	= "<TD ALIGN=CENTER COLSPAN=%d BGCOLOR=#C0C0C0><CENTER>&nbsp;</CENTER></TD>";
char*         JulianFormFactory::FBT_DayXColEmptyCell_HTML	= "<TD ALIGN=CENTER COLSPAN=%d BGCOLOR=#666666><CENTER>&nbsp;</CENTER></TD>";
char*         JulianFormFactory::FBT_DayXXParamCell_HTML	= "<TD ALIGN=CENTER COLSPAN=%d BGCOLOR=#C00000><CENTER>&nbsp;</CENTER></TD>";
char*         JulianFormFactory::FBT_EmptyRow_HTML			= "<TR><TD COLSPAN=31 BGCOLOR=#666666>&nbsp;</TD></TR>";

// Legend
char*         JulianFormFactory::FBT_Legend_Start_HTML		= "<TABLE BORDER=1 WIDTH=100><TR><TD WIDTH=50%>";
char*         JulianFormFactory::FBT_Legend_Text1_HTML		= "<CENTER>";
char*         JulianFormFactory::FBT_Legend_Text2_HTML		= "<TD WIDTH=30% BGCOLOR=#FFFFFF><CENTER>";
char*         JulianFormFactory::FBT_Legend_Text3_HTML		= "<TD WIDTH=30% BGCOLOR=#C0C0C0><CENTER>";
char*         JulianFormFactory::FBT_Legend_Text4_HTML		= "<TD WIDTH=30% BGCOLOR=#666666><CENTER>";
char*         JulianFormFactory::FBT_Legend_Text5_HTML		= "<TD WIDTH=30% BGCOLOR=#CC0000><CENTER>";
char*         JulianFormFactory::FBT_Legend_TextEnd_HTML	= "&nbsp;</CENTER></TD>";
char*         JulianFormFactory::FBT_Legend_End_HTML		= "</TR></TABLE>";

char*         JulianFormFactory::FBT_TickMark1				= "00";
char*         JulianFormFactory::FBT_TickMark2				= "15";
char*         JulianFormFactory::FBT_TickMark3				= "30";
char*         JulianFormFactory::FBT_TickMark4				= "45";
char*         JulianFormFactory::FBT_TickSetting			= "PT00H30M";
char*         JulianFormFactory::FBT_TickDaySetting			= "P1D";
char*         JulianFormFactory::FBT_TickOffset				= "PT00H01M";
char*         JulianFormFactory::FBT_DayFormat				= "EEE, MMM dd";
char*         JulianFormFactory::FBT_MonthFormat		    = "MMM yyyy";

/* 
** Fonts
*/
char*         JulianFormFactory::Start_Font					= "<FONT SIZE=-1>";
char*         JulianFormFactory::End_Font					= "</FONT>";
char*         JulianFormFactory::Start_BIG_Font				= "<FONT SIZE=2>";

/*
** Buttons
*/
char*         JulianFormFactory::Buttons_Single_Start_HTML	= "<INPUT TYPE=SUBMIT NAME=\"";
char*         JulianFormFactory::Buttons_Single_Mid_HTML	= "\" VALUE=\"";
char*         JulianFormFactory::Buttons_Single_End_HTML	= "\">";
char*         JulianFormFactory::Buttons_Text_End_HTML		= "\">&nbsp;<INPUT TYPE=TEXT NAME=\"%s\" SIZE=29 MAXLENGTH=100>";

JulianString  JulianFormFactory::Buttons_Details_Type		= "btnNotifySubmitMD";
JulianString  JulianFormFactory::Buttons_Add_Type			= "btnNotifySubmitATS";
JulianString  JulianFormFactory::Buttons_Close_Type			= "btnNotifySubmitC";
JulianString  JulianFormFactory::Buttons_Accept_Type		= "btnNotifySubmitAccept";
JulianString  JulianFormFactory::Buttons_Decline_Type		= "btnNotifySubmitDecline";
JulianString  JulianFormFactory::Buttons_Tentative_Type		= "btnNotifySubmitTent";
JulianString  JulianFormFactory::Buttons_SendFB_Type		= "btnNotifySubmitFB";
JulianString  JulianFormFactory::Buttons_SendRefresh_Type	= "btnNotifySubmitERR";
JulianString  JulianFormFactory::Buttons_DelTo_Type			= "btnNotifySubmitDelTo";

char*         JulianFormFactory::Buttons_SaveDel_HTML		= "<INPUT type=checkbox name=btnSaveRefCopy> Save copy of delegated event";

char*         JulianFormFactory::Buttons_GroupStart_HTML	= "<TR><TD VALIGN=CENTER COLSPAN=2 NOWRAP>";
char*         JulianFormFactory::Buttons_GroupEnd_HTML		= "</TD></TR>";
char*         JulianFormFactory::Buttons_GroupSingleStart_HTML= "<TR><TD WIDTH=110>&nbsp;\n</TD><TD>";
char*         JulianFormFactory::Buttons_GroupSingleEnd_HTML= "</TD></TR>";

/*
** Text Fields
*/
char*         JulianFormFactory::Text_Label_Start_HTML		= "<TR VALIGN=TOP><TD VALIGN=CENTER COLSPAN=2>";
JulianString  JulianFormFactory::Text_Label					= "Comments (optional):&nbsp;";
char*         JulianFormFactory::Text_Label_End_HTML		= "</TD></TR>";
JulianString  JulianFormFactory::CommentsFieldName			= "txtNotifyComments";
JulianString  JulianFormFactory::DelToFieldName				= "btnNotifySubmitDelTo";
char*         JulianFormFactory::Text_Field_HTML			= "<TR><TD VALIGN=TOP COLSPAN=2><TEXTAREA NAME=txtNotifyComments ROWS=5 COLS=48 wrap=on></TEXTAREA></TD>";

/*
** ICalPropDefs
*/
JulianString  JulianFormFactory::ICalPreProcess				= "%B%v%J";
JulianString  JulianFormFactory::ICalPostProcess			= "";

/*
** Error
*/
int32		  JulianFormFactory::error_Fields_Labels_Length = 2;
JulianString  JulianFormFactory::error_Fields_Labels[]	    = { "What:", "When:"};
int32		  JulianFormFactory::error_Fields_Data_HTML_Length = 3;
JulianString  JulianFormFactory::error_Fields_Data_HTML[]	= { "%S", "%B", "%i" };
char*         JulianFormFactory::error_End_HTML			    = "";

/*
** Publish
*/
int32		  JulianFormFactory::publish_Fields_Labels_Length = 3;
JulianString  JulianFormFactory::publish_Fields_Labels[]	= { "What:", "When:", "Location:"};
int32		  JulianFormFactory::publish_Fields_Data_HTML_Length = 4;
JulianString  JulianFormFactory::publish_Fields_Data_HTML[]	= { "%S", "%B", "%L", "%i" };
char*         JulianFormFactory::publish_End_HTML			= "";

/*
** Publish Detail
*/
int32		  JulianFormFactory::publish_D_Fields_Labels_Length	= 14;
JulianString  JulianFormFactory::publish_D_Fields_Labels[]		= { "What:", "When:", "Location:", "Organizer:", "Status:", "Priority:", "Categories:", "Resources:", "Attachments:", "Alarms:", "Created:", "Last Modified:", "Sent:", "UID:" };
int32		  JulianFormFactory::publish_D_Fields_Data_HTML_Length = 15;
JulianString  JulianFormFactory::publish_D_Fields_Data_HTML[]	= { "%S", "%B", "%L", "%J", "%g", "%p", "%k", "%r", "%a", "%w", "%t", "%M", "%C", "%U", "%i" };

/*
** Publish VFreeBusy
*/
int32		  JulianFormFactory::publishFB_Fields_Labels_Length	= 2;
JulianString  JulianFormFactory::publishFB_Fields_Labels[]		= { "What:", "When:"};
int32		  JulianFormFactory::publishFB_Fields_Data_HTML_Length = 2;
JulianString  JulianFormFactory::publishFB_Fields_Data_HTML[]	= { "%S", "%B" };
char*         JulianFormFactory::publishFB_End_HTML				= "";

/*
** Publish VFreeBusy Detail
*/
int32		  JulianFormFactory::publishFB_D_Fields_Labels_Length	= 4;
JulianString  JulianFormFactory::publishFB_D_Fields_Labels[]		= { "What:", "When:",  "Created:", "Sent:"};
int32		  JulianFormFactory::publishFB_D_Fields_Data_HTML_Length = 4;
JulianString  JulianFormFactory::publishFB_D_Fields_Data_HTML[]	= { "%S", "%B", "%t", "%C" };

/*
** Reply VFreeBusy
*/

/*
** Request VEvent
*/
int32		  JulianFormFactory::request_Fields_Labels_Length = 5;
JulianString  JulianFormFactory::request_Fields_Labels[]		= { "What:", "When:", "Location:", "Organizer:", "Who:"};
int32		  JulianFormFactory::request_Fields_Data_HTML_Length = 6;
JulianString  JulianFormFactory::request_Fields_Data_HTML[]	= { "%S", "%B", "%L", "%J", "%v", "%i" };
char*         JulianFormFactory::request_End_HTML			= "";

/*
** Request Detail VEvent
*/
int32		  JulianFormFactory::request_D_Fields_Labels_Length = 15;
JulianString  JulianFormFactory::request_D_Fields_Labels[]	= { "What:", "When:", "Location:", "Organizer:", "Who:", "Status:", "Priority:", "Categories:", "Resources:", "Attachments:", "Alarms:", "Created:", "Last Modified:", "Sent:", "UID:"};
int32		  JulianFormFactory::request_D_Fields_Data_HTML_Length = 16;
JulianString  JulianFormFactory::request_D_Fields_Data_HTML[]= { "%S", "%B", "%L", "%J", "%v", "%g", "%p", "%k", "%r", "%a", "%w", "%t", "%M", "%C", "%U", "%i" };

/*
** Request VFreeBusy
*/
int32         JulianFormFactory::request_FB_Fields_Labels_Length = 1;
JulianString  JulianFormFactory::request_FB_Fields_Labels[]		= { "When:"};
int32		  JulianFormFactory::request_FB_Fields_Data_HTML_Length = 1;
JulianString  JulianFormFactory::request_FB_Fields_Data_HTML[]	= { "%B" };
char*         JulianFormFactory::request_FB_End_HTML				= "";

/*
** Request VFreeBusy Detail
*/
int32		  JulianFormFactory::request_D_FB_Fields_Labels_Length = 1;
JulianString  JulianFormFactory::request_D_FB_Fields_Labels[]	= { "When:"};
int32		  JulianFormFactory::request_D_FB_Fields_Data_HTML_Length = 1;
JulianString  JulianFormFactory::request_D_FB_Fields_Data_HTML[]	= { "%B" };
char*         JulianFormFactory::request_D_FB_End_HTML			= "";

/*
** Event Reply
*/
int32		  JulianFormFactory::eventreply_Fields_Labels_Length = 2;
JulianString  JulianFormFactory::eventreply_Fields_Labels[]		= { "Who:", "Status:"};
int32		  JulianFormFactory::eventreply_Fields_Data_HTML_Length = 4;
JulianString  JulianFormFactory::eventreply_Fields_Data_HTML[]	= { "%v", "%g", "%K", "%i" };
char*         JulianFormFactory::eventreply_End_HTML				= "";

/*
** Event Reply Detail
*/
int32		  JulianFormFactory::eventreply_D_Fields_Labels_Length= 5;
JulianString  JulianFormFactory::eventreply_D_Fields_Labels[]	= { "What:", "When:", "Location:", "Who:", "Status:" };
int32		  JulianFormFactory::eventreply_D_Fields_Data_HTML_Length = 6;
JulianString  JulianFormFactory::eventreply_D_Fields_Data_HTML[]	= { "%S", "%B", "%L", "%v", "%g", "%K", "%i" };

/*
** Event Cancel
*/
int32		  JulianFormFactory::eventcancel_Fields_Labels_Length = 2;
JulianString  JulianFormFactory::eventcancel_Fields_Labels[]		= { "Status:", "UID:"};
int32		  JulianFormFactory::eventcancel_Fields_Data_HTML_Length = 3;
JulianString  JulianFormFactory::eventcancel_Fields_Data_HTML[]	= { "%g", "%U", "%K" };
char*         JulianFormFactory::eventcancel_End_HTML			= "";

/*
** Event Cancel Detail
*/
int32		  JulianFormFactory::eventcancel_D_Fields_Labels_Length= 14;
JulianString  JulianFormFactory::eventcancel_D_Fields_Labels[]	= { "What:", "When:", "Location:", "Organizer:", "Status:", "Priority:", "Categories:", "Resources:", "Attachments:", "Alarms:", "Created:", "Last Modified:", "Sent:", "UID:" };
int32		  JulianFormFactory::eventcancel_D_Fields_Data_HTML_Length = 16;
JulianString  JulianFormFactory::eventcancel_D_Fields_Data_HTML[]	= { "%S", "%B", "%L", "%J", "%g", "%p", "%k", "%r", "%a", "%w", "%t", "%M", "%C", "%U", "%K", "%i" };

/*
** Event Refresh Request
*/
int32		  JulianFormFactory::eventrefreshreg_Fields_Labels_Length = 2;
JulianString  JulianFormFactory::eventrefreshreg_Fields_Labels[]		= { "Who:", "UID:"};
int32		  JulianFormFactory::eventrefreshreg_Fields_Data_HTML_Length = 2;
JulianString  JulianFormFactory::eventrefreshreg_Fields_Data_HTML[]	= { "%v", "%U" };
char*         JulianFormFactory::eventrefreshreg_End_HTML				= "";

/*
** Event Refresh Request Detail
*/
int32		  JulianFormFactory::eventrefreshreg_D_Fields_Labels_Length= 14;
JulianString  JulianFormFactory::eventrefreshreg_D_Fields_Labels[]	= { "What:", "When:", "Location:", "Organizer:", "Status:", "Priority:", "Categories:", "Resources:", "Attachments:", "Alarms:", "Created:", "Last Modified:", "Sent:", "UID:" };
int32		  JulianFormFactory::eventrefreshreg_D_Fields_Data_HTML_Length = 15;
JulianString  JulianFormFactory::eventrefreshreg_D_Fields_Data_HTML[]= { "%S", "%B", "%L", "%J", "%g", "%p", "%k", "%r", "%a", "%w", "%t", "%M", "%C", "%U", "%i" };

/*
** Event Counter Proposal
*/

/*
** Event Deline Counter
*/
int32		  JulianFormFactory::eventdelinecounter_Fields_Labels_Length= 1;
JulianString  JulianFormFactory::eventdelinecounter_Fields_Labels[]	= { "What:" };
int32		  JulianFormFactory::eventdelinecounter_Fields_Data_HTML_Length = 3;
JulianString  JulianFormFactory::eventdelinecounter_Fields_Data_HTML[]= { "%S", "%K", "%i" };

/*
***
*** String to translate
***
*/
JulianString  JulianFormFactory::SubjectSep;

#ifndef LIBJULIAN_USE_XPSTRS
void
JulianFormFactory::init()
{
	do_capi_login = FALSE; /* Default is not to ask for capi login info */

	if (jf->getCallbacks()->BoolPref)
		(*jf->getCallbacks()->BoolPref)("calendar.capi.enabled", &do_capi_login);

    error_Header_HTML		        = "Error";
    publish_Header_HTML		        = "Published Event";
    publishFB_Header_HTML		    = "Published Free/Busy";
    replyFB_Header_HTML			    = "Reply Free/Busy";
    request_Header_HTML			    = "Event Request";
    request_FB_Header_HTML			= "Free/Busy Time Request";
    request_D_FB_Header_HTML	    = "Free/Busy Time Request";
    eventreply_Header_HTML		    = "Event Reply";
    eventcancel_Header_HTML			= "Event Cancellation";
    eventrefreshreg_Header_HTML		= "Event Refresh Request";
    eventcounterprop_Header_HTML	= "Event Counter Proposal";
    eventdelinecounter_Header_HTML	= "Decline Counter Proposal";

    String_What		                = "What:";
    String_When		                = "When:";
    String_Location		            = "Location:";
    String_Organizer		        = "Organizer:";
    String_Status		            = "Status:";
    String_Priority		            = "Priority:";
    String_Categories		        = "Categories:";
    String_Resources		        = "Resources:";
    String_Attachments		        = "Attachments:";
    String_Alarms		            = "Alarms:";
    String_Created		            = "Created:";
    String_Last_Modified	        = "Last Modified:";
    String_Sent	    	            = "Sent:";
    String_UID		                = "UID:";

    FBT_Legend_Title			    = "Legend:";
    FBT_Legend_Free		        	= "free";
    FBT_Legend_Busy		        	= "busy";
    FBT_Legend_Unknown			    = "unknown";
    FBT_Legend_xparam			    = "undefined";

    FBT_AM						    = " AM";
    FBT_PM						    = " PM";

    Buttons_Details_Label		    = "More Details...";
    Buttons_Add_Label			    = "Add To Schedule";
    Buttons_Close_Label		        = "Close";
    Buttons_Accept_Label		    = "Accept";
    Buttons_AcceptAll_Label         = "Accept All";
    Buttons_Update_Label		    = "Update Schedule";
    Buttons_Decline_Label		    = "Decline";
    Buttons_Tentative_Label	        = "Tentative";
    Buttons_SendFB_Label		    = "Send Free/Busy Time Infomation";
    Buttons_SendRefresh_Label	    = "Send Refresh";
    Buttons_DelTo_Label		        = "Delegate to";

/*
** Status Messages
*/
    EventInSchedule			        = "This event is already in your schedule\r\n";
    EventNotInSchedule			    = "This event is not yet in your schedule\r\n";
    EventConflict				    = "Conflicts:";
    EventNote					    = "Note:";
    EventError					    = "<FONT COLOR=#FF0000>Error:</FONT>";
    EventTest					    = "Staff meeting 3:30 - 4:30 PM :-(";
    Text_To					        = " to ";
    Text_AllDay				        = " ( Day Event) ";
    Text_StartOn				    = " Begins on ";
    Text_Was					    = "Was";

/*
** Muilt Stuff
*/
    MuiltEvent_Header_HTML	        = "Published Calendar Events";
    MuiltFB_Header_HTML	            = "Published Calendar Free/Busy";
    MuiltEvent				        = "This messages contains %d events.";
    WhenStr				            = "When";
    WhatStr				            = "What";
    SubjectSep				        = ": ";

/*
** Error Strings
*/
    MoreErrors                      = "There are also %d other errors";
    TooManyEvents                   = "Display limited to the first %d events.";
    error0                          = "Unknown";
    error1                          = "DTEnd before DTStart.  Setting DTEnd equal to DTStart";
    error2                          = "Prompt value must be ON or OFF";
    error3                          = "Cannot parse time/date string";
    error4                          = "Recurrence rules are too complicated.  Only the first instance was scheduled";
    error5                          = "Invalid property value";
    error6                          = "Invalid property name";
    error7                          = "Invalid parameter name";

/* Don't forget What, When etc */
}
#else
void
JulianFormFactory::init()
{
	do_capi_login = FALSE; /* Default is not to ask for capi login info */

	if (jf->getCallbacks()->BoolPref)
		(*jf->getCallbacks()->BoolPref)("calendar.capi.enabled", &do_capi_login);

    /*
    ** char*'s
    */
    if (cb && cb->GetString)
    {
        error_Header_HTML = XP_STRDUP((*cb->GetString)(JULIAN_STRING_ERROR_HEADER));		            
        publish_Header_HTML = XP_STRDUP((*cb->GetString)(JULIAN_STRING_PUBLISH_HEADER));		        
        publishFB_Header_HTML = XP_STRDUP((*cb->GetString)(JULIAN_STRING_PUBLISHFB_HEADER));		    	
        replyFB_Header_HTML = XP_STRDUP((*cb->GetString)(JULIAN_STRING_REPLYFB_HEADER));			    
        request_Header_HTML = XP_STRDUP((*cb->GetString)(JULIAN_STRING_REQUEST_HEADER));			    
        request_FB_Header_HTML = XP_STRDUP((*cb->GetString)(JULIAN_STRING_REQUESTFB_HEADER));			    
        request_D_FB_Header_HTML = XP_STRDUP((*cb->GetString)(JULIAN_STRING_REQUESTFB_HEADER));		    
        eventreply_Header_HTML = XP_STRDUP((*cb->GetString)(JULIAN_STRING_EVENTREPLY_HEADER));	    	
        eventcancel_Header_HTML = XP_STRDUP((*cb->GetString)(JULIAN_STRING_EVENTCANCEL_HEADER));			
        eventrefreshreg_Header_HTML = XP_STRDUP((*cb->GetString)(JULIAN_STRING_EVENTREFRESHREG_HEADER));		
        eventcounterprop_Header_HTML = XP_STRDUP((*cb->GetString)(JULIAN_STRING_EVENTCOUNTERPROP_HEADER));		
        eventdelinecounter_Header_HTML = XP_STRDUP((*cb->GetString)(JULIAN_STRING_EVENTDELINECOUNTER_HEADER));	

        String_What = XP_STRDUP((*cb->GetString)(JULIAN_STRING_WHAT));		                
        String_When = XP_STRDUP((*cb->GetString)(JULIAN_STRING_WHEN));		                
        String_Location = XP_STRDUP((*cb->GetString)(JULIAN_STRING_LOCATION));		            
        String_Organizer = XP_STRDUP((*cb->GetString)(JULIAN_STRING_ORGANIZER));		            
        String_Status = XP_STRDUP((*cb->GetString)(JULIAN_STRING_STATUS));		                
        String_Priority = XP_STRDUP((*cb->GetString)(JULIAN_STRING_PRIORITY));		            
        String_Categories = XP_STRDUP((*cb->GetString)(JULIAN_STRING_CATEGORIES));		            
        String_Resources = XP_STRDUP((*cb->GetString)(JULIAN_STRING_RESOURCES));		            
        String_Attachments = XP_STRDUP((*cb->GetString)(JULIAN_STRING_ATTACHMENTS));	            
        String_Alarms = XP_STRDUP((*cb->GetString)(JULIAN_STRING_ALARMS));		                
        String_Created = XP_STRDUP((*cb->GetString)(JULIAN_STRING_CREATED));	                
        String_Last_Modified = XP_STRDUP((*cb->GetString)(JULIAN_STRING_LAST_MODIFIED));	            
        String_Sent = XP_STRDUP((*cb->GetString)(JULIAN_STRING_SENT));	    	            
        String_UID = XP_STRDUP((*cb->GetString)(JULIAN_STRING_UID));	                    
    }

    /*
    ** JulianStrings
    */
    FBT_Legend_Title.LoadString(JULIAN_FBT_LEGEND_TITLE);		        
    FBT_Legend_Free.LoadString(JULIAN_FBT_LEGEND_FREE);		        	
    FBT_Legend_Busy.LoadString(JULIAN_FBT_LEGEND_BUSY);		        	
    FBT_Legend_Unknown.LoadString(JULIAN_FBT_LEGEND_UNKNOWN);		        
    FBT_Legend_xparam.LoadString(JULIAN_FBT_LEGEND_XPARAM);			        

    FBT_AM.LoadString(JULIAN_FBT_AM);					        
    FBT_PM.LoadString(JULIAN_FBT_PM);					        

    Buttons_Details_Label.LoadString(JULIAN_BUTTONS_DETAILS_LABEL);		        
    Buttons_Add_Label.LoadString(JULIAN_BUTTONS_ADD_LABEL);			        
    Buttons_Close_Label.LoadString(JULIAN_BUTTONS_CLOSE_LABEL);		        
    Buttons_Accept_Label.LoadString(JULIAN_BUTTONS_ACCEPT_LABEL);		        
    Buttons_AcceptAll_Label.LoadString(JULIAN_BUTTONS_ACCEPTALL_LABEL);           
    Buttons_Update_Label.LoadString(JULIAN_BUTTONS_UPDATE_LABEL);		        
    Buttons_Decline_Label.LoadString(JULIAN_BUTTONS_DECLINE_LABEL);		        
    Buttons_Tentative_Label.LoadString(JULIAN_BUTTONS_TENTATIVE_LABEL);	        
    Buttons_SendFB_Label.LoadString(JULIAN_BUTTONS_SENDFB_LABEL);		        
    Buttons_SendRefresh_Label.LoadString(JULIAN_BUTTONS_SEND_REFRESH_LABEL);	        
    Buttons_DelTo_Label.LoadString(JULIAN_BUTTONS_DELTO_LABEL);		        

    /*
    ** Status Messages
    */
    EventInSchedule.LoadString(JULIAN_EVENT_IN_SCHEDULE);			        
    EventNotInSchedule.LoadString(JULIAN_EVENT_NOT_IN_SCHEDULE);			        
    EventConflict.LoadString(JULIAN_EVENT_CONFLICT);				        
    EventNote.LoadString(JULIAN_NOTE);					        
    EventError.LoadString(JULIAN_EVENT_ERROR);					        
    EventTest.LoadString(JULIAN_EVENT_ERROR);					        
    Text_To.LoadString(JULIAN_TEXT_TO);					        
    Text_AllDay.LoadString(JULIAN_TEXT_ALL_DAY);				        
    Text_StartOn.LoadString(JULIAN_TEXT_START_ON);				        
    Text_Was.LoadString(JULIAN_TEXT_WAS);					        

    /*
    ** Muilt Stuff
    */
    MuiltEvent_Header_HTML.LoadString(JULIAN_MUILTEVENT_HEADER);	            
    MuiltFB_Header_HTML.LoadString(JULIAN_MUILTFB_HEADER);	            
    MuiltEvent.LoadString(JULIAN_MUILTEVENT);				            
    WhenStr.LoadString(JULIAN_WHENSTR);				            
    WhatStr.LoadString(JULIAN_WHATSTR);				            
    SubjectSep.LoadString(JULIAN_SUBJECTSEP);			            

    /*
    ** Error Strings
    */
    MoreErrors.LoadString(JULIAN_MORE_ERRORS);                         
    TooManyEvents.LoadString(JULIAN_TOO_MANY_EVENTS);                     
    error0.LoadString(JULIAN_ERROR_O);                             
    error1.LoadString(JULIAN_ERROR_1);                             
    error2.LoadString(JULIAN_ERROR_2);                             
    error3.LoadString(JULIAN_ERROR_3);                            
    error4.LoadString(JULIAN_ERROR_4);
    error5.LoadString(JULIAN_ERROR_5);                             
    error6.LoadString(JULIAN_ERROR_6);                             
    error7.LoadString(JULIAN_ERROR_7);         
}
#endif
