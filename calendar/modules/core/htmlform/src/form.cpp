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

#include <calendar.h>
#include <gregocal.h>
#include <datefmt.h>
#include <time.h>
#include <unistring.h>
#include "smpdtfmt.h"
#include <simpletz.h>
#include "datetime.h"
#include "jutility.h"
#include "duration.h"

#include "attendee.h"
#include "vevent.h"
#include "icalsrdr.h"
#include "icalfrdr.h"
#include "prprty.h"
#include "icalprm.h"
#include "freebusy.h"
#include "vfrbsy.h"
#include "nscal.h"
#include "keyword.h"

#include "txnobjfy.h"
#include "user.h"
#include "txnobj.h"

#include "jlog.h"
#include "uri.h"

#include "xp.h"
#include "prmon.h"

#include "form.h"

static void julian_handle_close(JulianForm *jf);
#ifdef OSF1
       void julian_handle_accept(JulianForm *jf, int newStatus);
#else
       void julian_handle_accept(JulianForm *jf, Attendee::STATUS newStatus);
#endif
static void julian_handle_moredetail(JulianForm *jf);
static void julian_send_response(JulianString& subject, JulianPtrArray& recipients, JulianString& LoginName, JulianForm& jf, NSCalendar& calendar);
static void julian_send_response_with_events(JulianString& subject, JulianPtrArray& recipients, JulianString& LoginName, JulianForm& jf, NSCalendar& calendar, JulianPtrArray * events);
static void julian_add_new_event_to_send(JulianPtrArray * vvEventsToSend, TimeBasedEvent * eventToSend);
static XP_Bool julian_events_comments_and_attendees_match(TimeBasedEvent * a, TimeBasedEvent * b);
static void julian_fill_in_delegatedToVector(char * deltonames, JulianPtrArray * vDelegatedToFillIn);

static void Julian_ClearLoginInfo();
static int Julian_GetLoginInfo(JulianForm& jf, MWContext* context, char** url, char** password);

#define julian_pref_name "calendar.login_url"

XP_Bool JulianForm::ms_bFoundNLSDataDirectory = FALSE;

JulianForm *jform_CreateNewForm(char *calendar_mime_data, pJulian_Form_Callback_Struct callbacks,
                                XP_Bool bFoundNLSDataDirectory)
{
	JulianForm	*jf = new JulianForm();

	if (jf)
	{
        jf->refcount = 1;
		jf->setMimeData(calendar_mime_data);
		jf->setCallbacks(callbacks);
        JulianForm::setFoundNLSDataDirectory(bFoundNLSDataDirectory);
	}
	return jf;
}

void jform_DeleteForm(JulianForm *jf)
{
    if (jf)
    {
        jf->refcount--;
        if (jf->refcount == 0)
        {
            delete jf;
        }
    }
}

char* jform_GetForm(JulianForm *jf)
{
    jf->StartHTML();
	return jf->getHTMLForm(FALSE);
}

