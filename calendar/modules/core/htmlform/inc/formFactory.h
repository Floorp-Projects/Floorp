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

#ifndef _JULIAN_FormFactory_H
#define _JULIAN_FormFactory_H

#include "jdefines.h"
#include "julnstr.h"

class JulianForm;
#if defined(XP_PC)
#pragma warning ( disable : 4251 )
#endif

const int32     jff_clip_events = 50;

class JULIAN_PUBLIC JulianFormFactory 
{
friend class JulianForm;

public:
		JulianFormFactory();
		JulianFormFactory(NSCalendar& imipCal);
		JulianFormFactory(NSCalendar& imipCal, JulianServerProxy* jsp);
		JulianFormFactory(NSCalendar& imipCal, JulianForm& hostForm, pJulian_Form_Callback_Struct callbacks);

        /*
        ** Call init before calling getHTML()
        */
        void init();

		virtual ~JulianFormFactory();

		/*
		** Returns a new UnicodeString with html that is intended to be enclosed in a real html file.
		** Call is responable for disposing of the returned UnicodeString.
		*/
		XP_Bool			getHTML(JulianString* htmlstorage, XP_Bool want_detail = FALSE);
		XP_Bool			getHTML(JulianString* OutString, NET_StreamClass *this_stream, XP_Bool want_detail = FALSE);

		/*
		** Sets the base NSCalendar.
		*/
		void			setNSCalendar(NSCalendar& newCalendar)		{ firstCalendar = &newCalendar; }

		/*
		** These are the different form button types. The call back url will list
		** as it's first thing the type = the name.
		*/
		static JulianString  Buttons_Details_Type;
		static JulianString  Buttons_Add_Type;
		static JulianString  Buttons_Close_Type;		
		static JulianString  Buttons_Accept_Type;
		static JulianString  Buttons_Decline_Type;
		static JulianString  Buttons_Tentative_Type;
		static JulianString  Buttons_SendFB_Type;
		static JulianString  Buttons_SendRefresh_Type;
		static JulianString  Buttons_DelTo_Type;
		static JulianString  CommentsFieldName;
		static JulianString  DelToFieldName;
		static JulianString  SubjectSep;

private:
		NSCalendar*		firstCalendar;	// Base NSCalendar 
		ICalComponent*	thisICalComp;	// Current vEnvent or vFreeBusy that is being made into html
		ICalComponent*	serverICalComp;	// Current Database Version vEnvent or vFreeBusy that is being made into html
		XP_Bool			detail;
		JulianString*	s;				// Holds the html that is being built up.
		JulianForm*		jf;
		JulianServerProxy* ServerProxy;	// How to get to the server
		int32			maxical;		// # of vevetns otr vfreebusy in the array
        int32           aref_count;     // For muiltevent table of contents
        int32           aref2_count;    // For muiltevent each form. must be in sync with aref_count
		pJulian_Form_Callback_Struct cb;// How to call code to create urls for the buttons
        NET_StreamClass *outStream;     // Could be streamed output. Optional
		XP_Bool 		do_capi_login;  // Default is not to ask for capi login info

        // Use by the freebusy table. In order.
        enum FB_State {
            Empty_State,
            Free_State,
            XParam_State,
            Busy_State
        };

        Julian_Duration	MinutesPerSlot;
        int32			SlotsPerHour;
        XP_Bool         displayTimeZone;
        int32           scaleType; // 1 = days 1 to 31, 2 = hours 0 to 24
        int32           slotsacross;
        int32           hoursToDisplay; // 1 to 24. default is 24 hours
        FB_State        state;          // For free, busy, skip state
        FB_State        new_state;
        int32           count;
        JulianString  	tz;
        JulianString    uTemp;
        JulianString    lineFormat;
        char*			TimeHour_HTML;
        int32           start_hour;
        int32           end_hour;
        DateTime		start;
        DateTime		end;
        TimeZone*		default_tz;
        Period*         p;
        Freebusy*       fb;
		int32           fbIndex;
		int32           pIndex;
        JulianPtrArray*	pv;
        JulianPtrArray* fbv;

        // These bools show what types of ical objects are being used in this message
		XP_Bool			isEvent;
		XP_Bool			isFreeBusy;

