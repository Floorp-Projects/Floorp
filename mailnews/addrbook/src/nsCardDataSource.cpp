/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCardDataSource.h"
#include "nsAbBaseCID.h"
#include "nsIAbCard.h"
#include "nsIAddrBookSession.h"

#include "rdf.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIRDFNode.h"
#include "nsEnumeratorUtils.h"
#include "nsIServiceManager.h"
#include "prprf.h"	 
#include "prlog.h"	 

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"


static NS_DEFINE_CID(kRDFServiceCID,  NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kAbCardDataSourceCID, NS_ABCARDDATASOURCE_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);

nsIRDFResource* nsAbCardDataSource::kNC_DisplayName = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_Name = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_Nickname = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_PrimaryEmail = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_SecondEmail = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_WorkPhone = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_HomePhone = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_Fax = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_Pager = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_Cellular = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_Title = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_Department = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_Organization = nsnull;

//for locale sorting,
nsIRDFResource* nsAbCardDataSource::kNC_DisplayNameCollation = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_NameCollation = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_NicknameCollation = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_PrimaryEmailCollation = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_SecondEmailCollation = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_WorkPhoneCollation = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_HomePhoneCollation = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_FaxCollation = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_PagerCollation = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_CellularCollation = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_TitleCollation = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_DepartmentCollation = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_CompanyCollation = nsnull;

// commands
nsIRDFResource* nsAbCardDataSource::kNC_Delete = nsnull;
nsIRDFResource* nsAbCardDataSource::kNC_NewCard = nsnull;


#define NC_RDF_DISPLAYNAME			"http://home.netscape.com/NC-rdf#DisplayName"
#define NC_RDF_NAME					"http://home.netscape.com/NC-rdf#Name"
#define NC_RDF_NICKNAME				"http://home.netscape.com/NC-rdf#Nickname"
#define NC_RDF_PRIMARYEMAIL			"http://home.netscape.com/NC-rdf#PrimaryEmail"
#define NC_RDF_SECONDEMAIL			"http://home.netscape.com/NC-rdf#SecondEmail"
#define NC_RDF_WORKPHONE			"http://home.netscape.com/NC-rdf#WorkPhone"
#define NC_RDF_HOMEPHONE			"http://home.netscape.com/NC-rdf#HomePhone"
#define NC_RDF_FAX					"http://home.netscape.com/NC-rdf#Fax"
#define NC_RDF_PAGER				"http://home.netscape.com/NC-rdf#Pager"
#define NC_RDF_CELLULAR				"http://home.netscape.com/NC-rdf#Cellular"
#define NC_RDF_TITLE				"http://home.netscape.com/NC-rdf#Title"
#define NC_RDF_DEPARTMENT			"http://home.netscape.com/NC-rdf#Department"
#define NC_RDF_ORGANIZATION			"http://home.netscape.com/NC-rdf#Company"

#define NC_RDF_DISPLAYNAME_SORT		"http://home.netscape.com/NC-rdf#DisplayName?collation=true"
#define NC_RDF_NAME_SORT			"http://home.netscape.com/NC-rdf#Name?collation=true"
#define NC_RDF_NICKNAME_SORT		"http://home.netscape.com/NC-rdf#Nickname?collation=true"
#define NC_RDF_PRIMARYEMAIL_SORT	"http://home.netscape.com/NC-rdf#PrimaryEmail?collation=true"
#define NC_RDF_SECONDEMAIL_SORT		"http://home.netscape.com/NC-rdf#SecondEmail?collation=true"
#define NC_RDF_WORKPHONE_SORT		"http://home.netscape.com/NC-rdf#WorkPhone?collation=true"
#define NC_RDF_HOMEPHONE_SORT		"http://home.netscape.com/NC-rdf#HomePhone?collation=true"
#define NC_RDF_FAX_SORT				"http://home.netscape.com/NC-rdf#Fax?collation=true"
#define NC_RDF_PAGER_SORT			"http://home.netscape.com/NC-rdf#Pager?collation=true"
#define NC_RDF_CELLULAR_SORT		"http://home.netscape.com/NC-rdf#Cellular?collation=true"
#define NC_RDF_TITLE_SORT			"http://home.netscape.com/NC-rdf#Title?collation=true"
#define NC_RDF_DEPARTMENT_SORT		"http://home.netscape.com/NC-rdf#Department?collation=true"
#define NC_RDF_ORGANIZATION_SORT	"http://home.netscape.com/NC-rdf#Company?collation=true"