void jform_CallBack(JulianForm *jf, char *type)
{
	char*			button_type;
	char*			button_type2;
	char*			button_data;
	form_data_combo	fdc;
	int32			x;

    PR_EnterMonitor(jf->my_monitor);
    /*
	** type contains the html form string for this.
	** The gernal format is ? type = label or name = data
	** The last thing in this list is the button that started
	** this.
	*/
	button_type = XP_STRCHR(type, '?');
	button_type++;	// Skip ?. Now points to type

	while (TRUE)
	{
		button_type2 = XP_STRCHR(button_type, '=');
		if (button_type2)
		{
			*button_type2++ = '\0';
			button_data = button_type2; // Points to data part
			button_type2 = XP_STRCHR(button_type2, '&');
		} else {
			button_data = nil;
			button_type2 = nil;
		}
		if (button_type2)
		{
			*button_type2++ = '\0';
		}
		fdc.type = button_type;
		fdc.data = button_data;
		jf->formData[jf->formDataCount] = fdc;
		jf->formDataCount++;
		if (!button_type2 || (jf->formDataCount > formDataIndex))
			break;
		else
			button_type = button_type2;

	}

	for (x=0; x < jf->formDataCount; x++)
	{
		button_type = jf->formData[x].type;
		jf->setLabel( button_data = jf->formData[x].data );

		if (!JulianFormFactory::Buttons_Details_Type.CompareTo(button_type)) 
		{
			julian_handle_moredetail(jf);
		} else
		if (!JulianFormFactory::Buttons_Accept_Type.CompareTo(button_type))
		{
			julian_handle_accept(jf, Attendee::STATUS_ACCEPTED);
		} else
		if (!JulianFormFactory::Buttons_Add_Type.CompareTo(button_type))
		{
			julian_handle_accept(jf, Attendee::STATUS_ACCEPTED);
		} else
		if (!JulianFormFactory::Buttons_Close_Type.CompareTo(button_type))
		{
			julian_handle_close(jf);
		} else
		if (!JulianFormFactory::Buttons_Decline_Type.CompareTo(button_type))
		{
			julian_handle_accept(jf, Attendee::STATUS_DECLINED);
		} else
		if (!JulianFormFactory::Buttons_Tentative_Type.CompareTo(button_type))
		{
			julian_handle_accept(jf, Attendee::STATUS_TENTATIVE);
		} else
		if (!JulianFormFactory::Buttons_SendFB_Type.CompareTo(button_type))
		{
		} else
		if (!JulianFormFactory::Buttons_SendRefresh_Type.CompareTo(button_type))
		{
		} else
		if (!JulianFormFactory::Buttons_DelTo_Type.CompareTo(button_type))
		{
			julian_handle_accept(jf, Attendee::STATUS_DELEGATED);
		}
	}

    PR_ExitMonitor(jf->my_monitor);
}

/**
*** JulianForm Class
***
*** This is the c++ interface to JulianFormFactory
***
**/

JulianForm::JulianForm()
{
	mimedata = nil;
	imipCal = nil;
    jff = nil;
	contextName = "Julian:More Details";
	formDataCount = 0;
    my_monitor = PR_NewMonitor();
}

JulianForm::~JulianForm()
{
    if (FALSE && imipCal)
    {
        if (imipCal->getLog())
            delete imipCal->getLog();
        delete imipCal;
		imipCal = nil;
    }

    if (jff)
    {
        delete jff;
    }

    if (my_monitor) PR_DestroyMonitor(my_monitor);
}

XP_Bool
JulianForm::StartHTML()
{
    JulianString		u;
    UnicodeString ust;
	if (imipCal == NULL)
	{
		ICalReader	*tfr = (ICalReader *) new ICalStringReader(mimedata);
        mimedata = nil;
		if (tfr)
		{
            JLog * log = new JLog();
            if (!ms_bFoundNLSDataDirectory)
            {
                if (log != 0)
                {
                    // TODO: finish
                    //log->log("ERROR: Can't find REQUIRED NLS Data Directory\n");
                }
            }
			imipCal = new NSCalendar(log);
            ust = u.GetBuffer();
			if (imipCal) imipCal->parse(tfr, ust);
			delete tfr;
		}

        return TRUE;
	}

    return FALSE;
}

char*
JulianForm::getHTMLForm(XP_Bool Want_Detail, NET_StreamClass *this_stream)
{
	if (imipCal != nil)
	{
		if ((jff = new JulianFormFactory(*imipCal, *this, getCallbacks())) != nil)
		{
            htmlForm = "";	// Empty it

            jff->init();
		    jff->getHTML(&htmlForm, this_stream, Want_Detail);
		}
	}

    char* t2 = (char*) XP_ALLOC(htmlForm.GetStrlen() + 1);
    if (t2) strcpy(t2, htmlForm.GetBuffer());
	return t2;
}

void
JulianForm::setMimeData(char *NewMimeData)
{
	if (NewMimeData)
	{
		mimedata = NewMimeData;
	}
}

