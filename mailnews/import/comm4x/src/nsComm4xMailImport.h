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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Srilatha Moturi <srilatha@netscape.com>
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

#ifndef nsComm4xMailImport_h___
#define nsComm4xMailImport_h___

#include "nsIImportModule.h"
#include "nsCOMPtr.h"
#include "nsIStringBundle.h"
#include "nsIImportMail.h"
#include "nsISupportsArray.h"
#include "nsIFileSpec.h"
#include "nsComm4xMail.h"

//{7792e9e0-412a-11d6-92bf-0010a4b26cda}
#define NS_COMM4XMAILIMPL_CID \
{ 0x7792e9e0, 0x412a, 0x11d6, { 0x92, 0xbf, 0x0, 0x10, 0xa4, 0xb2, 0x6c, 0xda } }
#define NS_COMM4XMAILIMPL_CONTRACTID "@mozilla.org/import/import-comm4xMailImpl;1" 
// {647cc990-2bdb-11d6-92a0-0010a4b26cda}
#define NS_COMM4XMAILIMPORT_CID                 \
{ 0x647ff990, 0x2bdb, 0x11d6, { 0x92, 0xa0, 0x0, 0x10, 0xa4, 0xb2, 0x6c, 0xda } }

#define kComm4xMailSupportsString NS_IMPORT_MAIL_STR

class nsComm4xMailImport : public nsIImportModule
{
public:

    nsComm4xMailImport();
    virtual ~nsComm4xMailImport();
    
    NS_DECL_ISUPPORTS

    ////////////////////////////////////////////////////////////////////////////////////////
    // we suppport the nsIImportModule interface 
    ////////////////////////////////////////////////////////////////////////////////////////

    NS_DECL_NSIIMPORTMODULE

protected:
     nsCOMPtr <nsIStringBundle>  m_pBundle;
};

class ImportComm4xMailImpl : public nsIImportMail
{
public:
    ImportComm4xMailImpl();
    virtual ~ImportComm4xMailImpl();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIImportMail interface
    
    /* void GetDefaultLocation (out nsIFileSpec location, out boolean found, out boolean userVerify); */
    NS_IMETHOD GetDefaultLocation(nsIFileSpec **location, PRBool *found, PRBool *userVerify);

    /* nsISupportsArray FindMailboxes (in nsIFileSpec location); */
    NS_IMETHOD FindMailboxes(nsIFileSpec *location, nsISupportsArray **_retval);

    /* void ImportMailbox (in nsIImportMailboxDescriptor source, in nsIFileSpec destination, out boolean fatalError); */
    NS_IMETHOD ImportMailbox(nsIImportMailboxDescriptor *source, nsIFileSpec *destination, 
                             PRUnichar **pErrorLog, PRUnichar **pSuccessLog, PRBool *fatalError);

    /* unsigned long GetImportProgress (); */
    NS_IMETHOD GetImportProgress(PRUint32 *_retval);

    NS_IMETHOD TranslateFolderName(const nsAString & aFolderName, nsAString & _retval);

public:
    static void SetLogs(nsString& success, nsString& error, PRUnichar **pError, PRUnichar **pSuccess);
    void ReportStatus(PRInt32 errorNum, nsString& name, nsString *pStream);
    nsresult Initialize();

private:
    nsComm4xMail  m_mail;
    PRUint32      m_bytesDone;
    nsCOMPtr<nsIStringBundle>  m_pBundleProxy;
};


#endif /* nsComm4xMailImport_h___ */