//Commands
#define NC_RDF_DELETE				"http://home.netscape.com/NC-rdf#Delete"
#define NC_RDF_NEWCADR				"http://home.netscape.com/NC-rdf#NewCard"

////////////////////////////////////////////////////////////////////////

nsAbCardDataSource::nsAbCardDataSource():
  mInitialized(PR_FALSE),
  mRDFService(nsnull)
{
}

nsAbCardDataSource::~nsAbCardDataSource (void)
{
	if (mRDFService)
	{
		mRDFService->UnregisterDataSource(this);
		nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService); 
		mRDFService = nsnull;
	}

	nsresult rv = NS_OK;
	nsCOMPtr<nsIAddrBookSession> abSession = 
	         do_GetService(kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
	abSession->RemoveAddressBookListener(this);

	nsrefcnt refcnt;

	if (kNC_DisplayName)
		NS_RELEASE2(kNC_DisplayName, refcnt);
	if (kNC_Name)
		NS_RELEASE2(kNC_Name, refcnt);
	if (kNC_Nickname)
		NS_RELEASE2(kNC_Nickname, refcnt);
	if (kNC_PrimaryEmail)
		NS_RELEASE2(kNC_PrimaryEmail, refcnt);
	if (kNC_SecondEmail)
		NS_RELEASE2(kNC_SecondEmail, refcnt);
	if (kNC_WorkPhone)
		NS_RELEASE2(kNC_WorkPhone, refcnt);
	if (kNC_HomePhone)
		NS_RELEASE2(kNC_HomePhone, refcnt);
	if (kNC_Fax)
		NS_RELEASE2(kNC_Fax, refcnt);
	if (kNC_Pager)
		NS_RELEASE2(kNC_Pager, refcnt);
	if (kNC_Cellular)
		NS_RELEASE2(kNC_Cellular, refcnt);
	if (kNC_Title)
		NS_RELEASE2(kNC_Title, refcnt);
	if (kNC_Department)
		NS_RELEASE2(kNC_Department, refcnt);
	if (kNC_Organization)
		NS_RELEASE2(kNC_Organization, refcnt);
	 
	if (kNC_DisplayNameCollation)
		NS_RELEASE2(kNC_DisplayNameCollation, refcnt);
	if (kNC_NameCollation)
		NS_RELEASE2(kNC_NameCollation, refcnt);
	if (kNC_NicknameCollation)
		NS_RELEASE2(kNC_NicknameCollation, refcnt);
	if (kNC_PrimaryEmailCollation)
		NS_RELEASE2(kNC_PrimaryEmailCollation, refcnt);
	if (kNC_SecondEmailCollation)
		NS_RELEASE2(kNC_SecondEmailCollation, refcnt);
	if (kNC_WorkPhoneCollation)
		NS_RELEASE2(kNC_WorkPhoneCollation, refcnt);
	if (kNC_HomePhoneCollation)
		NS_RELEASE2(kNC_HomePhoneCollation, refcnt);
	if (kNC_FaxCollation)
		NS_RELEASE2(kNC_FaxCollation, refcnt);
	if (kNC_PagerCollation)
		NS_RELEASE2(kNC_PagerCollation, refcnt);
	if (kNC_CellularCollation)
		NS_RELEASE2(kNC_CellularCollation, refcnt);
	if (kNC_TitleCollation)
		NS_RELEASE2(kNC_TitleCollation, refcnt);
	if (kNC_DepartmentCollation)
		NS_RELEASE2(kNC_DepartmentCollation, refcnt);
	if (kNC_CompanyCollation)
		NS_RELEASE2(kNC_CompanyCollation, refcnt);

	if (kNC_Delete)
		NS_RELEASE2(kNC_Delete, refcnt);
	if (kNC_NewCard)
		NS_RELEASE2(kNC_NewCard, refcnt);

}