char*
JulianForm::getComment()
{
	for (int32 x=0; x < formDataCount; x++)
	{
		if (!JulianFormFactory::CommentsFieldName.CompareTo(formData[x].type))
		{
			(*getCallbacks()->PlusToSpace)(formData[x].data);
			(*getCallbacks()->UnEscape)	 (formData[x].data);
			return formData[x].data;
		}
	}

	return nil;
}

char*
JulianForm::getDelTo()
{
	for (int32 x=0; x < formDataCount; x++)
	{
        char* temp;

        temp = PR_smprintf( "%s", formData[x].type); // Where is a pr_strcpy??
        if (temp)
        {
            (*getCallbacks()->PlusToSpace)(temp);
		    if (!XP_STRCMP(jff->Buttons_DelTo_Label.GetBuffer(), temp))
		    {
			    (*getCallbacks()->PlusToSpace)(formData[x].data);
			    (*getCallbacks()->UnEscape)	 (formData[x].data);
			    return formData[x].data;
		    }
            PR_DELETE(temp);
        }
	}

	return nil;
}

void
julian_handle_close(JulianForm *jf)
{
	MWContext*	cx = nil;

    if ((cx = jf->getContext()) != nil)
	{
		(*jf->getCallbacks()->DestroyWindow)(cx);
	}
}

void
julian_handle_moredetail(JulianForm *jf)
{
	MWContext*	        cx = nil;
    char*		        newhtml = NULL;
	NET_StreamClass*    stream = nil;
    URL_Struct*         url = nil;

    if (jf->getCallbacks() == nil ||
        jf->getCallbacks()->CreateURLStruct == nil)
        return;

	url = (*jf->getCallbacks()->CreateURLStruct)("internal_url://", NET_RESIZE_RELOAD);
    if (url)
    {
        url->internal_url = TRUE;
	    (*jf->getCallbacks()->SACopy)(&(url->content_type), TEXT_HTML);

	    // Look to see if we already made a window for this
	    if ((cx = jf->getContext()) != nil)
	    {
		    (*jf->getCallbacks()->RaiseWindow)(cx);
	    }

        //
        // If the more details window isn't there,
        // make one
        //
	    if (!cx)
	    {
            Chrome* customChrome = XP_NEW_ZAP(Chrome);

		    /* make the window */
            if (customChrome)
            {
                customChrome->show_scrollbar = TRUE;		 /* TRUE to show scrollbars on window */
                customChrome->allow_resize = TRUE;			 /* TRUE to allow resize of windows */
                customChrome->allow_close = TRUE;			 /* TRUE to allow window to be closed */
                customChrome->disable_commands = TRUE;		 /* TRUE if user has set hot-keys / menus off */
                customChrome->restricted_target = TRUE;		 /* TRUE if window is off-limits for opening links into */
            }

		    cx = (*jf->getCallbacks()->MakeNewWindow)((*jf->getCallbacks()->FindSomeContext)(), nil, jf->contextName, customChrome);
	    }

	    if (cx)
	    {
	        static PA_InitData data;

		    /* make a netlib stream to display in the window */
	        data.output_func = jf->getCallbacks()->ProcessTag;
		    stream = (*jf->getCallbacks()->BeginParseMDL)(FO_CACHE_AND_VIEW_SOURCE | FO_CACHE_AND_PRESENT_INLINE, &data, url, cx);
            if (stream)
            {
                jf->StartHTML();
                jf->getHTMLForm(TRUE, stream);
			    (*stream->complete) (stream->data_object);
                XP_FREE(stream);
            }
        } 
    }
}

