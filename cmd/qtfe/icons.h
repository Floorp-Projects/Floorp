/* $Id: icons.h,v 1.1 1998/09/25 18:01:34 ramiro%netscape.com Exp $
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

#ifndef ICONS_H
#define ICONS_H

#include <qiconset.h>
#include <qmovie.h>


class Icon: public QIconSet
{
public:
    Icon( const char * );
};


class Pixmap: public QPixmap
{
public:
    Pixmap( const char * );
    Pixmap( int ); //jint???
};


class Movie: public QMovie
{
public:
    Movie( const char * );
};

#endif