nsresult nsAbCardDataSource::Init()
{
  if (mInitialized)
      return NS_ERROR_ALREADY_INITIALIZED;

  nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
                                             NS_GET_IID(nsIRDFService),
                                             (nsISupports**) &mRDFService); // XXX probably need shutdown listener here
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAddrBookSession> abSession = 
           do_GetService(kAddrBookSessionCID, &rv); 
  if (NS_SUCCEEDED(rv))
    abSession->AddAddressBookListener(this);

  mRDFService->RegisterDataSource(this, PR_FALSE);

  if (! kNC_DisplayName)
  {
    mRDFService->GetResource(NC_RDF_DISPLAYNAME, &kNC_DisplayName);
    mRDFService->GetResource(NC_RDF_NAME, &kNC_Name);
    mRDFService->GetResource(NC_RDF_NICKNAME,	&kNC_Nickname);
    mRDFService->GetResource(NC_RDF_PRIMARYEMAIL, &kNC_PrimaryEmail);
    mRDFService->GetResource(NC_RDF_SECONDEMAIL, &kNC_SecondEmail);
    mRDFService->GetResource(NC_RDF_WORKPHONE,	&kNC_WorkPhone);
    mRDFService->GetResource(NC_RDF_HOMEPHONE, &kNC_HomePhone);
    mRDFService->GetResource(NC_RDF_FAX, &kNC_Fax);
    mRDFService->GetResource(NC_RDF_PAGER,	&kNC_Pager);
    mRDFService->GetResource(NC_RDF_CELLULAR, &kNC_Cellular);
    mRDFService->GetResource(NC_RDF_TITLE, &kNC_Title);
    mRDFService->GetResource(NC_RDF_DEPARTMENT,	&kNC_Department);
    mRDFService->GetResource(NC_RDF_ORGANIZATION,	&kNC_Organization);

    mRDFService->GetResource(NC_RDF_DISPLAYNAME_SORT, &kNC_DisplayNameCollation);
    mRDFService->GetResource(NC_RDF_NAME_SORT, &kNC_NameCollation);
    mRDFService->GetResource(NC_RDF_NICKNAME_SORT, &kNC_NicknameCollation);
    mRDFService->GetResource(NC_RDF_PRIMARYEMAIL_SORT, &kNC_PrimaryEmailCollation);
    mRDFService->GetResource(NC_RDF_SECONDEMAIL_SORT, &kNC_SecondEmailCollation);
    mRDFService->GetResource(NC_RDF_WORKPHONE_SORT, &kNC_WorkPhoneCollation);
    mRDFService->GetResource(NC_RDF_HOMEPHONE_SORT, &kNC_HomePhoneCollation);
    mRDFService->GetResource(NC_RDF_FAX_SORT, &kNC_FaxCollation);
    mRDFService->GetResource(NC_RDF_PAGER_SORT, &kNC_PagerCollation);
    mRDFService->GetResource(NC_RDF_CELLULAR_SORT, &kNC_CellularCollation);
    mRDFService->GetResource(NC_RDF_TITLE_SORT, &kNC_TitleCollation);
    mRDFService->GetResource(NC_RDF_DEPARTMENT_SORT, &kNC_DepartmentCollation);
    mRDFService->GetResource(NC_RDF_ORGANIZATION_SORT, &kNC_CompanyCollation);

    mRDFService->GetResource(NC_RDF_DELETE, &kNC_Delete);
    mRDFService->GetResource(NC_RDF_NEWCADR, &kNC_NewCard);
  }
  mInitialized = PR_TRUE;
  return NS_OK;
}



