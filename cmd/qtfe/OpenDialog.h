/* $Id: OpenDialog.h,v 1.2 1998/09/25 23:30:38 cls%seawood.org Exp $
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
 * The Initial Developer of this code under the NPL is Kalle
 * Dalheimer.  Further developed by Warwick Allison, Kalle Dalheimer,
 * Eirik Eng, Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and
 * Paul Olav Tvete.  Copyright (C) 1998 Warwick Allison, Kalle
 * Dalheimer, Eirik Eng, Matthias Ettrich, Arnt Gulbrandsen, Haavard
 * Nord and Paul Olav Tvete.  All Rights Reserved.
 */

#ifndef _OPENDIALOG_H
#define _OPENDIALOG_H

#include <qdialog.h>

class QtBrowserContext;
class QLineEdit;

class OpenDialog : public QDialog
{
    Q_OBJECT
public:
    OpenDialog( QtBrowserContext* context, QWidget* parent, const char* name = 0 );

private slots:
    void chooseFile();
    void openInComposer();
    void openInNavigator();
    void clear();

private:
    QLineEdit* edit;
    QtBrowserContext* context;
};


#endif
