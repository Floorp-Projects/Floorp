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

#if defined(ENDER) && defined(MOZ_ENDER_MIME)

#ifdef DEBUG_kin
#define DEBUG_SAFE_LIST 1
#endif /* DEBUG_kin */

#ifdef DEBUG_SAFE_LIST
#include <stdio.h>
#endif /* DEBUG_SAFE_LIST */


#include "editor.h"

class LListElement
{
  LListElement *m_pPrev;
  LListElement *m_pNext;

protected:
  void *m_pData;

public:

  LListElement(void *data = 0)
			   : m_pPrev(0), m_pNext(0), m_pData(data) {}

  virtual ~LListElement();

  int32 insertBefore(LListElement *ele);
  int32 insertAfter(LListElement *ele);

  int32 unlink();

  void *getData()           { return m_pData; }
  LListElement *getPrev()   { return m_pPrev; }
  LListElement *getNext()   { return m_pNext; }

#ifdef DEBUG_SAFE_LIST
  virtual void print();
#endif /* DEBUG_SAFE_LIST */
};

class LList
{
  LListElement *m_pHead;
  LListElement *m_pTail;

public:

  LList() : m_pHead(0), m_pTail(0) {}
  virtual ~LList();

  LListElement *getHead() { return m_pHead; }
  LListElement *getTail() { return m_pTail; }

  int32 insertAtHead(LListElement *ele);
  int32 insertAtTail(LListElement *ele);
  int32 insertBefore(LListElement *ele1, LListElement *ele2);
  int32 insertAfter(LListElement *ele1, LListElement*ele2);

  XP_Bool isEmpty();

  XP_Bool contains(LListElement *ele);

  int32 remove(LListElement *ele);

#ifdef DEBUG_SAFE_LIST
  virtual void print();
#endif /* DEBUG_SAFE_LIST */
};

class LListIterator
{
  LList *m_pList;
  LListElement *m_pNextElement;

public:

  LListIterator(LList *list = 0) : m_pList(list), m_pNextElement(0) { init(); }

  virtual int32 init()
  {
    if (!m_pList)
      return 0;

    m_pNextElement = m_pList->getHead();

    return 0;
  }

  virtual LListElement *getNext()
  {
    LListElement *ele = m_pNextElement;

    if (m_pNextElement)
      m_pNextElement = m_pNextElement->getNext();

    return ele;
  }
};

class SortedLList : public LList
{
public:

  virtual int32 insert(LListElement *ele);
  virtual int32 compare(LListElement *ele1, LListElement *ele2);
};

LListElement::~LListElement()
{
  // If we're destroying this element, make sure it's
  // not on any list!
  unlink();
}

//
// Insert ele before this
//
int32
LListElement::insertBefore(LListElement *ele)
{
  if (!ele)
    return -1;

  ele->m_pNext = this;
  ele->m_pPrev = m_pPrev;
  m_pPrev      = ele;

  if (ele->m_pPrev)
    ele->m_pPrev->m_pNext = ele;

  return 0;
}

//
// Insert ele after this.
//
int32
LListElement::insertAfter(LListElement *ele)
{
  if (!ele)
    return -1;

  ele->m_pPrev = this;
  ele->m_pNext = m_pNext;
  m_pNext      = ele;

  if (ele->m_pNext)
    ele->m_pNext->m_pPrev = ele;

  return 0;
}

//
// unlink this from it's list.
//
int32
LListElement::unlink()
{
  if (m_pPrev)
    m_pPrev->m_pNext = m_pNext;

  if (m_pNext)
    m_pNext->m_pPrev = m_pPrev;

  m_pPrev = 0;
  m_pNext = 0;

  return 0;
}

#ifdef DEBUG_SAFE_LIST

void
LListElement::print()
{
  fprintf(stdout, "this:         0x%.8x\n", this);
  fprintf(stdout, "    m_pData:  0x%.8x\n", m_pData);
  fprintf(stdout, "    m_pPrev:  0x%.8x\n", m_pPrev);
  fprintf(stdout, "    m_pNext:  0x%.8x\n", m_pNext);
}

#endif /* DEBUG_SAFE_LIST */


LList::~LList()
{
  LListElement *ele;

  while((ele = m_pHead))
  {
    remove(ele);
    delete ele;
  }
}

//
// Insert ele at the head of the list.
//
int32
LList::insertAtHead(LListElement *ele)
{
  if (!ele)
    return -1;

  if (!m_pHead)
  {
    m_pHead = m_pTail = ele;
    return 0;
  }

  m_pHead->insertBefore(ele);
  m_pHead = ele;

  return 0;
}