void
julian_send_response_with_events(JulianString& subject, JulianPtrArray& recipients, JulianString& LoginName, JulianForm& jf, NSCalendar& calendar, JulianPtrArray * events)
{
	TransactionObject::ETxnErrorCode txnStatus;
	TransactionObject*	txnObj;
	JulianPtrArray*		capiModifiers = 0;

    if (jf.getCallbacks() == nil ||
        jf.getCallbacks()->FindSomeContext == nil)
        return;

	MWContext* this_context = (*jf.getCallbacks()->FindSomeContext)();

	capiModifiers = new JulianPtrArray();

	if (capiModifiers)
	{
        UnicodeString usMeUri = LoginName.GetBuffer();
		URI meUri(usMeUri);
		User* uFrom = new User(usMeUri, meUri.getName());
        char *capurl = NULL, *passwd = NULL;
        UnicodeString uSubject;
        UnicodeString uLoginName;
		XP_Bool do_capi_login = FALSE; /* Default is not to ask for capi login info */

		if (jf.getCallbacks()->BoolPref)
			(*jf.getCallbacks()->BoolPref)("calendar.capi.enabled", &do_capi_login);

        if (do_capi_login &&
        	Julian_GetLoginInfo(jf, this_context, &capurl, &passwd) > 0)
        {
            char*   calUser = "";
            char*   calHost = "";
            char*   calNode = "10000";
            char*   temp;

            // Skip pass "capi://", if it is there
            if (XP_STRSTR(capurl, "://"))
            {
               capurl = XP_STRSTR(capurl, "://");
               capurl += 3;
            }

            // Break apart the user and host:node parts
            temp = XP_STRSTR(capurl, "/");
            if (temp)
            {
                calUser = temp;
                *(calUser) = '\0';
                calUser++;
                calHost = capurl;
            }

            // Break apart the host and node parts
            temp = XP_STRSTR(calHost, ":");
            if (temp)
            {
                calNode = temp;
                *(calNode++) = '\0';
            }

            // Currently the URL login form is as follows:
            // capi://host:node/login
            // i.e. 
            // capi://calendar-1.mcom.com:10000/John Sun
            // TODO: this may change.

            uFrom->setRealName(calUser);
            uFrom->setCAPIInfo(calUser, passwd, calHost, calNode);
            if (0)
            {
                if (passwd) XP_FREE(passwd);
                if (capurl) XP_FREE(capurl);
            }
        }

        uSubject = subject.GetBuffer();
        uLoginName = LoginName.GetBuffer();

		txnObj = TransactionObjectFactory::Make(calendar, *(events),
												*uFrom, recipients, uSubject, *capiModifiers,
												&jf, this_context, uLoginName);
		if (txnObj != 0)
		{
			txnObj->execute(0, txnStatus);
			delete txnObj; txnObj = 0;
		}

		if (uFrom) delete uFrom;
	    
        delete capiModifiers;
	}

	// If more details windows, then close it
	// Only if there were no problems
	if (txnStatus == TransactionObject::TR_OK)
	{
		if (jf.getContext())
		{
			julian_handle_close(&jf);
		}
	}
}

void
julian_send_response(JulianString& subject, JulianPtrArray& recipients, JulianString& LoginName, JulianForm& jf, NSCalendar& calendar)
{
    julian_send_response_with_events(subject, recipients, LoginName, jf, calendar, calendar.getEvents());
}

void 
julian_fill_in_delegatedToVector(char * deltonames,
                                 JulianPtrArray * vDelegatedToFillIn)
{    
    if (0 != vDelegatedToFillIn)
    {
        UnicodeString mailto = "MAILTO:";
        UnicodeString del_name;
        char * nextname;
		while (*deltonames)
		{
			del_name = mailto;
			nextname = deltonames;

            while (nextname && *nextname && *nextname != ' ')
            {
                nextname++;
            }

            if (*nextname)
            {
                *nextname = '\0';
    			nextname++;
            }

			del_name += deltonames;
            vDelegatedToFillIn->Add(new UnicodeString(del_name));

	    	if (!(*nextname))
		    {
			    // Last one
				break;
            }
            else
            {						    	 
                deltonames = nextname;
            }
		}
    }
}