NS_IMPL_ADDREF_INHERITED(nsAbCardDataSource, nsAbRDFDataSource)
NS_IMPL_RELEASE_INHERITED(nsAbCardDataSource, nsAbRDFDataSource)

NS_IMETHODIMP nsAbCardDataSource::QueryInterface(REFNSIID iid, void** result)
{
  if (! result)
    return NS_ERROR_NULL_POINTER;

	*result = nsnull;
	if(iid.Equals(NS_GET_IID(nsIAbListener)))
	{
		*result = NS_STATIC_CAST(nsIAbListener*, this);
		NS_ADDREF(this);
		return NS_OK;
	}
	else
		return nsAbRDFDataSource::QueryInterface(iid, result);
}

 // nsIRDFDataSource methods
NS_IMETHODIMP nsAbCardDataSource::GetURI(char* *uri)
{
  if ((*uri = nsCRT::strdup("rdf:addresscard")) == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  else
    return NS_OK;
}

NS_IMETHODIMP nsAbCardDataSource::GetTarget(nsIRDFResource* source,
                                          nsIRDFResource* property,
                                          PRBool tv,
                                          nsIRDFNode** target)
{
  nsresult rv = NS_RDF_NO_VALUE;

  // we only have positive assertions in the mail data source.
  if (! tv)
    return NS_RDF_NO_VALUE;

  nsCOMPtr<nsIAbCard> card(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv) && card) {
    rv = createCardNode(card, property, target);
  }
  else
	  return NS_RDF_NO_VALUE;
  return rv;
}


NS_IMETHODIMP nsAbCardDataSource::GetTargets(nsIRDFResource* source,
                                                nsIRDFResource* property,    
                                                PRBool tv,
                                                nsISimpleEnumerator** targets)
{
  nsresult rv = NS_RDF_NO_VALUE;

  nsCOMPtr<nsIAbCard> card(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv) && card)
  { 
	if((kNC_DisplayName == property) || (kNC_Name == property) || 
	   (kNC_Nickname == property) || (kNC_PrimaryEmail == property) || 
	   (kNC_SecondEmail == property) || (kNC_WorkPhone == property) || 
	   (kNC_HomePhone == property) || (kNC_Fax == property) || 
	   (kNC_Pager == property) || (kNC_Cellular == property) || 
	   (kNC_Title == property) || (kNC_Department == property) || 
	   (kNC_Organization == property)) 
    { 
      nsSingletonEnumerator* cursor =
        new nsSingletonEnumerator(property);
      if (cursor == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(cursor);
      *targets = cursor;
	  return NS_OK;
    }
  }
  else {
	  //create empty cursor
	  nsCOMPtr<nsISupportsArray> assertions;
      NS_NewISupportsArray(getter_AddRefs(assertions));
	  nsArrayEnumerator* cursor = 
		  new nsArrayEnumerator(assertions);
	  if(cursor == nsnull)
		  return NS_ERROR_OUT_OF_MEMORY;
	  NS_ADDREF(cursor);
	  *targets = cursor;
	  return NS_OK;
  }

  return NS_NewEmptyEnumerator(targets);
}

