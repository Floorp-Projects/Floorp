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
 * The Original Code is Java XPCOM Bindings.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Javier Pedemonte (jhpedemonte@gmail.com)
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

#include "TestParams.h"
#include "nsIGenericFactory.h"
#include "nsMemory.h"
#include "prmem.h"
#include "nsString.h"
#include "nsWeakReference.h"
#include "nsILocalFile.h"
#include "stdio.h"


/* TestParams */

NS_IMPL_ISUPPORTS1(TestParams, ITestParams)

TestParams::TestParams()
{
  /* member initializers and constructor code */
}

TestParams::~TestParams()
{
  /* destructor code */
}

NS_IMETHODIMP
TestParams::TestSimpleTypeArrayIn(PRUint32 aCount, PRUint8* aArray)
{
  printf("-> TestSimpleTypeArrayIn()\n");

  for (PRUint32 i = 0; i < aCount; i++) {
    printf("[%d]  %u\n", i, aArray[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
TestParams::TestCharStrTypeArrayIn(PRUint32 aCount, const char** aArray)
{
  printf("-> TestCharStrTypeArrayIn()\n");

  for (PRUint32 i = 0; i < aCount; i++) {
    printf("[%d]  %s\n", i, aArray[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
TestParams::TestWCharStrTypeArrayIn(PRUint32 aCount, const PRUnichar** aArray)
{
  printf("-> TestWCharStrTypeArrayIn()\n");

  for (PRUint32 i = 0; i < aCount; i++) {
    nsAutoString tmp(aArray[i]);
    printf("[%d]  %s\n", i, NS_LossyConvertUCS2toASCII(tmp).get());
  }
  return NS_OK;
}

NS_IMETHODIMP
TestParams::TestIIDTypeArrayIn(PRUint32 aCount, const nsIID** aArray)
{
  printf("-> TestIIDTypeArrayIn()\n");

  for (PRUint32 i = 0; i < aCount; i++) {
    char* iid = aArray[i]->ToString();
    printf("[%d]  %s\n", i, iid);
    PR_Free(iid);
  }
  return NS_OK;
}

NS_IMETHODIMP
TestParams::TestIfaceTypeArrayIn(PRUint32 aCount, nsILocalFile** aArray)
{
  printf("-> TestIfaceTypeArrayIn()\n");

  for (PRUint32 i = 0; i < aCount; i++) {
    nsAutoString path;
    aArray[i]->GetPath(path);
    printf("[%d]  %s\n", i, NS_LossyConvertUCS2toASCII(path).get());
  }
  return NS_OK;
}

NS_IMETHODIMP
TestParams::TestSimpleTypeArrayOut(PRUint32* aCount, char** aResult)
{
  printf("-> TestSimpleTypeArrayOut()\n");

  PRUint32 count = 4;
  char* array = (char*) nsMemory::Alloc(count * sizeof(char));
  array[0] = 's';
  array[1] = 't';
  array[2] = 'u';
  array[3] = 'v';

  *aCount = count;
  *aResult = array;
  return NS_OK;
}

NS_IMETHODIMP
TestParams::TestCharStrTypeArrayOut(PRUint32* aCount, char*** aResult)
{
  printf("-> TestCharStrTypeArrayOut()\n");

  PRUint32 count = 3;
  char** array = (char**) nsMemory::Alloc(count * sizeof(char*));

  array[0] = (char*) nsMemory::Alloc(4 * sizeof(char));
  strcpy(array[0], "one");
  array[1] = (char*) nsMemory::Alloc(4 * sizeof(char));
  strcpy(array[1], "two");
  array[2] = (char*) nsMemory::Alloc(6 * sizeof(char));
  strcpy(array[2], "three");

  *aCount = count;
  *aResult = array;
  return NS_OK;
}

NS_IMETHODIMP
TestParams::TestWCharStrTypeArrayOut(PRUint32* aCount, PRUnichar*** aResult)
{
  printf("-> TestWCharStrTypeArrayOut()\n");

  PRUint32 count = 3;
  PRUnichar** array = (PRUnichar**) nsMemory::Alloc(count * sizeof(PRUnichar*));

  NS_NAMED_LITERAL_STRING(one, "ône");
  NS_NAMED_LITERAL_STRING(two, "twò");
  NS_NAMED_LITERAL_STRING(three, "threë");

  array[0] = (PRUnichar*) nsMemory::Alloc(4 * sizeof(PRUnichar));
  memcpy(array[0], one.get(), 4 * sizeof(PRUnichar));
  array[1] = (PRUnichar*) nsMemory::Alloc(4 * sizeof(PRUnichar));
  memcpy(array[1], two.get(), 4 * sizeof(PRUnichar));
  array[2] = (PRUnichar*) nsMemory::Alloc(6 * sizeof(PRUnichar));
  memcpy(array[2], three.get(), 6 * sizeof(PRUnichar));

  *aCount = count;
  *aResult = array;
  return NS_OK;
}

NS_IMETHODIMP
TestParams::TestIIDTypeArrayOut(PRUint32* aCount, nsIID*** aResult)
{
  printf("-> TestIIDTypeArrayOut()\n");

  PRUint32 count = 2;
  nsIID** array = (nsIID**) nsMemory::Alloc(count * sizeof(nsIID*));

  const nsIID& iid = NS_GET_IID(nsISupports);
  array[0] = (nsIID*) nsMemory::Clone(&iid, sizeof(iid));
  const nsIID& iid2 = NS_GET_IID(nsISupportsWeakReference);
  array[1] = (nsIID*) nsMemory::Clone(&iid2, sizeof(iid2));

  *aCount = count;
  *aResult = array;
  return NS_OK;
}

NS_IMETHODIMP
TestParams::TestIfaceTypeArrayOut(PRUint32* aCount, nsILocalFile*** aResult)
{
  printf("-> TestIfaceTypeArrayOut()\n");

  PRUint32 count = 3;
  nsILocalFile** array = (nsILocalFile**)
                                 nsMemory::Alloc(count * sizeof(nsILocalFile*));

  nsILocalFile* dir1;
  NS_NewLocalFile(NS_LITERAL_STRING("/usr/local/share"), PR_FALSE, &dir1);
  array[0] = dir1;
  nsILocalFile* dir2;
  NS_NewLocalFile(NS_LITERAL_STRING("/home"), PR_FALSE, &dir2);
  array[1] = dir2;
  nsILocalFile* dir3;
  NS_NewLocalFile(NS_LITERAL_STRING("/var/log"), PR_FALSE, &dir3);
  array[2] = dir3;

  *aCount = count;
  *aResult = array;
  return NS_OK;
}

NS_IMETHODIMP
TestParams::TestSimpleTypeArrayInOut(PRUint32 aCount, PRInt16** aArray)
{
  printf("-> TestSimpleTypeArrayInOut()\n");

  printf("in:\n");
  PRUint32 i;
  for (i = 0; i < aCount; i++) {
    printf("[%d]  %d\n", i, aArray[0][i]);
  }

  for (i = 0; i < aCount/2; i++) {
    PRUint32 index = aCount - 1 - i;
    PRUint16 temp = aArray[0][index];
    aArray[0][index] = aArray[0][i];
    aArray[0][i] = temp;
  }

  return NS_OK;
}

NS_IMETHODIMP
TestParams::TestCharStrTypeArrayInOut(PRUint32 aCount, char*** aArray)
{
  printf("-> TestCharStrTypeArrayInOut()\n");

  printf("in:\n");
  PRUint32 i;
  for (i = 0; i < aCount; i++) {
    printf("[%d]  %s\n", i, aArray[0][i]);
  }

  for (i = 0; i < aCount/2; i++) {
    PRUint32 index = aCount - 1 - i;
    char* temp = aArray[0][index];
    aArray[0][index] = aArray[0][i];
    aArray[0][i] = temp;
  }

  return NS_OK;
}

NS_IMETHODIMP
TestParams::TestWCharStrTypeArrayInOut(PRUint32 aCount, PRUnichar*** aArray)
{
  printf("-> TestWCharStrTypeArrayInOut()\n");

  printf("in:\n");
  PRUint32 i;
  for (i = 0; i < aCount; i++) {
    nsAutoString tmp(aArray[0][i]);
    printf("[%d]  %s\n", i, NS_LossyConvertUCS2toASCII(tmp).get());
  }

  for (i = 0; i < aCount/2; i++) {
    PRUint32 index = aCount - 1 - i;
    PRUnichar* temp = aArray[0][index];
    aArray[0][index] = aArray[0][i];
    aArray[0][i] = temp;
  }

  return NS_OK;
}

NS_IMETHODIMP
TestParams::TestIIDTypeArrayInOut(PRUint32 aCount, nsIID*** aArray)
{
  printf("-> TestIIDTypeArrayInOut()\n");

  printf("in:\n");
  PRUint32 i;
  for (i = 0; i < aCount; i++) {
    char* iid = aArray[0][i]->ToString();
    printf("[%d]  %s\n", i, iid);
    PR_Free(iid);
  }

  for (i = 0; i < aCount/2; i++) {
    PRUint32 index = aCount - 1 - i;
    nsID* temp = aArray[0][index];
    aArray[0][index] = aArray[0][i];
    aArray[0][i] = temp;
  }

  return NS_OK;
}

NS_IMETHODIMP
TestParams::TestIfaceTypeArrayInOut(PRUint32 aCount, nsILocalFile*** aArray)
{
  printf("-> TestIfaceTypeArrayInOut()\n");

  printf("in:\n");
  PRUint32 i;
  for (i = 0; i < aCount; i++) {
    nsAutoString path;
    aArray[0][i]->GetPath(path);
    printf("[%d]  %s\n", i, NS_LossyConvertUCS2toASCII(path).get());
  }

  for (i = 0; i < aCount/2; i++) {
    PRUint32 index = aCount - 1 - i;
    nsILocalFile* temp = aArray[0][index];
    aArray[0][index] = aArray[0][i];
    aArray[0][i] = temp;
  }

  return NS_OK;
}

NS_IMETHODIMP
TestParams::ReturnSimpleTypeArray(PRUint32* aCount, PRUint32** aResult)
{
  printf("-> ReturnSimpleArrayType()\n");

  PRUint32 count = 5;
  PRUint32* array = (PRUint32*) nsMemory::Alloc(count * sizeof(PRUint32));
  for (PRUint32 i = 0, j = 1; i < count; i++ ) {
    array[i] = j;
    j *= 13;
  }

  *aCount = count;
  *aResult = array;
  return NS_OK;
}

NS_IMETHODIMP
TestParams::ReturnCharStrTypeArray(PRUint32* aCount, char*** aResult)
{
  printf("-> ReturnCharStrTypeArray()\n");

  PRUint32 count = 3;
  char** array = (char**) nsMemory::Alloc(count * sizeof(char*));

  array[0] = (char*) nsMemory::Alloc(4 * sizeof(char));
  strcpy(array[0], "one");
  array[1] = (char*) nsMemory::Alloc(4 * sizeof(char));
  strcpy(array[1], "two");
  array[2] = (char*) nsMemory::Alloc(6 * sizeof(char));
  strcpy(array[2], "three");

  *aCount = count;
  *aResult = array;
  return NS_OK;
}

NS_IMETHODIMP
TestParams::ReturnWCharStrTypeArray(PRUint32* aCount, PRUnichar*** aResult)
{
  printf("-> ReturnWCharStrTypeArray()\n");

  PRUint32 count = 3;
  PRUnichar** array = (PRUnichar**) nsMemory::Alloc(count * sizeof(PRUnichar*));

  NS_NAMED_LITERAL_STRING(one, "one");
  NS_NAMED_LITERAL_STRING(two, "two");
  NS_NAMED_LITERAL_STRING(three, "three");

  array[0] = (PRUnichar*) nsMemory::Alloc(4 * sizeof(PRUnichar));
  memcpy(array[0], one.get(), 4 * sizeof(PRUnichar));
  array[1] = (PRUnichar*) nsMemory::Alloc(4 * sizeof(PRUnichar));
  memcpy(array[1], two.get(), 4 * sizeof(PRUnichar));
  array[2] = (PRUnichar*) nsMemory::Alloc(6 * sizeof(PRUnichar));
  memcpy(array[2], three.get(), 6 * sizeof(PRUnichar));

  *aCount = count;
  *aResult = array;
  return NS_OK;
}

NS_IMETHODIMP
TestParams::ReturnIIDTypeArray(PRUint32* aCount, nsIID*** aResult)
{
  printf("-> ReturnIIDTypeArray()\n");

  PRUint32 count = 2;
  nsIID** array = (nsIID**) nsMemory::Alloc(count * sizeof(nsIID*));

  const nsIID& iid = NS_GET_IID(nsISupports);
  array[0] = (nsIID*) nsMemory::Clone(&iid, sizeof(iid));
  const nsIID& iid2 = NS_GET_IID(nsISupportsWeakReference);
  array[1] = (nsIID*) nsMemory::Clone(&iid2, sizeof(iid2));

  *aCount = count;
  *aResult = array;
  return NS_OK;
}

NS_IMETHODIMP
TestParams::ReturnIfaceTypeArray(PRUint32* aCount, nsILocalFile*** aResult)
{
  printf("-> ReturnIfaceTypeArray()\n");

  PRUint32 count = 3;
  nsILocalFile** array = (nsILocalFile**)
                                 nsMemory::Alloc(count * sizeof(nsILocalFile*));

  nsILocalFile* dir1;
  NS_NewLocalFile(NS_LITERAL_STRING("/usr/local/share"), PR_FALSE, &dir1);
  array[0] = dir1;
  nsILocalFile* dir2;
  NS_NewLocalFile(NS_LITERAL_STRING("/home"), PR_FALSE, &dir2);
  array[1] = dir2;
  nsILocalFile* dir3;
  NS_NewLocalFile(NS_LITERAL_STRING("/var/log"), PR_FALSE, &dir3);
  array[2] = dir3;

  *aCount = count;
  *aResult = array;
  return NS_OK;
}


/* TestParamsFactory */

NS_GENERIC_FACTORY_CONSTRUCTOR(TestParams)

static const nsModuleComponentInfo components[] = {
  { "TestParams", TESTPARAMS_CID, nsnull, TestParamsConstructor }
};

NS_IMPL_NSGETMODULE(TestParamsFactory, components)


