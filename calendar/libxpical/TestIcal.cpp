/* 
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
 * The Original Code is OEone Corporation.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by OEone Corporation are Copyright (C) 2001
 * OEone Corporation. All Rights Reserved.
 *
 * Contributor(s): Mostafa Hosseini (mostafah@oeone.com)
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
 * 
*/

#include <unistd.h>
#include <oeIICal.h>
#include <oeICalImpl.h>
#include <oeICalEventImpl.h>
#include <nsIServiceManager.h>
//#include <nsXPIDLString.h>


main()
{
    nsresult rv;
    char buf[80];

    // Initialize XPCOM
    rv = NS_InitXPCOM(nsnull, nsnull);
    if (NS_FAILED(rv))
    {
        printf("ERROR: XPCOM intialization error [%x].\n", rv);
        return -1;
    }

    // Do Autoreg to make sure our component is registered. The real way of
    // doing this is running the xpcom registraion tool, regxpcom, at install
    // time to get components registered and not make this call everytime.
    // Ignore return value.
    //
    // Here we use the global component manager. Note that this will cause
    // linkage dependency to XPCOM library. We feel that linkage dependency
    // to XPCOM is inevitable and this is simpler to code.
    // To break free from such dependencies, we can GetService() the component
    // manager from the service manager that is returned from NS_InitXPCOM().
    // We feel that linkage dependency to XPCOM library is inevitable.
    (void) nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, nsnull);

    // Create an instance of our component
    nsCOMPtr<oeIICal> mysample = do_CreateInstance(OE_ICAL_CONTRACTID, &rv);
    if (NS_FAILED(rv))
    {
        printf("ERROR: Cannot create instance of component " OE_ICAL_CONTRACTID " [%x].\n"
               "Debugging hint:\n"
               "\tsetenv NSPR_LOG_MODULES nsComponentManager:5\n"
               "\tsetenv NSPR_LOG_FILE xpcom.log\n"
               "\t./TestICal\n"
               "\t<check the contents for xpcom.log for possible cause of error>.\n",
               rv);
        return -2;
    }

    mysample->SetServer( "/home/mostafah/calendar" );
    rv = mysample->Test();

    if ( rv != NS_OK )
    {
        printf("ERROR: Calling oeIICal::Test()\n");
        return 0;
    }

    printf("Test finished\n");

    // Shutdown XPCOM
    NS_ShutdownXPCOM(nsnull);
    return 0;
}
