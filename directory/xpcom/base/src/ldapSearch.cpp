/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the mozilla.org LDAP XPCOM component.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Dan Mosedale <dmose@mozilla.org>n
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include "ldap.h"
#include "nsLDAPConnection.h"
#include "nsLDAPOperation.h"
#include "nsLDAPMessage.h"
#include "nsLDAPChannel.h"

void
lds(class nsLDAPChannel *chan, const char *url)
{
  class nsLDAPConnection *myConnection;
  class nsLDAPMessage *myMessage;
  class nsLDAPOperation *myOperation;
  struct timeval nullTimeval = {0,0};
  // struct timeval timeout = {10,0};
  int returnCode;
  char *errString;
  // url = "ldap://memberdir.netscape.com:389/ou=member_directory,
  //        o=netcenter.com??sub?(sn=Mosedale)"
  myConnection = new nsLDAPConnection();
  if ( !myConnection->Init("nsdirectory.netscape.com", LDAP_PORT) ) {
    fprintf(stderr, "main: nsLDAPConnection::Init failed\n");
    exit(-1);
  }

  myOperation = new nsLDAPOperation(myConnection);

#ifdef NO_URL_SEARCH
  fprintf(stderr, "starting bind\n");
  fflush(stderr);
  
  if ( !myOperation->SimpleBind(NULL, NULL) ) {
    char *errstring = myConnection->GetErrorString();
    fprintf(stderr, "ldap_simple_bind: %s\n", errstring);
    exit(-1);
  }

  fprintf(stderr, "waiting for bind to complete");

  myMessage = new nsLDAPMessage(myOperation);
  returnCode = myOperation->Result(0, &nullTimeval, myMessage);

  while (returnCode == 0 ) {
    returnCode =   
      myOperation->Result(0, &nullTimeval, myMessage);
    usleep(20);
    fputc('.',stderr);
  }

  switch (returnCode) {
  case LDAP_RES_BIND:
    // success
    break;
  case -1:
    errString = myConnection->GetErrorString();
    fprintf(stderr, 
	    "myOperation->Result() [myOperation->SimpleBind]: %s: errno=%d\n",
	    errString, errno);
    ldap_memfree(errString); 
    exit(-1);
    break;
  default:
    fprintf(stderr, "\nmyOperation->Result() returned unexpected value: %d", 
	    returnCode);
    exit(-1);
  }

  fprintf(stderr, "bound\n");

  delete myMessage;
  delete myOperation;

#endif 

  // start search
  
  fprintf(stderr, "starting search\n");
  fflush(stderr);
  myOperation = new nsLDAPOperation(myConnection);

  // XXX what about timeouts?
  //
  returnCode = myOperation->URLSearch(url, 0);
  if (returnCode == -1) {
    exit(-1);
  }

  // poll for results
  //
  fprintf(stderr, "polling search operation");
  returnCode = LDAP_SUCCESS;
  while ( returnCode != LDAP_RES_SEARCH_RESULT ) {

      char *dn, *attr;
      int rc2;

      fputc('.', stderr);

      myMessage = new nsLDAPMessage(myOperation);
      returnCode = myOperation->Result(LDAP_MSG_ONE, &nullTimeval, 
				       myMessage);

      switch (returnCode) {
      case -1: // something went wrong
	  errString = myConnection->GetErrorString();
	  fprintf(stderr, 
		  "\nmyOperation->Result() [URLSearch]: %s: errno=%d\n",
		  errString, errno);
	  ldap_memfree(errString); 
	  exit(-1);
      case 0: // nothing's been returned yet
	  break; 	

      case LDAP_RES_SEARCH_ENTRY:
	  fprintf(stderr, "\nentry returned!\n");

	  // get the DN
	  dn = myMessage->GetDN();
	  if (!dn) {
	      fprintf(stderr, "myMessage->GetDN(): %s\n", 
		      myConnection->GetErrorString());
	      exit(-1);
	  }
	  chan->pipeWrite("dn: ");
	  chan->pipeWrite(dn);
	  chan->pipeWrite("\n");

	  ldap_memfree(dn);

	  // fetch the attributesget the first attribute
	  for (attr = myMessage->FirstAttribute();
	       attr != NULL;
	       attr = myMessage->NextAttribute()) {

	      int i;
	      char **vals;

	      // get the values of this attribute
	      vals = myMessage->GetValues(attr);
	      if (vals == NULL) {
		  fprintf(stderr, "myMessage->GetValues: %s\n", 
			  myConnection->GetErrorString());
		  exit(-1);
	      }

	      // print all values of this attribute
	      for ( i=0 ; vals[i] != NULL; i++ ) {
		chan->pipeWrite(attr);
		chan->pipeWrite(": ");
		chan->pipeWrite(vals[i]);
		chan->pipeWrite("\n");
	      }

	      ldap_value_free(vals);
	      ldap_memfree(attr);
	  }
	  
	  // did we reach this statement because of an error?
	  if ( myConnection->GetLdErrno(NULL, NULL) != LDAP_SUCCESS ) {
	      fprintf(stderr, "myMessage: error getting attribute: %s\n", 
		      myConnection->GetErrorString());	
	      exit(-1);
	  }

	  // separate this entry from the next
	  chan->pipeWrite("\n");

	  // continue polling
	  fprintf(stderr, "polling search operation");
	  break;

      case LDAP_RES_SEARCH_REFERENCE: // referral
	  fprintf(stderr, 
		  "LDAP_RES_SEARCH_REFERENCE returned; not implemented!");
	  exit(-1);
	  break;

      case LDAP_RES_SEARCH_RESULT: // all done (the while condition sees this)
	  fprintf(stderr, "\nresult returned: \n");

	  rc2 = myMessage->GetErrorCode();
	  if ( rc2 != LDAP_SUCCESS ) {
	      fprintf(stderr, " %s\n", 
		      ldap_err2string(rc2));
	      exit(-1);
	  }
	  fprintf(stderr, "success\n");
	  break;
	  
      default:
	  fprintf(stderr,"unexpected result returned");
	  exit(-1);
	  break;
      }

      delete myMessage;

      usleep(200);
  }

  fprintf(stderr,"unbinding\n");
  delete myConnection;	
  fprintf(stderr,"unbound\n");
}
