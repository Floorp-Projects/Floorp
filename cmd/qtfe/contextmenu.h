/* $Id: contextmenu.h,v 1.1 1998/09/25 18:01:32 ramiro%netscape.com Exp $
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
 * The Initial Developer of this code under the NPL is Arnt
 * Gulbrandsen.  Further developed by Warwick Allison, Kalle Dalheimer,
 * Eirik Eng, Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and
 * Paul Olav Tvete.  Copyright (C) 1998 Warwick Allison, Kalle
 * Dalheimer, Eirik Eng, Matthias Ettrich, Arnt Gulbrandsen, Haavard
 * Nord and Paul Olav Tvete.  All Rights Reserved.
 */

#ifndef CONTEXTMENU_H
#define CONTEXTMENU_H

#include <qpopmenu.h>
#include <qstring.h>


class QtBrowserContext;

class ContextMenu: public QPopupMenu
{
    Q_OBJECT
public:
    enum MenuItem { Back = 1, Forward, Reload,
		    OpenLinkWindow,
		    ViewSource, ViewInfo, ViewImage,
		    AddBookmark, SendPage,
		    SaveLink, SaveImageAs,
		    CopyLinkLocation, CopyImageLocation };
		
    ContextMenu( QtBrowserContext * context, const QPoint & pos,
		 const char * linkToURL, const char * imageURL );
    ~ContextMenu();

public slots:
    void goAway();

private slots:
    void openLinkWindow();

    void sendPage();

    void viewImage();

    void saveLinkAs();
    void saveImageAs();
    void copyLinkLocation();
    void copyImageLocation();

private:
    void insert( int id, const char * text,
		 QObject * receiver, const char * slot, int accel=0 );

    QtBrowserContext * context;
    QString linkTo, image;
};

#endif