XP_Bool
julian_events_comments_and_attendees_match(TimeBasedEvent * a, TimeBasedEvent * b)
{
    if (a != 0 && b != 0)
    {
        // comments must match and attendees must match
        if (a->getComment() != 0 && b->getComment() == 0)
            return FALSE;
        if (a->getComment() == 0 && b->getComment() != 0)
            return FALSE;
        if (a->getComment() != 0 && b->getComment() != 0)
        {
            if (a->getComment()->GetSize() != b->getComment()->GetSize())
                return FALSE;
            // only compare 1st comment in vector
            if (a->getComment()->GetSize() > 0 && b->getComment()->GetSize() > 0)
            {
                ICalProperty * iua = 0;
                ICalProperty * iub = 0;
                UnicodeString ua, ub;
                iua = (ICalProperty *) a->getComment()->GetAt(0);
                iub = (ICalProperty *) b->getComment()->GetAt(0);
                ua = *((UnicodeString *) iua->getValue());
                ub = *((UnicodeString *) iub->getValue());
                if (ua != ub)
                    return FALSE;
            }
        }

        if (a->getAttendees() != 0 && b->getAttendees() == 0)
            return FALSE;
        if (a->getAttendees() == 0 && b->getAttendees() != 0)
            return FALSE;
        if (a->getAttendees() != 0 && b->getAttendees() != 0)
        {
            if (a->getAttendees()->GetSize() != b->getAttendees()->GetSize())
                return FALSE;
            t_int32 i;
            Attendee * attA = 0;
            Attendee * attB = 0;
            for (i = 0; i < a->getAttendees()->GetSize(); i++)
            {
                attA = (Attendee *) a->getAttendees()->GetAt(i);
                attB = (Attendee *) b->getAttendees()->GetAt(i);
                if (attA->getName() != attB->getName())
                    return FALSE;
                if (attA->getStatus() != attB->getStatus())
                    return FALSE;
            }
        }
        return TRUE;
    }
    return FALSE;
}

void
julian_add_new_event_to_send(JulianPtrArray * vvEventsToSend, TimeBasedEvent * eventToSend)
{
    
    if (vvEventsToSend != 0)
    {
        XP_Bool bFound = FALSE;
        int32 i;
        JulianPtrArray * vEventsToSend = 0;
        TimeBasedEvent * event;
        for (i = 0; i < vvEventsToSend->GetSize(); i++)
        {
            vEventsToSend = (JulianPtrArray *) vvEventsToSend->GetAt(i);
            if (vEventsToSend != 0 && vEventsToSend->GetSize() > 0)
            {
                event = (TimeBasedEvent *) vEventsToSend->GetAt(0);
                if (julian_events_comments_and_attendees_match(event, eventToSend))
                {
                    vEventsToSend->Add(eventToSend);
                    bFound = TRUE;
                    break;
                }
            }
        }
        if (!bFound)
        {
            JulianPtrArray * evtVctrToAdd = new JulianPtrArray();
            if (evtVctrToAdd != 0)
            {
                evtVctrToAdd->Add(eventToSend);
                vvEventsToSend->Add(evtVctrToAdd);
            }
        }
    }
}

