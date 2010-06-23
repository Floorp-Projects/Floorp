/******* BEGIN LICENSE BLOCK *******
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
 * The Initial Developer of the Original Code is Björn Jacke. Portions created
 * by the Initial Developers are Copyright (C) 2000-2007 the Initial
 * Developers. All Rights Reserved.
 * 
 * Contributor(s): Björn Jacke (bjoern.jacke@gmx.de)
 *                 László Németh (nemethl@gyorsposta.hu)
 *                 Caolan McNamara (caolanm@redhat.com)
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
 * Changelog:
 *  2000-01-05  Björn Jacke <bjoern.jacke AT gmx.de>
 *              Initial Release insprired by the article about phonetic
 *              transformations out of c't 25/1999
 *
 *  2007-07-20  Björn Jacke <bjoern.jacke AT gmx.de>
 *              Released under MPL/GPL/LGPL tri-license for Hunspell
 *
 *  2007-08-22  László Németh <nemeth at OOo>
 *              Porting from Aspell to Hunspell by little modifications
 *
 ******* END LICENSE BLOCK *******/

#ifndef __PHONETHXX__
#define __PHONETHXX__

#define HASHSIZE          256
#define MAXPHONETLEN      256
#define MAXPHONETUTF8LEN  (MAXPHONETLEN * 4)

#include "hunvisapi.h"

struct phonetable {
  char utf8;
  cs_info * lang;
  int num;
  char * * rules;
  int hash[HASHSIZE];
};

LIBHUNSPELL_DLL_EXPORTED void init_phonet_hash(phonetable & parms);

LIBHUNSPELL_DLL_EXPORTED int phonet (const char * inword, char * target,
              int len, phonetable & phone);

#endif