//
// Insert ele at the tail of the list.
//
int32
LList::insertAtTail(LListElement *ele)
{
  if (!ele)
    return -1;

  if (!m_pHead)
  {
    m_pHead = m_pTail = ele;
    return 0;
  }

  m_pTail->insertAfter(ele);
  m_pTail = ele;

  return 0;
}

//
// Insert ele2 before ele1 in the list.
//
int32
LList::insertBefore(LListElement *ele1, LListElement *ele2)
{
  if (!ele1 || !ele2 || !contains(ele1))
    return -1;

  if (m_pHead == ele1)
  {
    insertAtHead(ele2);
    return 0;
  }

  ele1->insertBefore(ele2);

  return 0;
}

//
// Insert ele2 after ele1 in the list.
//
int32
LList::insertAfter(LListElement *ele1, LListElement *ele2)
{
  if (!ele1 || !ele2 || !contains(ele1))
    return -1;

  if (m_pTail == ele1)
  {
    insertAtTail(ele2);
    return 0;
  }

  ele1->insertAfter(ele2);

  return 0;
}

//
// TRUE if the list contains no elements.
//
XP_Bool
LList::isEmpty()
{
  return !m_pHead;
}

//
// TRUE if this contains ele.
//
XP_Bool
LList::contains(LListElement *ele)
{
  LListElement *e = m_pHead;

  if (!ele)
    return FALSE;

  while (e)
  {
    if (e == ele)
      return TRUE;
    e = e->getNext();
  }

  return FALSE;
}

//
// Remove ele from the list.
//
int32
LList::remove(LListElement *ele)
{
  if (!ele || !contains(ele))
    return -1;

  if (!m_pHead)
    return 0;

  if (ele == m_pHead)
    m_pHead = ele->getNext();

  if (ele == m_pTail)
    m_pTail = ele->getPrev();

  ele->unlink();

  return 0;
}

#ifdef DEBUG_SAFE_LIST

void
LList::print()
{
  LListIterator li(this);
  LListElement *ele;

  fprintf(stdout, "-------------------------------\n");
  fprintf(stdout, "HEAD:         0x%.8x\n", m_pHead);
  fprintf(stdout, "TAIL:         0x%.8x\n", m_pTail);

  while ((ele = li.getNext()))
  {
    ele->print();
    fflush(stdout);
  }
}

#endif /* DEBUG_SAFE_LIST */

int32
SortedLList::insert(LListElement *ele)
{
  LListIterator li(this);
  LListElement *le;

  if (contains(ele))
    return 0;

  if (isEmpty())
  {
    LList::insertAtHead(ele);
    return 0;
  }

  while ((le = li.getNext()))
  {
    if (compare(le, ele) >= 0)
      return(LList::insertBefore(le, ele));
  }

  return(LList::insertAtTail(ele));
}

int32
SortedLList::compare(LListElement *ele1, LListElement *ele2)
{
  if (ele1->getData() < ele2->getData())
    return -1;
  else if (ele1->getData() > ele2->getData())
    return 1;

  return 0;
}

class SafeListElement : public LListElement
{
public:
  void *m_pID;
  char *m_pTmpFilePath;
  int32 m_iRefCount;

  SafeListElement(void *id, const char *url_str, const char *path=0)
                 : LListElement(), m_pTmpFilePath(0)
  {
    if (url_str)
      m_pData = (void *)XP_STRDUP(url_str);

    if (path)
      m_pTmpFilePath = XP_STRDUP(path);

    m_pID       = id;
    m_iRefCount = 1;
  }

  virtual ~SafeListElement()
  {
    if (m_pTmpFilePath)
    {
      XP_FileRemove(m_pTmpFilePath, xpFileToPost);
      XP_FREE(m_pTmpFilePath);
    }

    if (m_pData)
      XP_FREE(m_pData);

    m_pData        = 0;
    m_pID          = 0;
    m_pTmpFilePath = 0;
  }

  XP_Bool match(void *id, const char *url_str)
  {
    if (m_pID != id || (m_pData != (void *)url_str && (!url_str || !m_pData)))
      return(FALSE);

    return(!XP_STRCMP((char *)m_pData, url_str));
  }

#ifdef DEBUG_SAFE_LIST
  void print()
  {
    fprintf(stdout, "this:         0x%.8x\n", this);
    fprintf(stdout, "    id:       0x%.8x\n", m_pID);
    fprintf(stdout, "    url_str:  %s\n", (char *)m_pData);
    fprintf(stdout, "    path:     %s\n", m_pTmpFilePath ? m_pTmpFilePath : "0");
    fprintf(stdout, "    count:    %d\n", m_iRefCount);
    fprintf(stdout, "    prev:     0x%.8x\n", getPrev());
    fprintf(stdout, "    next:     0x%.8x\n", getNext());
  }
#endif /* DEBUG_SAFE_LIST */
};