void
#ifdef OSF1
julian_handle_accept(JulianForm *jf, int newStatus)
#else
julian_handle_accept(JulianForm *jf, Attendee::STATUS newStatus)
#endif
{
	NSCalendar*		imipCal = jf->getCalendar();
	ICalComponent*	component;
    JulianString	orgName;
    char*			name = nil;
	JulianString	nameU;
	JulianString	subject = JulianString(jf->getLabel());
    UnicodeString   uTemp;
    int32           i;
    int32           j;
    int32           k;

    // this should be set to the logged in user
    // TODO: make it efficient
    // TRY to minimize number of calls to julian_send_response_with_events.

	if (jf->getCallbacks()->CopyCharPref)
		(*jf->getCallbacks()->CopyCharPref)("mail.identity.useremail", &name);
	if (name)
	{
		nameU = "MAILTO:";
		nameU += name;
	}

	if (imipCal->getEvents() != 0)
    {
		XP_Bool firstTime = TRUE;
        // process events to send
        UnicodeString uName = nameU.GetBuffer();
        JulianPtrArray * vDelegatedTo = 0;
        char * deltonames = 0;
        char * cOrgName = 0;
        char * cSummary = 0;

        XP_Bool bDontSend = FALSE;
        JulianPtrArray * recipients = 0;
        TimeBasedEvent * tbe;
        // The overall strategy
        // Create a vector of vector of events (vvEventsToReply).
        // This vector is used to minimize the number of send_response calls (which sends email).
        // Events with the same comments and attendees(status and name) are placed in the same bucket.
        // Events in the same bucket are send in the same email message.
        // Thus I minimize the number of sends response calls.
        // 
        // Foreach event in calendar,
        //   setAttendeeStatus for this user to newStatus
        //   setComment from comment text-field
        //   setDTSTAMP
        //   if (first event in calendar) set subject to its summary.
        //   add event to vvEventsToReply
        // Foreach vector in vvEventsToReply
        //   send REPLY message to ORGANIZER                                            
        //   if (delegated status)
        //      send DELEGATE-REQUEST message to DELEGATEEs
        //
        JulianPtrArray * vvEventsToReply = new JulianPtrArray();

		for (i = 0; i < imipCal->getEvents()->GetSize(); i++)
		{
			component = (ICalComponent *) imipCal->getEvents()->GetAt(i);

			if (component->GetType() == ICalComponent::ICAL_COMPONENT_VEVENT)
			{                
                tbe = (TimeBasedEvent *) component;
                // Set org
				if (orgName.GetStrlen() == 0) 
                {
                    if (tbe->getOrganizer().size() > 0)
                    {
                        cOrgName = (tbe->getOrganizer()).toCString("");
                        if (0 != cOrgName)
                            orgName = cOrgName;
                    }
                }
                if (newStatus == Attendee::STATUS_DELEGATED) 
                {
                    deltonames = jf->getDelTo();
                    if ((0 != deltonames) && (XP_STRLEN(deltonames) > 0))
                    {
                        vDelegatedTo = new JulianPtrArray();
                        if (0 != vDelegatedTo)
                            julian_fill_in_delegatedToVector(deltonames, vDelegatedTo);
                    }
                }
				tbe->setAttendeeStatusInt(uName, newStatus, vDelegatedTo);
    
				if (jf->hasComment())
				{
					tbe->setNewComments(jf->getComment());
				}
				tbe->stamp();

                if (i == 0)
                {
                    // only print subject once and use first summary.
    				subject += JulianFormFactory::SubjectSep;
	    			cSummary = (tbe->getSummary()).toCString("");
                    subject += cSummary;
                }

                julian_add_new_event_to_send(vvEventsToReply, tbe);

                if (vDelegatedTo != 0)
                {
                    ICalComponent::deleteUnicodeStringVector(vDelegatedTo);
                    delete vDelegatedTo; vDelegatedTo = 0;
                }
            }
        }
        JulianPtrArray * vEventsToReply = 0;
        JulianPtrArray * replyRecipients = new JulianPtrArray();
        t_int32 method = imipCal->getMethod();

        // setup reply recipients to contain only organizer
        if (replyRecipients != 0)
        {
            if (method != NSCalendar::METHOD_PUBLISH)
            {
                UnicodeString usOrgName = orgName.GetBuffer();
                URI orgUri(usOrgName);
                User* uToOrg = new User(usOrgName, orgUri.getName());
                replyRecipients->Add(uToOrg);
            }
        }
        for (i = 0; i < vvEventsToReply->GetSize(); i++)
        {
            vEventsToReply = (JulianPtrArray *) vvEventsToReply->GetAt(i);

            // send reply
            recipients = replyRecipients;
            imipCal->setMethod(NSCalendar::METHOD_REPLY);
            julian_send_response_with_events(subject, *recipients, nameU, *jf, *imipCal, vEventsToReply);
            recipients = 0;            

            // send delegate request message if necessary
            if (newStatus == Attendee::STATUS_DELEGATED && 
                method == NSCalendar::METHOD_REQUEST) 
            {
                deltonames = jf->getDelTo();
                if ((0 != deltonames) && (XP_STRLEN(deltonames) > 0))
                {
                    vDelegatedTo = new JulianPtrArray();
                    if (0 != vDelegatedTo)
                        julian_fill_in_delegatedToVector(deltonames, vDelegatedTo);
                    else
                        bDontSend = TRUE;
                }
                else
                {
                    bDontSend = TRUE;
                }
                if (!bDontSend)
                {
                    recipients = new JulianPtrArray();
                    if (recipients != 0)
                    {
                        PR_ASSERT(vDelegatedTo != 0 && vDelegatedTo->GetSize() > 0);
                        User * userDelegate = 0;
                        for (j = 0; j < vDelegatedTo->GetSize(); j++)
                        {
                            uTemp = *((UnicodeString *) vDelegatedTo->GetAt(j));
                            URI delUri(uTemp);
                            userDelegate = new User(uTemp, delUri.getName());
                            for (k = 0; k < vEventsToReply->GetSize(); k++)
                            {
                                component = (ICalComponent *) vEventsToReply->GetAt(k);
#ifdef OSF1
                                ((TimeBasedEvent *)component)->setAttendeeStatusInt(uTemp, 0);
#else
                                ((TimeBasedEvent *)component)->setAttendeeStatusInt(uTemp, Attendee::STATUS_NEEDSACTION);
#endif
                            }
                            recipients->Add(userDelegate);
                        }
                        imipCal->setMethod(NSCalendar::METHOD_REQUEST);
                
                        julian_send_response_with_events(subject, *recipients, nameU, *jf, *imipCal, vEventsToReply);
    
    			        User::deleteUserVector(recipients);
                        delete recipients; recipients = 0;
                    }
                }
                if (vDelegatedTo != 0)
                {
                    ICalComponent::deleteUnicodeStringVector(vDelegatedTo);
                    delete vDelegatedTo; vDelegatedTo = 0;
                }
            }
        }
        // delete allocated objects.
        if (cOrgName != 0)
        {
            delete [] cOrgName; cOrgName = 0;
        }
        if (cSummary != 0)
        {
            delete [] cSummary; cSummary = 0;
        }
        if (0 != replyRecipients)
        {
            User::deleteUserVector(replyRecipients);
            delete replyRecipients; replyRecipients = 0;
        }
        for (i = vvEventsToReply->GetSize() -1; i >= 0; i--)
        {
            vEventsToReply = (JulianPtrArray *) vvEventsToReply->GetAt(i);
            ICalComponent::deleteICalComponentVector(vEventsToReply);
            delete vEventsToReply; vEventsToReply = 0;
        }
        delete vvEventsToReply; vvEventsToReply = 0;
    }
	if (name) XP_FREE(name);
}

