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
 * The Original Code is Mozilla Spellchecker Component.
 *
 * The Initial Developer of the Original Code is
 * David Einstein.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): David Einstein Deinst@world.std.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "mozSpellI18NManager.h"
#include "mozEnglishWordUtils.h"
#include "mozGenericWordUtils.h"
#include "nsString.h"

NS_IMPL_ISUPPORTS1(mozSpellI18NManager, mozISpellI18NManager)

mozSpellI18NManager::mozSpellI18NManager()
{
  /* member initializers and constructor code */
}

mozSpellI18NManager::~mozSpellI18NManager()
{
  /* destructor code */
}

/* mozISpellI18NUtil GetUtil (in wstring language); */
NS_IMETHODIMP mozSpellI18NManager::GetUtil(const PRUnichar *aLanguage, mozISpellI18NUtil **_retval)
{
 if( NULL == _retval) {
   return NS_ERROR_NULL_POINTER;
 }
 *_retval = NULL;
 nsAutoString lang;
 lang.Assign(aLanguage);
 if(lang.EqualsLiteral("en")){
   *_retval = new mozEnglishWordUtils;
 }
 else{
   *_retval = new mozEnglishWordUtils;   
 }

 NS_IF_ADDREF(*_retval);
 return NS_OK;
}
