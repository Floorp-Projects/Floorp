/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* 
   OutlinerDrop.h -- class definition for the outliner drop class
   Created: Chris Toshok <toshok@netscape.com>, 9-Sep-97.
 */



#ifndef _OUTLINER_DROP_H
#define _OUTLINER_DROP_H

#include "DragDrop.h"

class XFE_Outliner;

class XFE_OutlinerDrop : public XFE_DropNetscape
{
public:
  XFE_OutlinerDrop(Widget, XFE_Outliner *);
  virtual ~XFE_OutlinerDrop();

  static Atom _XA_NETSCAPE_OUTLINER_COLUMN;

  static Atom _XA_NETSCAPE_BOOKMARK;
  static Atom _XA_NETSCAPE_MAIL_SERVER;
  static Atom _XA_NETSCAPE_NEWS_SERVER;
  static Atom _XA_NETSCAPE_MAIL_FOLDER;
  static Atom _XA_NETSCAPE_NEWSGROUP;
  static Atom _XA_NETSCAPE_MAIL_MESSAGE;
  static Atom _XA_NETSCAPE_NEWS_MESSAGE;

  static Atom _XA_NETSCAPE_PAB;
  static Atom _XA_NETSCAPE_DIRSERV;


protected:
  virtual void targets();
  virtual void operations();
  virtual int processTargets(Atom *targets, const char **data, int numItems);

  virtual void dragIn();
  virtual void dragOut();
  virtual void dragMotion();

  int _sameDragSource;

private:
  XFE_Outliner *m_outliner;
};

class XFE_OutlinerDrag : public XFE_DragNetscape
{
public:
  XFE_OutlinerDrag(Widget, XFE_Outliner *);
  virtual ~XFE_OutlinerDrag();

protected:
  void buttonPressCb(XButtonPressedEvent *event);

  int dragStart(int,int);
  void targets();
  void operations();
  char *getTargetData(Atom);
  void deleteTarget();
  void dragComplete();

private:
  XFE_Outliner *m_outliner;
  int m_dragcolumn;
  int m_dragrow;

  fe_icon_data *createFakeHeaderIconData(int column);
};

#endif /* _OUTLINER_DROP_H */