NS_IMETHODIMP nsAbCardDataSource::HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool* hasAssertion)
{
	nsresult rv;
	nsCOMPtr<nsIAbCard> card(do_QueryInterface(source, &rv));
	if(NS_SUCCEEDED(rv))
		return DoCardHasAssertion(card, property, target, tv, hasAssertion);
	else
		*hasAssertion = PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP 
nsAbCardDataSource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
  nsresult rv;
  nsCOMPtr<nsIAbCard> card(do_QueryInterface(aSource, &rv));
  if (NS_SUCCEEDED(rv) && card) {
    *result = (aArc == kNC_DisplayName ||
               aArc == kNC_Name ||
               aArc == kNC_Nickname ||
               aArc == kNC_PrimaryEmail ||
               aArc == kNC_SecondEmail ||
               aArc == kNC_WorkPhone ||
               aArc == kNC_HomePhone ||
               aArc == kNC_Fax ||
               aArc == kNC_Pager ||
               aArc == kNC_Cellular ||
               aArc == kNC_Title ||
               aArc == kNC_Department ||
               aArc == kNC_Organization);
  }
  else {
    *result = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP nsAbCardDataSource::ArcLabelsOut(nsIRDFResource* source,
                                                 nsISimpleEnumerator** labels)
{
  nsCOMPtr<nsISupportsArray> arcs;
  nsresult rv = NS_RDF_NO_VALUE;

  nsCOMPtr<nsIAbCard> card(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv) && card) {
    // fflush(stdout);  // huh?
    rv = getCardArcLabelsOut(card, getter_AddRefs(arcs));
  }
  else {
    // how to return an empty cursor?
    // for now return a 0-length nsISupportsArray
    NS_NewISupportsArray(getter_AddRefs(arcs));
  }

  nsArrayEnumerator* cursor =
  new nsArrayEnumerator(arcs);

  if (cursor == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(cursor);
  *labels = cursor;
  return NS_OK;
}

nsresult
nsAbCardDataSource::getCardArcLabelsOut(nsIAbCard *card,
                                      nsISupportsArray **arcs)
{
	nsresult rv;
	rv = NS_NewISupportsArray(arcs);
	NS_ENSURE_SUCCESS(rv, rv);

	(*arcs)->AppendElement(kNC_DisplayName);
	(*arcs)->AppendElement(kNC_Name);
	(*arcs)->AppendElement(kNC_Nickname);
	(*arcs)->AppendElement(kNC_PrimaryEmail);
	(*arcs)->AppendElement(kNC_SecondEmail);
	(*arcs)->AppendElement(kNC_WorkPhone);
	(*arcs)->AppendElement(kNC_HomePhone);
	(*arcs)->AppendElement(kNC_Fax);
	(*arcs)->AppendElement(kNC_Pager);
	(*arcs)->AppendElement(kNC_Cellular);
	(*arcs)->AppendElement(kNC_Title);
	(*arcs)->AppendElement(kNC_Department);
	(*arcs)->AppendElement(kNC_Organization);
	return NS_OK;
}

NS_IMETHODIMP
nsAbCardDataSource::GetAllCommands(nsIRDFResource* source,
                                      nsIEnumerator/*<nsIRDFResource>*/** commands)
{
  nsresult rv;
  nsCOMPtr<nsISupportsArray> cmds;

  nsCOMPtr<nsIAbCard> card(do_QueryInterface(source, &rv));
  if (NS_SUCCEEDED(rv)) {
    rv = NS_NewISupportsArray(getter_AddRefs(cmds));
    NS_ENSURE_SUCCESS(rv, rv);
    cmds->AppendElement(kNC_Delete);
    cmds->AppendElement(kNC_NewCard);
  }

  if (cmds != nsnull)
    return cmds->Enumerate(commands);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsAbCardDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                        nsIRDFResource*   aCommand,
                                        nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                        PRBool* aResult)
{
  nsresult rv;
  nsCOMPtr<nsIAbCard> card;

  PRUint32 i, cnt;
  rv = aSources->Count(&cnt);
  for (i = 0; i < cnt; i++) {
    nsCOMPtr<nsISupports> source = getter_AddRefs(aSources->ElementAt(i));
		card = do_QueryInterface(source, &rv);
    if (NS_SUCCEEDED(rv)) {
      // we don't care about the arguments -- card commands are always enabled
      if (!((aCommand == kNC_Delete) ||
		    (aCommand == kNC_NewCard))) {
        *aResult = PR_FALSE;
        return NS_OK;
      }
    }
  }
  *aResult = PR_TRUE;
  return NS_OK; // succeeded for all sources
}

NS_IMETHODIMP
nsAbCardDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                 nsIRDFResource*   aCommand,
                                 nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
	nsresult rv = NS_OK;

	// XXX need to handle batching of command applied to all sources

	PRUint32 i, cnt;
	rv = aSources->Count(&cnt);
	for (i = 0; i < cnt; i++) 
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(aSources->ElementAt(i));
		nsCOMPtr<nsIAbCard> card = do_QueryInterface(supports, &rv);
		if (NS_SUCCEEDED(rv)) 
		{
			if ((aCommand == kNC_Delete))  
				rv = DoDeleteFromCard(card, aArguments);
			else if((aCommand == kNC_NewCard)) 
				rv = DoNewCard(card, aArguments);
		}
	}
	//for the moment return NS_OK, because failure stops entire DoCommand process.
	return NS_OK;
}

