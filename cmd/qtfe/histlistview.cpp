/*-*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *  $Id: histlistview.cpp,v 1.1 1998/09/25 18:01:34 ramiro%netscape.com Exp $
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
 * First version of this file written 1998 by Jonas Utterstrom 
 * <mailto:jonas.utterstrom@vittran.norrnod.se>
 */

#include "histlistview.h"

HistoryListView::HistoryListView(QWidget *parent, const char* title)
   : QListView(parent, title)
{

}

void HistoryListView::setSorting(int column, bool increasing)
{
   QListView::setSorting(column, increasing);
   emit sortColumnChanged(column, increasing);
}