        void			SetDetail(XP_Bool newDetail)				{ detail = newDetail; };
		XP_Bool			isDetail()									{ return detail;};
		XP_Bool			isPreProcessed(JulianString&  icalpropstr)	{ return  (ICalPreProcess.IndexOf(icalpropstr.GetBuffer(), 0) >= 0);  };
		XP_Bool			isPostProcessed(JulianString&  icalpropstr)	{ return  (ICalPostProcess.IndexOf(icalpropstr.GetBuffer(), 0) >= 0); };

		JulianString  *	doARef(JulianString& refText, JulianString& refTarget, JulianString* outputString);
		JulianString  *	doFont(JulianString& fontText, JulianString* outputString);
		JulianString  *	doItalic(JulianString& ItalicText, JulianString* outputString);
		JulianString  *	doBold(JulianString& BoldText, JulianString* outputString);
		JulianString  *	doBold(char* BoldText, JulianString* outputString);

        void			doAddHTML(UnicodeString & moreHtml)			{ char*y = moreHtml.toCString(""); if (y) {*s += y;	delete y;} flush_stream(); };
        void			doAddHTML(JulianString  & moreHtml)			{ *s += moreHtml; flush_stream();};
		void			doAddHTML(JulianString  * moreHtml)			{ *s +=*moreHtml; flush_stream();};
		void			doAddHTML(char* moreHtml)					{ *s += moreHtml; flush_stream();};
        int32           flush_stream();

		void			doPreProcessing(char*  icalpropstr);
		void			doPreProcessingDateTime(JulianString  & icalpropstr, XP_Bool allday, DateTime &start, DateTime &end, ICalComponent &ic);
		void			doPreProcessingAttend(ICalComponent &ic);
		void			doPreProcessingOrganizer(ICalComponent &ic);
		void			doDifferenceProcessing(JulianString  icalpropstr);
		void			doDifferenceProcessingAttendees();
		void			doHeaderMuiltStart();
		void			doHeaderMuilt();
		void			doHeaderMuiltEnd();

		void			doProps(int32 labelCount, JulianString  labels[], int32 dataCount, JulianString  data[]);
		void			doHeader(JulianString  HeaderText);
		void			doClose();
		void			doStatus();
		void			doSingleTableLine(JulianString  & labelString, JulianString  & dataString, XP_Bool addSpacer = TRUE);
		void			doCommentText();
		JulianString	doCreateButton(JulianString InputType, JulianString ButtonName, XP_Bool addtextField = FALSE);
		void			doAddGroupButton(JulianString GroupButton_HTML);
		void			doAddButton(JulianString  SingleButton_HTML);
		void			doMakeFreeBusyTable();

        void            HandleError();
		void			HandlePublishVEvent();
		void			HandlePublishVFreeBusy(XP_Bool isPublish);
		void			HandleRequestVEvent();
		void			HandleRequestVFreeBusy();
		void			HandleEventReplyVEvent();
		void			HandleEventCancelVEvent();
		void			HandleEventRefreshRequestVEvent();
		void			HandleEventCounterPropVEvent();
		void			HandleDeclineCounterVEvent();

        char*           getJulianErrorString(int32 ErrorNum);

/* Table Utils Functions */
		void			addLegend();
		void			addTimeZone();
		void			addMajorScale();
		void			addMinorScale();
		void			addTicksScale();
		/*void			emptyRow();*/
		void			makeHourFBTable();
		void			makeDaysFBTable();
		void			DaysLineInc(DateTime* dtTime);
		void			MonthLineInc(DateTime* dtTime);
		void			LineInc(DateTime* dtTime);
        int32           LineCheck(DateTime& baseTime, DateTime& checkTime, DateTime& checkTimeEnd);
        void            enterState(FB_State newState, XP_Bool forse);
        void            checkPeriodBounds(DateTime& startofslot, DateTime& endofslot);
        Period*         getNextPeriod();
        int32           getFBType();

        JulianString    MoreErrors;
        JulianString    TooManyEvents;
        JulianString    error0;
        JulianString    error1;
        JulianString    error2;
        JulianString    error3;
        JulianString    error4;
        JulianString    error5;
        JulianString    error6;
        JulianString    error7;