NS_IMETHODIMP nsAbCardDataSource::OnItemAdded(nsISupports *parentDirectory, nsISupports *item)
{
  return NS_OK;
}

NS_IMETHODIMP nsAbCardDataSource::OnItemRemoved(nsISupports *parentDirectory, nsISupports *item)
{
  return NS_OK;
}

NS_IMETHODIMP nsAbCardDataSource::OnItemPropertyChanged(nsISupports *item, const char *property,
													const PRUnichar *oldValue, const PRUnichar *newValue)

{
	
	nsresult rv;
	nsCOMPtr<nsIRDFResource> resource(do_QueryInterface(item, &rv));

	if(NS_SUCCEEDED(rv))
	{
		if(PL_strcmp("DisplayName", property) == 0)
		{
			NotifyPropertyChanged(resource, kNC_DisplayName, oldValue, newValue);
		}
		if(PL_strcmp("Name", property) == 0)
		{
			NotifyPropertyChanged(resource, kNC_Name, oldValue, newValue);
		}
		if(PL_strcmp("NickName", property) == 0)
		{
			NotifyPropertyChanged(resource, kNC_Nickname, oldValue, newValue);
		}
		if(PL_strcmp("PrimaryEmail", property) == 0)
		{
			NotifyPropertyChanged(resource, kNC_PrimaryEmail, oldValue, newValue);
		}
		if(PL_strcmp("SecondEmail", property) == 0)
		{
			NotifyPropertyChanged(resource, kNC_SecondEmail, oldValue, newValue);
		}
		if(PL_strcmp("WorkPhone", property) == 0)
		{
			NotifyPropertyChanged(resource, kNC_WorkPhone, oldValue, newValue);
		}
		if(PL_strcmp("HomePhone", property) == 0)
		{
			NotifyPropertyChanged(resource, kNC_HomePhone, oldValue, newValue);
		}
		if(PL_strcmp("FaxNumber", property) == 0)
		{
			NotifyPropertyChanged(resource, kNC_Fax, oldValue, newValue);
		}
		if(PL_strcmp("PagerNumber", property) == 0)
		{
			NotifyPropertyChanged(resource, kNC_Pager, oldValue, newValue);
		}
		if(PL_strcmp("CellularNumber", property) == 0)
		{
			NotifyPropertyChanged(resource, kNC_Cellular, oldValue, newValue);
		}
		if(PL_strcmp("JobTitle", property) == 0)
		{
			NotifyPropertyChanged(resource, kNC_Title, oldValue, newValue);
		}
		if(PL_strcmp("Department", property) == 0)
		{
			NotifyPropertyChanged(resource, kNC_Department, oldValue, newValue);
		}
		if(PL_strcmp("Company", property) == 0)
		{
			NotifyPropertyChanged(resource, kNC_Organization, oldValue, newValue);
		}
	}
	return NS_OK;
}

