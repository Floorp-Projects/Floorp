/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsIWidget.h"

extern nsresult CoverageTest(int * argc, char **argv);

#ifdef XP_PC

#include <windows.h>

void main(int argc, char **argv)
{
  int argC = argc;

  CoverageTest(&argC, argv);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, 
    int nCmdShow)
{
  int  argC = 0;
  char ** argv = NULL;

  return(CoverageTest(&argC, argv));
}

#endif

#if defined(XP_UNIX) || defined(XP_BEOS)
int main(int argc, char **argv)
{
  int argC = argc;

  CoverageTest(&argC, argv);

  return 0;
}
#endif

#ifdef XP_MAC
int main(int argc, char **argv)
{
  int argC = argc;

  CoverageTest(&argC, argv);
	
	return 0;
}
#endif