		static char*         Start_HTML;
		static char*         End_HTML;
		static char*         Props_Head_HTML;
		static char*         Props_HTML_Before_Label;
		static char*         Props_HTML_After_Label;
		static char*         Props_HTML_After_Data;
		static char*         Props_HTML_Empty_Label;
		static char*         Props_End_HTML;
		static char*         General_Header_Start_HTML;
		static char*         General_Header_Status_HTML;
		static char*         General_Header_End_HTML;
		static char*         Head2_HTML;
		static char*         Italic_Start_HTML;
		static char*         Italic_End_HTML;
		static char*         Bold_Start_HTML;
		static char*         Bold_End_HTML;
		static char*         Aref_Start_HTML;
		static char*         Aref_End_HTML;
		static char*         ArefTag_End_HTML;
		static char*         nbsp;
		static char*         Accepted_Gif_HTML;
		static char*         Declined_Gif_HTML;
		static char*         Delegated_Gif_HTML;
		static char*         NeedsAction_Gif_HTML;
		static char*         Question_Gif_HTML;
		static char*         Line_3_HTML;
		static char*         Cell_Start_HTML;
		static char*         Cell_End_HTML;
		static char*         Font_Fixed_Start_HTML;
		static char*         Font_Fixed_End_HTML;
		static char*         Line_Break_HTML;

		static char*         Start_Font;
		static char*         Start_BIG_Font;
		static char*         End_Font;

		static char*         Buttons_Single_Start_HTML;
		static char*         Buttons_Single_Mid_HTML;
		static char*         Buttons_Single_End_HTML;
		static char*         Buttons_Text_End_HTML;

        JulianString  Buttons_Details_Label;
        JulianString  Buttons_Add_Label;
        JulianString  Buttons_Close_Label;
        JulianString  Buttons_Accept_Label;
        JulianString  Buttons_AcceptAll_Label;
        JulianString  Buttons_Update_Label;
        JulianString  Buttons_Decline_Label;
        JulianString  Buttons_Tentative_Label;
        JulianString  Buttons_SendFB_Label;
        JulianString  Buttons_SendRefresh_Label;
        JulianString  Buttons_DelTo_Label;

		static char*         Buttons_SaveDel_HTML;

		static char*         Buttons_GroupStart_HTML;
		static char*         Buttons_GroupEnd_HTML;
		static char*         Buttons_GroupSingleStart_HTML;
		static char*         Buttons_GroupSingleEnd_HTML;

		static char*         Text_Label_Start_HTML;
		static JulianString  Text_Label;
		static char*         Text_Label_End_HTML;
		static char*         Text_Field_HTML;

        static JulianString  ICalPreProcess;
		static JulianString  ICalPostProcess;

        JulianString  EventInSchedule;
        JulianString  EventNotInSchedule;
        JulianString  EventConflict;
        JulianString  EventNote;
        JulianString  EventError;
        JulianString  EventTest;
        JulianString  Text_To;	
        JulianString  Text_AllDay;
        JulianString  Text_StartOn;
        JulianString  Text_Was;
        JulianString  MuiltEvent;
        JulianString  WhenStr;
        JulianString  WhatStr;

		JulianString  MuiltEvent_Header_HTML;
		JulianString  MuiltFB_Header_HTML;

        char*        String_What;		                
        char*        String_When;		                
        char*        String_Location;		            
        char*        String_Organizer;		            
        char*        String_Status;		                
        char*        String_Priority;		            
        char*        String_Categories;		            
        char*        String_Resources;		            
        char*        String_Attachments	;	            
        char*        String_Alarms;		                
        char*        String_Created	;	                
        char*        String_Last_Modified;	            
        char*        String_Sent;	    	            
        char*        String_UID	;	                    