void
Julian_ClearLoginInfo()
{
}

int 
Julian_GetLoginInfo(JulianForm& jf, MWContext* context, char** url, char** password) { 
    static char* lastCalPwd = NULL; 
    char* defaultUrl; 
    int status = -2; 
  
    if (jf.getCallbacks() &&
        jf.getCallbacks()->CopyCharPref)
        status = (*jf.getCallbacks()->CopyCharPref)(julian_pref_name, &defaultUrl);
    // -1 is pref isn't there, which will be a normal case for us
    // Any other neg number is some other bad error so bail.
    if (status < -1) return status; 

    *url = defaultUrl; 
    *password = NULL;

    if (lastCalPwd != NULL)
    { 
        *password = XP_STRDUP(lastCalPwd); 
        if (*password == NULL) return 0 /*MK_OUT_OF_MEMORY*/;

        return 1;
    } 

   if (!(*context->funcs->PromptUsernameAndPassword)
                                      (context, 
//                                    XP_GetString(JULIAN_LOGIN_PROMPT), 
                                      "Enter capi url and password", 
                                      url, 
                                      password))
    { 
        *url = NULL; 
        *password = NULL;
        status = -1;
    } else { 
        if (jf.getCallbacks() &&
            jf.getCallbacks()->SetCharPref)
        {
            (*jf.getCallbacks()->SetCharPref)(julian_pref_name, *url); 
        }
        lastCalPwd = XP_STRDUP(*password);
        status  = 1;
    } 
    XP_FREE(defaultUrl); 
    return status; 
}