class SafeList : public SortedLList
{
public:

  virtual ~SafeList()
  {
    removeAll();
  }

  SafeListElement *find(void *id, const char *url_str)
  {
    LListIterator li(this);
    SafeListElement *ele;

    while ((ele = (SafeListElement *)li.getNext()))
    {
      if (ele->match(id, url_str))
        return ele;
    }

    return 0;
  }

  int32 insert(void *id, const char *url_str, const char *path = 0)
  {
    SafeListElement *ele = find(id, url_str);

    if (ele)
    {
      ele->m_iRefCount++;
      return 0;
    }

    ele = new SafeListElement(id, url_str, path);

    if (!ele)
      return -1;

    return SortedLList::insert((LListElement *)ele);
  }

  int32 remove(void *id, const char *url_str)
  {
    SafeListElement *ele = find(id, url_str);

    if (!ele)
      return 1;

    ele->m_iRefCount--;

    if (ele->m_iRefCount <= 0)
    {
      LList::remove((LListElement *)ele);
      delete ele;
    }

    return 0;
  }

  int32 removeID(void *id)
  {
    LListIterator li(this);
    SafeListElement *ele;

    while ((ele = (SafeListElement *)li.getNext()))
    {
      if (ele->m_pID == id)
      {
        LList::remove((LListElement *)ele);
        delete ele;
      }
    }

    return 0;
  }

  int32 removeAll()
  {
    LListIterator li(this);
    LListElement *ele;

    while ((ele = li.getNext()))
    {
      LList::remove(ele);
      delete ele;
    }

    return 0;
  }

  int32 compare(LListElement *ele1, LListElement *ele2)
  {
    SafeListElement *e1 = (SafeListElement *)ele1;
    SafeListElement *e2 = (SafeListElement *)ele2;

    char *d1 = (char *)e1->getData();
    char *d2 = (char *)e2->getData();

    if (e1->m_pID < e2->m_pID)
      return -1;
    else if (e1->m_pID > e2->m_pID)
      return 1;
    else if (!d1)
      return -1;
    else if (!d2)
      return 1;

    return XP_STRCMP(d1, d2);
  }
};

static SafeList *app_safe_list = 0;

extern "C" XP_Bool
EDT_URLOnSafeList( void *id, const char *url_str )
{
  return(app_safe_list &&
         app_safe_list->find(id, url_str) != 0);
}

extern "C" int32
EDT_AddTmpFileURLToSafeList( void *id, const char *url_str, const char *path )
{
  int32 status = 0;

  if (!app_safe_list)
  {
    app_safe_list = new SafeList();
    if (!app_safe_list)
      return -1;
  }

  status = app_safe_list->insert(id, url_str, path);

#ifdef DEBUG_SAFE_LIST
  app_safe_list->print();
#endif /* DEBUG_SAFE_LIST */

  return status;
}

extern "C" int32
EDT_AddURLToSafeList( void *id, const char *url_str )
{
  return EDT_AddTmpFileURLToSafeList(id, url_str, 0);
}

extern "C" int32
EDT_RemoveURLFromSafeList( void *id, const char *url_str )
{
  int32 status = 0;

  if (!app_safe_list)
      return 0;

  status = app_safe_list->remove(id, url_str);

#ifdef DEBUG_SAFE_LIST
  app_safe_list->print();
#endif /* DEBUG_SAFE_LIST */

  return status;
}

extern "C" int32
EDT_RemoveIDFromSafeList( void *id )
{
  int32 status = 0;

  if (!app_safe_list)
    return 0;

  status = app_safe_list->removeID(id);

#ifdef DEBUG_SAFE_LIST
  app_safe_list->print();
#endif /* DEBUG_SAFE_LIST */

  return status;
}

extern "C" int32
EDT_DestroySafeList()
{
  if (!app_safe_list)
    return 0;

  delete app_safe_list;
  app_safe_list = 0;

  return 0;
}

extern "C" void *
EDT_GetIdFromContext(MWContext *pContext)
{
  GET_EDIT_BUF_OR_RETURN(pContext, pEditBuffer) 0;
  return pEditBuffer->m_pEmbeddedData;
}

#endif /* ENDER && MOZ_ENDER_MIME */
