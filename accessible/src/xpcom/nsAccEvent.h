/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

