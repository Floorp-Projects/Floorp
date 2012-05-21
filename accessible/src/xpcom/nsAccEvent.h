/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsAccEvent_H_
#define _nsAccEvent_H_

#include "nsIAccessibleEvent.h"

#include "AccEvent.h"

/**
 * Generic accessible event.
 */
class nsAccEvent: public nsIAccessibleEvent
{
public:
  nsAccEvent(AccEvent* aEvent) : mEvent(aEvent) { }
  virtual ~nsAccEvent() { }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIACCESSIBLEEVENT

protected:
  nsRefPtr<AccEvent> mEvent;

private:
  nsAccEvent();
  nsAccEvent(const nsAccEvent&);
  nsAccEvent& operator =(const nsAccEvent&);
};


/**
 * Accessible state change event.
 */
class nsAccStateChangeEvent: public nsAccEvent,
                             public nsIAccessibleStateChangeEvent
{
public:
  nsAccStateChangeEvent(AccStateChangeEvent* aEvent) : nsAccEvent(aEvent) { }
  virtual ~nsAccStateChangeEvent() { }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLESTATECHANGEEVENT

private:
  nsAccStateChangeEvent();
  nsAccStateChangeEvent(const nsAccStateChangeEvent&);
  nsAccStateChangeEvent& operator =(const nsAccStateChangeEvent&);
};


/**
 * Accessible text change event.
 */
class nsAccTextChangeEvent: public nsAccEvent,
                            public nsIAccessibleTextChangeEvent
{
public:
  nsAccTextChangeEvent(AccTextChangeEvent* aEvent) : nsAccEvent(aEvent) { }
  virtual ~nsAccTextChangeEvent() { }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLETEXTCHANGEEVENT

private:
  nsAccTextChangeEvent();
  nsAccTextChangeEvent(const nsAccTextChangeEvent&);
  nsAccTextChangeEvent& operator =(const nsAccTextChangeEvent&);
};


/**
 * Accessible hide event.
 */
class nsAccHideEvent : public nsAccEvent,
                       public nsIAccessibleHideEvent
{
public:
  nsAccHideEvent(AccHideEvent* aEvent) : nsAccEvent(aEvent) { }
  virtual ~nsAccHideEvent() { }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLEHIDEEVENT

private:
  nsAccHideEvent() MOZ_DELETE;
  nsAccHideEvent(const nsAccHideEvent&) MOZ_DELETE;
  nsAccHideEvent& operator =(const nsAccHideEvent&) MOZ_DELETE;
};


/**
 * Accessible caret move event.
 */
class nsAccCaretMoveEvent: public nsAccEvent,
                           public nsIAccessibleCaretMoveEvent
{
public:
  nsAccCaretMoveEvent(AccCaretMoveEvent* aEvent) : nsAccEvent(aEvent) { }
  virtual ~nsAccCaretMoveEvent() { }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLECARETMOVEEVENT

private:
  nsAccCaretMoveEvent();
  nsAccCaretMoveEvent(const nsAccCaretMoveEvent&);
  nsAccCaretMoveEvent& operator =(const nsAccCaretMoveEvent&);
};


/**
 * Accessible table change event.
 */
class nsAccTableChangeEvent : public nsAccEvent,
                              public nsIAccessibleTableChangeEvent
{
public:
  nsAccTableChangeEvent(AccTableChangeEvent* aEvent) : nsAccEvent(aEvent) { }
  virtual ~nsAccTableChangeEvent() { }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLETABLECHANGEEVENT

private:
  nsAccTableChangeEvent();
  nsAccTableChangeEvent(const nsAccTableChangeEvent&);
  nsAccTableChangeEvent& operator =(const nsAccTableChangeEvent&);
};

/**
 * Accessible virtual cursor change event.
 */
class nsAccVirtualCursorChangeEvent : public nsAccEvent,
                                      public nsIAccessibleVirtualCursorChangeEvent
{
public:
  nsAccVirtualCursorChangeEvent(AccVCChangeEvent* aEvent) :
    nsAccEvent(aEvent) { }
  virtual ~nsAccVirtualCursorChangeEvent() { }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLEVIRTUALCURSORCHANGEEVENT

private:
  nsAccVirtualCursorChangeEvent() MOZ_DELETE;
  nsAccVirtualCursorChangeEvent(const nsAccVirtualCursorChangeEvent&) MOZ_DELETE;
  nsAccVirtualCursorChangeEvent& operator =(const nsAccVirtualCursorChangeEvent&) MOZ_DELETE;
};

#endif