		/*
		** Free/Busy Table
		*/
		static char*         FBT_Start_HTML;
		static char*         FBT_End_HTML;
		static char*         FBT_NewRow_HTML;
		static char*         FBT_EndRow_HTML;
		static char*         FBT_TimeHead_HTML;
		static char*         FBT_TimeHeadEnd_HTML;
		static char*         FBT_TimeHour_HTML;
		static char*         FBT_TimeHourEnd_HTML;
		static char*         FBT_TD_HourColor_HTML;
		static char*         FBT_TD_HourColorEnd_HTML;
		static char*         FBT_TD_MinuteColor_HTML;
		static char*         FBT_TD_MinuteColorEnd_HTML;
		static char*         FBT_TDOffsetCell_HTML;
		static char*         FBT_TickLong_HTML;
		static char*         FBT_TickShort_HTML;
		static char*         FBT_TimeMin_HTML;
		static char*         FBT_TimeMinEnd_HTML;
		static char*         FBT_HourStart;
		static char*         FBT_HourEnd;

		static char*         FBT_DayStart_HTML;
		static char*         FBT_DayEnd_HTML;
		static char*         FBT_DayEmptyCell_HTML;
		static char*         FBT_DayFreeCell_HTML;
		static char*         FBT_DayBusyCell_HTML;
		static char*         FBT_DayXParamCell_HTML;
		static char*         FBT_EmptyRow_HTML;
		static char*         FBT_DayXColFreeCell_HTML;
		static char*         FBT_DayXColBusyCell_HTML;
		static char*         FBT_DayXColEmptyCell_HTML;
		static char*         FBT_DayXXParamCell_HTML;

        static char*         FBT_MonthFormat;
        static char*         FBT_TickDaySetting;

		static char*         FBT_Legend_Start_HTML;
		static char*         FBT_Legend_Text1_HTML;
		static char*         FBT_Legend_Text2_HTML;
		static char*         FBT_Legend_Text3_HTML;
        static char*         FBT_Legend_Text4_HTML;
        static char*         FBT_Legend_Text5_HTML;
		static char*         FBT_Legend_TextEnd_HTML;
		static char*         FBT_Legend_End_HTML;

        JulianString  FBT_Legend_Title;
        JulianString  FBT_Legend_Free;
        JulianString  FBT_Legend_Busy;
        JulianString  FBT_Legend_Unknown;
        JulianString  FBT_Legend_xparam;

		       JulianString  FBT_AM;
		       JulianString  FBT_PM;
		static char*         FBT_TickMark1;
		static char*         FBT_TickMark2;
		static char*         FBT_TickMark3;
		static char*         FBT_TickMark4;
		static char*         FBT_TickSetting;
		static char*         FBT_TickOffset;
		static char*         FBT_DayFormat;

        /*
        ** Error
        */
		       char*         error_Header_HTML;
		static int32		 error_Fields_Labels_Length;
		static JulianString  error_Fields_Labels[];
		static int32		 error_Fields_Data_HTML_Length;
		static JulianString  error_Fields_Data_HTML[];
		static char*         error_End_HTML	;

		/*
		** Publish
		*/
		       char*         publish_Header_HTML;
		static int32		 publish_Fields_Labels_Length;
		static JulianString  publish_Fields_Labels[];
		static int32		 publish_Fields_Data_HTML_Length;
		static JulianString  publish_Fields_Data_HTML[];
		static char*         publish_End_HTML;

		/*
		** Publish Detail
		*/
		static int32		 publish_D_Fields_Labels_Length;
		static JulianString  publish_D_Fields_Labels[];
		static int32		 publish_D_Fields_Data_HTML_Length;
		static JulianString  publish_D_Fields_Data_HTML[];

		/*
		** Publish VFreeBusy
		*/
		       char*         publishFB_Header_HTML;
		       char*         replyFB_Header_HTML;
		static int32		 publishFB_Fields_Labels_Length;
		static JulianString  publishFB_Fields_Labels[];
		static int32		 publishFB_Fields_Data_HTML_Length;
		static JulianString  publishFB_Fields_Data_HTML[];
		static char*         publishFB_End_HTML;

		/*
		** Publish VFreeBusy Detail
		*/
		static int32		 publishFB_D_Fields_Labels_Length;
		static JulianString  publishFB_D_Fields_Labels[];
		static int32		 publishFB_D_Fields_Data_HTML_Length;
		static JulianString  publishFB_D_Fields_Data_HTML[];

