/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 tw=80 et cindent: */
/*
 * ***** BEGIN LICENSE BLOCK *****
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
 * timeless <timeless@mozdev.org>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef __EmbedFilePicker_h
#define __EmbedFilePicker_h

#include "nsIFilePicker.h"
#include "nsISupports.h"
#include "nsNetCID.h"

#include "gtkmozembed.h"
#include "gtkmozembed_common.h"
#include "EmbedPrivate.h"

#define EMBED_FILEPICKER_CID           \
{ /* f097d33b-1c97-48a6-af4c-07022857eb7c */         \
    0xf097d33b,                                      \
    0x1c97,                                          \
    0x48a6,                                          \
    {0xaf, 0x4c, 0x07, 0x02, 0x28, 0x57, 0xeb, 0x7c} \
}

#define EMBED_FILEPICKER_CONTRACTID  "@mozilla.org/filepicker;1"
#define EMBED_FILEPICKER_CLASSNAME  "File Picker Implementation"

class EmbedFilePicker : public nsIFilePicker
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFILEPICKER
  EmbedFilePicker();

protected:
  nsIDOMWindow *mParent;
  PRInt16 mMode;
  nsCString mFilename;

private:
  ~EmbedFilePicker();
};
#endif