nsresult nsAbCardDataSource::createCardNode(nsIAbCard* card,
                                          nsIRDFResource* property,
                                          nsIRDFNode** target)
{
	PRUnichar *name = nsnull;
	nsresult rv = NS_RDF_NO_VALUE;

	if ((kNC_DisplayName == property))
		rv = card->GetDisplayName(&name);
	else if ((kNC_Name== property))
		rv = card->GetName(&name);
	else if ((kNC_Nickname == property))
		rv = card->GetNickName(&name);
	else if ((kNC_PrimaryEmail == property))
		rv = card->GetPrimaryEmail(&name);
	else if ((kNC_SecondEmail == property))
		rv = card->GetSecondEmail(&name);
	else if ((kNC_WorkPhone == property))
		rv = card->GetWorkPhone(&name);
	else if ((kNC_HomePhone == property))
		rv = card->GetHomePhone(&name);
	else if ((kNC_Fax == property))
		rv = card->GetFaxNumber(&name);
	else if ((kNC_Pager == property))
		rv = card->GetPagerNumber(&name);
	else if ((kNC_Cellular == property))
		rv = card->GetCellularNumber(&name);
	else if ((kNC_Title == property))
		rv = card->GetJobTitle(&name);
	else if ((kNC_Department == property))
		rv = card->GetDepartment(&name);
	else if ((kNC_Organization == property))
		rv = card->GetCompany(&name);
	else
	{
		PRUnichar *tempStr = nsnull;
		if (kNC_DisplayNameCollation == property)
			rv = card->GetDisplayName(&tempStr);
		else if (kNC_NameCollation == property)
			rv = card->GetName(&tempStr);
		else if (kNC_NicknameCollation == property)
			rv = card->GetNickName(&tempStr);
		else if (kNC_PrimaryEmailCollation == property)
			rv = card->GetPrimaryEmail(&tempStr);
		else if (kNC_SecondEmailCollation == property)
			rv = card->GetSecondEmail(&tempStr);
		else if (kNC_WorkPhoneCollation == property)
			rv = card->GetWorkPhone(&tempStr);
		else if (kNC_HomePhoneCollation == property)
			rv = card->GetHomePhone(&tempStr);
		else if (kNC_FaxCollation == property)
			rv = card->GetFaxNumber(&tempStr);
		else if (kNC_PagerCollation == property)
			rv = card->GetPagerNumber(&tempStr);
		else if (kNC_CellularCollation == property)
			rv = card->GetCellularNumber(&tempStr);
		else if (kNC_TitleCollation == property)
			rv = card->GetJobTitle(&tempStr);
		else if (kNC_DepartmentCollation == property)
			rv = card->GetDepartment(&tempStr);
		else if (kNC_CompanyCollation == property)
			rv = card->GetCompany(&tempStr);
		if (tempStr)
		{
			rv = card->GetCollationKey(tempStr, &name);
			nsCRT::free(tempStr);
		}
	}
	NS_ENSURE_SUCCESS(rv, rv);
	if (name)
	{
		nsString nameString(name);
		createNode(nameString, target);
		nsCRT::free(name);
	}
	return rv;
}

nsresult nsAbCardDataSource::DoDeleteFromCard(nsIAbCard *card, nsISupportsArray *arguments)
{
	nsresult rv = NS_OK;
	return rv;
}

nsresult nsAbCardDataSource::DoNewCard(nsIAbCard *card, nsISupportsArray *arguments)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsISupports> elem = getter_AddRefs(arguments->ElementAt(0)); 
	nsCOMPtr<nsIRDFLiteral> literal(do_QueryInterface(elem, &rv));
	if(NS_SUCCEEDED(rv))
	{
		PRUnichar *name;
		literal->GetValue(&name);
		nsString tempStr(name);
		nsMemory::Free(name);
		nsCAutoString nameStr; nameStr.AssignWithConversion(tempStr);

//		rv = card->CreateNewCard(nameStr);
	}
	return rv;
}

nsresult nsAbCardDataSource::DoCardHasAssertion(nsIAbCard *card, nsIRDFResource *property, nsIRDFNode *target,
													 PRBool tv, PRBool *hasAssertion)
{
	*hasAssertion = PR_TRUE;

	return NS_OK;

}

nsresult NS_NewAbCardDataSource(const nsIID& iid, void **result)
{
    NS_PRECONDITION(result != nsnull, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    nsAbCardDataSource* datasource = new nsAbCardDataSource();
    if (! datasource)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    rv = datasource->Init();
    if (NS_FAILED(rv)) {
        delete datasource;
        return rv;
    }

	return datasource->QueryInterface(iid, result);
}



