/* $Id: QtBookmarkMenu.h,v 1.1 1998/09/25 18:01:27 ramiro%netscape.com Exp $
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
 * The Initial Developer of this code under the NPL is Eirik Eng.
 * Further developed by Warwick Allison, Kalle Dalheimer, Eirik Eng,
 * Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  Copyright (C) 1998 Warwick Allison, Kalle Dalheimer, Eirik
 * Eng, Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  All Rights Reserved.
 */

#ifndef _QTBOOKMARKMENU_H
#define _QTBOOKMARKMENU_H

#include <qintdict.h>
#include <qpopmenu.h>

#include "bkmks.h"

class QLineEdit;
class QMultiLineEdit;
class QtBrowserContext;
class QtBMEntryDict;

class QtBookmarkMenu : public QPopupMenu
{
    Q_OBJECT
public:
    QtBookmarkMenu( QtBrowserContext *b = 0,
		    QWidget* p = 0, const char* wName = 0 );
    ~QtBookmarkMenu();

    static void invalidate();

public slots:    
    void cmdAddBookmark();

private slots:
    void highlightedSlot( int );
    void activatedSlot( int );
    void beforeShow();
    void trollTech();
    void guideI();
    void guideP();
    void guideY();
    void guideN();
    void guideC();

private:
    void clear();
    void populate();
    void populate( QPopupMenu *, BM_Entry *, int *idCounter );
    void goTo( const char *url );
    void insertGuide();

    BM_Entry         *root;
    QtBrowserContext *browser;
    QtBMEntryDict    *entryDict;
    bool	      dirty;
};

#endif
