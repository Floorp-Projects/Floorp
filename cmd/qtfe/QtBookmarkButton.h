/* $Id: QtBookmarkButton.h,v 1.1 1998/09/25 18:01:27 ramiro%netscape.com Exp $
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

#ifndef QTBOOKMARKBUTTON_H
#define QTBOOKMARKBUTTON_H

#include "qtoolbutton.h"

class QtBookmarkMenu;
class QtContext;

class QtBookmarkButton : public QToolButton
{
    Q_OBJECT
public:
    QtBookmarkButton( const QPixmap & pm, const char * textLabel,
		      QtContext *context,
		      QToolBar * parent, const char * name = 0 );
    ~QtBookmarkButton();

public slots:
    void beenToggled( bool );
    void menuDied();

protected:
    bool eventFilter( QObject *o, QEvent *e );
private:
    QtBookmarkMenu *menu;
};


#endif
