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
#ifndef _nsCardDataSource_H_
#define _nsCardDataSource_H_

#include "nsAbRDFDataSource.h"
#include "nsIRDFService.h"
#include "nsIAbListener.h"
#include "nsIAbCard.h"
#include "nsISupportsArray.h"
#include "nsString.h"


/**
 * The addressbook person card data source.
 */
class nsAbCardDataSource : public nsAbRDFDataSource,
						   public nsIAbListener
{
private:

	PRBool		mInitialized;

	// The cached service managers
	nsIRDFService* mRDFService;
  
public:
  
	NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIABLISTENER
	nsAbCardDataSource(void);
	virtual ~nsAbCardDataSource (void);
	virtual nsresult Init();

	// nsIRDFDataSource methods

	NS_IMETHOD GetURI(char* *uri);

	NS_IMETHOD GetTarget(nsIRDFResource* source,
					   nsIRDFResource* property,
					   PRBool tv,
					   nsIRDFNode** target);

	NS_IMETHOD GetTargets(nsIRDFResource* source,
						nsIRDFResource* property,    
						PRBool tv,
						nsISimpleEnumerator** targets);

	NS_IMETHOD HasAssertion(nsIRDFResource* source,
						  nsIRDFResource* property,
						  nsIRDFNode* target,
						  PRBool tv,
						  PRBool* hasAssertion);

	NS_IMETHOD HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result);

	NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
						  nsISimpleEnumerator** labels); 

	NS_IMETHOD GetAllCommands(nsIRDFResource* source,
							nsIEnumerator/*<nsIRDFResource>*/** commands);
	NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
							  nsIRDFResource*   aCommand,
							  nsISupportsArray/*<nsIRDFResource>*/* aArguments,
							  PRBool* aResult);

	NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
					   nsIRDFResource*   aCommand,
					   nsISupportsArray/*<nsIRDFResource>*/* aArguments);

  // caching frequently used resources
protected:

	nsresult createCardNode(nsIAbCard* directory, nsIRDFResource* property,
                            nsIRDFNode** target);
	static nsresult getCardArcLabelsOut(nsIAbCard *directory,
										nsISupportsArray **arcs);

	nsresult DoDeleteFromCard(nsIAbCard *directory,
							  nsISupportsArray *arguments);

	nsresult DoNewCard(nsIAbCard *directory, nsISupportsArray *arguments);
	nsresult DoCardHasAssertion(nsIAbCard *card, nsIRDFResource *property, 
								nsIRDFNode *target, PRBool tv, PRBool *hasAssertion);

	static nsIRDFResource* kNC_CardChild;
	static nsIRDFResource* kNC_DisplayName;
	static nsIRDFResource* kNC_Name;
	static nsIRDFResource* kNC_Nickname;
	static nsIRDFResource* kNC_PrimaryEmail;
	static nsIRDFResource* kNC_SecondEmail;
	static nsIRDFResource* kNC_WorkPhone;
	static nsIRDFResource* kNC_HomePhone;
	static nsIRDFResource* kNC_Fax;
	static nsIRDFResource* kNC_Pager;
	static nsIRDFResource* kNC_Cellular;
	static nsIRDFResource* kNC_Title;
	static nsIRDFResource* kNC_Department;
	static nsIRDFResource* kNC_Organization;

	static nsIRDFResource* kNC_DisplayNameCollation;
	static nsIRDFResource* kNC_NameCollation;
	static nsIRDFResource* kNC_NicknameCollation;
	static nsIRDFResource* kNC_PrimaryEmailCollation;
	static nsIRDFResource* kNC_SecondEmailCollation;
	static nsIRDFResource* kNC_WorkPhoneCollation;
	static nsIRDFResource* kNC_HomePhoneCollation;
	static nsIRDFResource* kNC_FaxCollation;
	static nsIRDFResource* kNC_PagerCollation;
	static nsIRDFResource* kNC_CellularCollation;
	static nsIRDFResource* kNC_TitleCollation;
	static nsIRDFResource* kNC_DepartmentCollation;
	static nsIRDFResource* kNC_CompanyCollation;

	// commands
	static nsIRDFResource* kNC_Delete;
	static nsIRDFResource* kNC_NewCard;

};

nsresult NS_NewAbCardDataSource(const nsIID& iid, void **result);


#endif