		/*
		** Request
		*/
		       char*         request_Header_HTML;
		static int32		 request_Fields_Labels_Length;
		static JulianString  request_Fields_Labels[];
		static int32		 request_Fields_Data_HTML_Length;
		static JulianString  request_Fields_Data_HTML[];
		static char*         request_End_HTML;

		/*
		** Request Detail
		*/
		static int32		 request_D_Fields_Labels_Length;
		static JulianString  request_D_Fields_Labels[];
		static int32		 request_D_Fields_Data_HTML_Length;
		static JulianString  request_D_Fields_Data_HTML[];

		/*
		** Request VFreeBusy
		*/
		       char*         request_FB_Header_HTML;			
		static int32		 request_FB_Fields_Labels_Length;	
		static JulianString  request_FB_Fields_Labels[];		
		static int32		 request_FB_Fields_Data_HTML_Length;
		static JulianString  request_FB_Fields_Data_HTML[];	
		static char*         request_FB_End_HTML;				

		/*
		** Request VFreeBusy Detail
		*/
		       char*         request_D_FB_Header_HTML;
		static int32		 request_D_FB_Fields_Labels_Length;
		static JulianString  request_D_FB_Fields_Labels[];
		static int32		 request_D_FB_Fields_Data_HTML_Length;
		static JulianString  request_D_FB_Fields_Data_HTML[];
		static char*         request_D_FB_End_HTML;

		/*
		** Event Reply
		*/
		       char*         eventreply_Header_HTML;
		static int32		 eventreply_Fields_Labels_Length;
		static JulianString  eventreply_Fields_Labels[];
		static int32		 eventreply_Fields_Data_HTML_Length;
		static JulianString  eventreply_Fields_Data_HTML[];
		static char*         eventreply_End_HTML;
		
		/*
		** Event Reply Detail
		*/
		static int32		 eventreply_D_Fields_Labels_Length;
		static JulianString  eventreply_D_Fields_Labels[];
		static int32		 eventreply_D_Fields_Data_HTML_Length;
		static JulianString  eventreply_D_Fields_Data_HTML[];

		/*
		** Event Cancel
		*/
		       char*         eventcancel_Header_HTML;
		static int32		 eventcancel_Fields_Labels_Length;
		static JulianString  eventcancel_Fields_Labels[];
		static int32		 eventcancel_Fields_Data_HTML_Length;
		static JulianString  eventcancel_Fields_Data_HTML[];
		static char*         eventcancel_End_HTML;
		
		/*
		** Event Cancel Detail
		*/
		static int32		 eventcancel_D_Fields_Labels_Length;
		static JulianString  eventcancel_D_Fields_Labels[];
		static int32		 eventcancel_D_Fields_Data_HTML_Length;
		static JulianString  eventcancel_D_Fields_Data_HTML[];
		
		/*
		** Event Refresh Request
		*/
		       char*         eventrefreshreg_Header_HTML;
		static int32		 eventrefreshreg_Fields_Labels_Length;
		static JulianString  eventrefreshreg_Fields_Labels[];
		static int32		 eventrefreshreg_Fields_Data_HTML_Length;
		static JulianString  eventrefreshreg_Fields_Data_HTML[];
		static char*         eventrefreshreg_End_HTML;

		/*
		** Event Refresh Request Detail
		*/
		static int32		 eventrefreshreg_D_Fields_Labels_Length;
		static JulianString  eventrefreshreg_D_Fields_Labels[];
		static int32		 eventrefreshreg_D_Fields_Data_HTML_Length;
		static JulianString  eventrefreshreg_D_Fields_Data_HTML[];

		/*
		** Event Counter Proposal
		*/
		        char*         eventcounterprop_Header_HTML;

		/*
		** Event Deline Counter
		*/
		       char*         eventdelinecounter_Header_HTML;
		static int32		 eventdelinecounter_Fields_Labels_Length;
		static JulianString  eventdelinecounter_Fields_Labels[];
		static int32		 eventdelinecounter_Fields_Data_HTML_Length;
		static JulianString  eventdelinecounter_Fields_Data_HTML[];
};
#if defined(XP_PC)
#pragma warning ( default : 4251 )
#endif

#endif
