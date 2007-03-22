/* vim:set expandtab ts=4 sw=4 sts=4 cin: */
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
 * The Original Code is mozilla.org unicode stream converter code.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#ifndef NSCONVERTEROUTPUTSTREAM_H_
#define NSCONVERTEROUTPUTSTREAM_H_

#include "nsIOutputStream.h"
#include "nsIConverterOutputStream.h"
#include "nsCOMPtr.h"

class nsIUnicodeEncoder;
class nsIOutputStream;

/* ff8780a5-bbb1-4bc5-8ee7-057e7bc5c925 */
#define NS_CONVERTEROUTPUTSTREAM_CID \
{ 0xff8780a5, 0xbbb1, 0x4bc5, \
  { 0x8e, 0xe7, 0x05, 0x7e, 0x7b, 0xc5, 0xc9, 0x25 } }

class nsConverterOutputStream : public nsIConverterOutputStream {
    public:
        nsConverterOutputStream() {}

        NS_DECL_ISUPPORTS
        NS_DECL_NSIUNICHAROUTPUTSTREAM
        NS_DECL_NSICONVERTEROUTPUTSTREAM

    private:
        ~nsConverterOutputStream();

        nsCOMPtr<nsIUnicodeEncoder> mConverter;
        nsCOMPtr<nsIOutputStream>   mOutStream;
};

#endif
