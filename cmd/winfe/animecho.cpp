/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */


#include "stdafx.h"

#include "animecho.h"

#include "wfedde.h"

//  Static property.
CPtrList CDDEAnimationEcho::m_Registry;

void CDDEAnimationEcho::Echo(DWORD dwWindowID, DWORD dwState)  
{
  //  Go through the registry.
  CDDEAnimationEcho *pItem;
  POSITION rIndex = m_Registry.GetHeadPosition();
  while(rIndex != NULL) 
  {
    pItem = (CDDEAnimationEcho*)m_Registry.GetNext(rIndex);
    
    pItem->EchoAnimation(dwWindowID, dwState);
  }
}

void CDDEAnimationEcho::DDERegister(CString &csServiceName)  
{
  //  Just create the item.  Our job's done.
  CDDEAnimationEcho *pDontCare = new CDDEAnimationEcho(csServiceName);
}

BOOL CDDEAnimationEcho::DDEUnRegister(CString &csServiceName)  
{
  //  Go through all registered items.
  POSITION rIndex = m_Registry.GetHeadPosition();
  CDDEAnimationEcho *pDelMe;
  
  while (rIndex != NULL) 
  {
    pDelMe = (CDDEAnimationEcho*)m_Registry.GetNext(rIndex);
    
    if(pDelMe->GetServiceName() == csServiceName)  
	{
       delete pDelMe;
       return TRUE;
    }
  }

  return FALSE;
}

void CDDEAnimationEcho::EchoAnimation(DWORD dwWindowID, DWORD dwState)  
{
  CDDEWrapper::AnimationEcho(this, dwWindowID, dwState);
}
