/* $Id: menus.h,v 1.1 1998/09/25 18:01:36 ramiro%netscape.com Exp $
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
 * Copyright (C) 1998 Netscape Communications Corporation.  Portions
 * created by Warwick Allison, Kalle Dalheimer, Eirik Eng, Matthias
 * Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav Tvete are
 * Copyright (C) 1998 Warwick Allison, Kalle Dalheimer, Eirik Eng,
 * Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  All Rights Reserved.
 */

#ifndef _MENUS_H
#define _MENUS_H

class QMenuBar;

#define MENU_BF_FILE                    11000
#define MENU_BF_EDIT                    11100
#define MENU_BF_VIEW                    11200
#define MENU_BF_GO                      11300
#define MENU_WINDOW                     10400
#define MENU_HELP                       10500
#define MENU_NEW                        10000
#define MENU_BOOKMARKS                  10100
#define MENU_ENCODING                   10200
#define MENU_NEW_NAVIGATORWINDOW        10001
#define MENU_NEW_COMPOSEMESSAGE         10002
#define MENU_NEW_BLANKPAGE              10003
#define MENU_NEW_NEWTEMPLATE            10004
#define MENU_NEW_NEWWIZARD              10005
#define MENU_BF_FILE_NAVIGATORWINDOW    11001
#define MENU_BF_FILE_OPENPAGE           11002
#define MENU_BF_FILE_SAVEAS             11003
#define MENU_BF_FILE_SAVEFRAMEAS        11004
#define MENU_BF_FILE_SENDPAGE           11005
#define MENU_BF_FILE_SENDLINK           11006
#define MENU_BF_FILE_EDITPAGE           11007
#define MENU_BF_FILE_EDITFRAME          11008
#define MENU_BF_FILE_UPLOADFILE         11009
#define MENU_BF_FILE_PRINT              11010
#define MENU_BF_FILE_CLOSE              11011
#define MENU_BF_FILE_QUIT               11012
#define MENU_BF_EDIT_UNDO               11101
#define MENU_BF_EDIT_REDO               11102
#define MENU_BF_EDIT_CUT                11103
#define MENU_BF_EDIT_COPY               11104
#define MENU_BF_EDIT_PASTE              11105
#define MENU_BF_EDIT_SELECTALL          11106
#define MENU_BF_EDIT_FINDINOBJECT       11107
#define MENU_BF_EDIT_FINDAGAIN          11108
#define MENU_BF_EDIT_SEARCHINTERNET     11109
#define MENU_BF_EDIT_SEARCHADDRESS      11110
#define MENU_BF_EDIT_PREFERENCES        11111
#define MENU_BF_VIEW_TOGGLENAVIGATIONTOOLBAR 11201
#define MENU_BF_VIEW_TOGGLELOCATIONTOOLBAR   11202
#define MENU_BF_VIEW_TOGGLEPERSONALTOOLBAR   11203
#define MENU_BF_VIEW_INCREASEFONT            11204
#define MENU_BF_VIEW_DECREASEFONT            11205
#define MENU_BF_VIEW_RELOAD                  11206
#define MENU_BF_VIEW_SHOWIMAGES              11207
#define MENU_BF_VIEW_REFRESH                 11208
#define MENU_BF_VIEW_STOPLOADING             11209
#define MENU_BF_VIEW_VIEWPAGESOURCE          11210
#define MENU_BF_VIEW_VIEWPAGEINFO            11211
#define MENU_BF_VIEW_PAGESERVICES            11212
#define MENU_BF_VIEW_ENCODING                11213
#define MENU_BF_GO_BACK                      11301
#define MENU_BF_GO_FORWARD                   11302
#define MENU_BF_GO_HOME                      11303
#define MENU_WINDOW_OPENNAVCENTER            10401
#define MENU_WINDOW_OPENBROWSER              10402
#define MENU_WINDOW_OPENINBOX                10403
#define MENU_WINDOW_OPENNEWSGROUPS           10404
#define MENU_WINDOW_OPENEDITOR               10405
#define MENU_WINDOW_OPENCONFERENCE           10406
#define MENU_WINDOW_OPENCALENDAR             10407
#define MENU_WINDOW_HOSTONDEMAND             10408
#define MENU_WINDOW_NETCASTER                10409
#define MENU_WINDOW_TOGGLETASKBAR            10410
#define MENU_WINDOW_OPENFOLDERS              10411
#define MENU_WINDOW_OPENADDRESSBOOK          10412
#define MENU_WINDOW_BOOKMARKS                10413
#define MENU_WINDOW_OPENHISTORY              10414
#define MENU_WINDOW_JAVACONSOLE              10415
#define MENU_WINDOW_SECURITY                 10416
#define MENU_HELP_ABOUTNETSCAPE              10501
#define MENU_HELP_ABOUTMOZILLA               10502
#define MENU_HELP_ABOUTQT                    10503
#define MENU_BOOKMARK_ADD		     10601
#define MENU_BOOKMARK_EDIT		     10602
#define MENU_BMF_FILE                        12000
#define MENU_BMF_EDIT                        12100
#define MENU_BMF_VIEW                        12200
#define MENU_BMF_FILE_NEWBOOKMARK            12001
#define MENU_BMF_FILE_NEWFOLDER              12002
#define MENU_BMF_FILE_NEWSEPARATOR           12003
#define MENU_BMF_FILE_OPENBOOKMARKFILE       12004
#define MENU_BMF_FILE_IMPORT                 12005
#define MENU_BMF_FILE_SAVEAS                 12006
#define MENU_BMF_FILE_OPENSELECTED           12007
#define MENU_BMF_FILE_ADDTOTOOLBAR           12008
#define MENU_BMF_FILE_MAKEALIAS              12009
#define MENU_BMF_FILE_CLOSE                  12010
#define MENU_BMF_FILE_EXIT                   12011
#define MENU_BMF_EDIT_UNDO                   12101
#define MENU_BMF_EDIT_REDO                   12102
#define MENU_BMF_EDIT_CUT                    12103
#define MENU_BMF_EDIT_COPY                   12104
#define MENU_BMF_EDIT_PASTE                  12105
#define MENU_BMF_EDIT_DELETE                 12106
#define MENU_BMF_EDIT_SELECTALL              12107
#define MENU_BMF_EDIT_FINDINOBJECT           12108
#define MENU_BMF_EDIT_FINDAGAIN              12109
#define MENU_BMF_EDIT_SEARCHINTERNET         12110
#define MENU_BMF_EDIT_SEARCHADDRESS          12111
#define MENU_BMF_EDIT_PREFERENCES            12112
#define MENU_BMF_VIEW_SORTBYTITLE            12201
#define MENU_BMF_VIEW_SORTBYLOCATION         12202
#define MENU_BMF_VIEW_SORTBYDATELASTVISITED  12203
#define MENU_BMF_VIEW_SORTBYDATECREATED      12204
#define MENU_BMF_VIEW_SORTASCENDING          12205
#define MENU_BMF_VIEW_SORTDESCENDING         12206
#define MENU_BMF_VIEW_BOOKMARKUP             12207
#define MENU_BMF_VIEW_BOOKMARKDOWN           12208
#define MENU_BMF_VIEW_BOOKMARKWHATSNEW       12209
#define MENU_BMF_VIEW_SETTOOLBARFOLDER       12210
#define MENU_BMF_VIEW_SETNEWBOOKMARKFOLDER   12211
#define MENU_BMF_VIEW_SETBOOKMARKMENUFOLDER  12212
#define MENU_HIST_FILE                       12300
#define MENU_HIST_EDIT                       12400
#define MENU_HIST_VIEW                       12500
#define MENU_HIST_FILE_SAVEAS                12501
#define MENU_HIST_FILE_GOTOPAGE              12502
#define MENU_HIST_FILE_ADDBOOKMARK           12503
#define MENU_HIST_FILE_ADDTOOLBAR            12504
#define MENU_HIST_FILE_CLOSE                 12505
#define MENU_HIST_FILE_EXIT                  12506
#define MENU_HIST_EDIT_UNDO                  12507
#define MENU_HIST_EDIT_REDO                  12508
#define MENU_HIST_EDIT_CUT                   12509
#define MENU_HIST_EDIT_COPY                  12510
#define MENU_HIST_EDIT_PASTE                 12511
#define MENU_HIST_EDIT_DELETE                12512
#define MENU_HIST_EDIT_SELECTALL             12513
#define MENU_HIST_EDIT_FINDINOBJECT          12514
#define MENU_HIST_EDIT_FINDAGAIN             12515
#define MENU_HIST_EDIT_PREFERENCES           12516
#define MENU_HIST_VIEW_SORTBYTITLE           12517
#define MENU_HIST_VIEW_SORTBYLOCATION        12518
#define MENU_HIST_VIEW_SORTBYDATEFIRSTVISITED 12519
#define MENU_HIST_VIEW_SORTBYDATELASTVISITED 12520
#define MENU_HIST_VIEW_EXPIRATIONDATE        12521
#define MENU_HIST_VIEW_VISITCOUNT            12522
#define MENU_HIST_VIEW_SORTASCENDING         12523
#define MENU_HIST_VIEW_SORTDESCENDING        12524


#define MENU_BMF_BOOKMARKS_OFFSET           100000
#endif





