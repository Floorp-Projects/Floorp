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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mounir Lamouri <mounir.lamouri@mozilla.com> (original author)
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

#ifndef _nsRadioVisitor_h__
#define _nsRadioVisitor_h__

#include "nsIRadioVisitor.h"

class nsIFormControl;
class nsIDocument;


/**
 * nsRadioVisitor is the base class implementing nsIRadioVisitor and inherited
 * by all radio visitors.
 */
class nsRadioVisitor : public nsIRadioVisitor
{
public:
  nsRadioVisitor() { }
  virtual ~nsRadioVisitor() { }

  NS_DECL_ISUPPORTS

  virtual bool Visit(nsIFormControl* aRadio) = 0;
};

/**
 * The following declarations are radio visitors inheriting from nsRadioVisitor.
 */

/**
 * nsRadioSetCheckedChangedVisitor is calling SetCheckedChanged with the given
 * parameter to all radio elements in the group.
 */
class nsRadioSetCheckedChangedVisitor : public nsRadioVisitor
{
public:
  nsRadioSetCheckedChangedVisitor(bool aCheckedChanged)
    : mCheckedChanged(aCheckedChanged)
    { }

  virtual bool Visit(nsIFormControl* aRadio);

protected:
  bool mCheckedChanged;
};

/**
 * nsRadioGetCheckedChangedVisitor is getting the current checked changed value.
 * Getting it from one radio element is the group is enough given that all
 * elements should have the same value.
 */
class nsRadioGetCheckedChangedVisitor : public nsRadioVisitor
{
public:
  nsRadioGetCheckedChangedVisitor(bool* aCheckedChanged,
                                  nsIFormControl* aExcludeElement)
    : mCheckedChanged(aCheckedChanged)
    , mExcludeElement(aExcludeElement)
    { }

  virtual bool Visit(nsIFormControl* aRadio);

protected:
  bool* mCheckedChanged;
  nsIFormControl* mExcludeElement;
};

/**
 * nsRadioSetValueMissingState is calling SetValueMissingState with the given
 * parameter to all radio elements in the group.
 * It is also calling ContentStatesChanged if needed.
 */
class nsRadioSetValueMissingState : public nsRadioVisitor
{
public:
  nsRadioSetValueMissingState(nsIFormControl* aExcludeElement,
                              bool aValidity, bool aNotify)
    : mExcludeElement(aExcludeElement)
    , mValidity(aValidity)
    , mNotify(aNotify)
    { }

  virtual bool Visit(nsIFormControl* aRadio);

protected:
  nsIFormControl* mExcludeElement;
  bool mValidity;
  bool mNotify;
};

#endif // _nsRadioVisitor_h__

