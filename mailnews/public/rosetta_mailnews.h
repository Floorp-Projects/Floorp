/*
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

#ifndef _ROSETTA_MAILNEWS_H_
#define _ROSETTA_MAILNEWS_H_

/* If you need a new rosetta number, use the first one marked "--- unused, take it ! ---" */

#define HJ04305
#define HJ75043
#define HJ02278
#define HJ99161 
#define HJ95534
#define HJ73123
#define HJ77855
#define HJ85617
#define HJ69290
#define HJ43055
#define HJ32538
#define HJ83505
#define HJ37212
#define HJ91549
#define HJ42256
#define HJ27863
#define HJ26763
#define HJ64582
#define HJ92535
#define HJ22867
#define HJ09384
#define HJ74362
#define HJ38714
#define HJ42055
#define HJ53211
#define HJ19119
#define HJ96829

#define HJ89510 \
	retHost = GetMaster()->FindHost(host_and_port, -1);

#define HJ97882 \
	MSG_NewsHost* FindHost(const char* name, int32 port) {return NULL;}

#define HJ13591
#define HJ86782
#define HJ08142
#define HJ21695
#define HJ98703 
#define HJ30181 \
	PRInt16 SetNewsUrlHeader (const char *hostPort, const char *group);

#define HJ36954 \
	PRInt16 nsMsgCompFields::SetNewsUrlHeader (const char *hostPort, const char *group)

#define HJ57077 \
	PR_smprintf ("%s://%s/", "news", hostPort);

#define HJ81279 \
	PRBool aBool; \
	status = NET_parse_news_url (singleValue, &hostPort, &aBool, &group, &id, &data);

#define HJ78808 \
	status = SetNewsUrlHeader (hostPort, FALSE, group);

#define HJ08239	--- unused #040, take it! ---
#define HJ45609	--- unused #041, take it! ---
#define HJ58534	--- unused #042, take it! ---
#define HJ91531	--- unused #043, take it! ---
#define HJ62011	--- unused #044, take it! ---
#define HJ41792	--- unused #045, take it! ---
#define HJ70669	--- unused #046, take it! ---
#define HJ77514	--- unused #047, take it! ---
#define HJ97347	--- unused #048, take it! ---
#define HJ82388	--- unused #049, take it! ---
#define HJ67078	--- unused #050, take it! ---
#define HJ10196	--- unused #051, take it! ---
#define HJ58932	--- unused #052, take it! ---
#define HJ04608	--- unused #053, take it! ---
#define HJ79059	--- unused #054, take it! ---
#define HJ73882	--- unused #055, take it! ---
#define HJ30080	--- unused #056, take it! ---
#define HJ45070	--- unused #057, take it! ---
#define HJ61958	--- unused #058, take it! ---
#define HJ21522	--- unused #059, take it! ---
#define HJ96785	--- unused #060, take it! ---
#define HJ59487	--- unused #061, take it! ---
#define HJ54981	--- unused #062, take it! ---
#define HJ00694	--- unused #063, take it! ---
#define HJ41820	--- unused #064, take it! ---
#define HJ45057	--- unused #065, take it! ---
#define HJ36644	--- unused #066, take it! ---
#define HJ10796	--- unused #067, take it! ---
#define HJ54126	--- unused #068, take it! ---
#define HJ98599	--- unused #069, take it! ---
#define HJ77112	--- unused #070, take it! ---
#define HJ47843	--- unused #071, take it! ---
#define HJ99856	--- unused #072, take it! ---
#define HJ19399	--- unused #073, take it! ---
#define HJ44303	--- unused #074, take it! ---
#define HJ25252	--- unused #075, take it! ---
#define HJ31338	--- unused #076, take it! ---
#define HJ97854	--- unused #077, take it! ---
#define HJ61508	--- unused #078, take it! ---
#define HJ99641	--- unused #079, take it! ---
#define HJ11208	--- unused #080, take it! ---
#define HJ03550	--- unused #081, take it! ---
#define HJ87744	--- unused #082, take it! ---
#define HJ66276	--- unused #083, take it! ---
#define HJ29683	--- unused #084, take it! ---
#define HJ44384	--- unused #085, take it! ---
#define HJ24073	--- unused #086, take it! ---
#define HJ47039	--- unused #087, take it! ---
#define HJ04672	--- unused #088, take it! ---
#define HJ09993	--- unused #089, take it! ---
#define HJ65563	--- unused #090, take it! ---
#define HJ24393	--- unused #091, take it! ---
#define HJ91088	--- unused #092, take it! ---
#define HJ14587	--- unused #093, take it! ---
#define HJ85226	--- unused #094, take it! ---
#define HJ39686	--- unused #095, take it! ---
#define HJ86771	--- unused #096, take it! ---
#define HJ17283	--- unused #097, take it! ---
#define HJ60714	--- unused #098, take it! ---
#define HJ06328	--- unused #099, take it! ---
#define HJ73151	--- unused #100, take it! ---


#endif _ROSETTA_MAILNEWS_H_
